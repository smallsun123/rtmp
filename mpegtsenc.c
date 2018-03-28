/*
 * MPEG-2 transport stream (aka DVB) muxer
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avassert.h"
#include "libavutil/bswap.h"
#include "libavutil/crc.h"
#include "libavutil/dict.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"

#include "libavcodec/internal.h"

#include "avformat.h"
#include "avio_internal.h"
#include "internal.h"
#include "mpegts.h"

#define PCR_TIME_BASE 27000000

/* write DVB SI sections */

#define DVB_PRIVATE_NETWORK_START 0xff01

/*********************************************/
/* mpegts section writer */

typedef struct MpegTSSection {
    int pid;
    int cc;
    int discontinuity;
    void (*write_packet)(struct MpegTSSection *s, const uint8_t *packet);
    void *opaque;
} MpegTSSection;

typedef struct MpegTSService {
    MpegTSSection pmt; /* MPEG-2 PMT table context */
    int sid;           /* service ID */
    char *name;
    char *provider_name;
    int pcr_pid;
    int pcr_packet_count;
    int pcr_packet_period;
    AVProgram *program;
} MpegTSService;

// service_type values as defined in ETSI 300 468
enum {
    MPEGTS_SERVICE_TYPE_DIGITAL_TV                   = 0x01,
    MPEGTS_SERVICE_TYPE_DIGITAL_RADIO                = 0x02,
    MPEGTS_SERVICE_TYPE_TELETEXT                     = 0x03,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_RADIO = 0x0A,
    MPEGTS_SERVICE_TYPE_MPEG2_DIGITAL_HDTV           = 0x11,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_SDTV  = 0x16,
    MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_HDTV  = 0x19,
    MPEGTS_SERVICE_TYPE_HEVC_DIGITAL_HDTV            = 0x1F,
};
typedef struct MpegTSWrite {
    const AVClass *av_class;
    MpegTSSection pat; /* MPEG-2 PAT table */
    MpegTSSection sdt; /* MPEG-2 SDT table context */
    MpegTSService **services;
    int sdt_packet_count;
    int sdt_packet_period;
    int pat_packet_count;
    int pat_packet_period;
    int nb_services;
    int onid;
    int tsid;
    int64_t first_pcr;
    int mux_rate; ///< set to 1 when VBR
    int pes_payload_size;

    int transport_stream_id;
    int original_network_id;
    int service_id;
    int service_type;

    int pmt_start_pid;
    int start_pid;
    int m2ts_mode;

    int reemit_pat_pmt; // backward compatibility

    int pcr_period;
#define MPEGTS_FLAG_REEMIT_PAT_PMT  0x01
#define MPEGTS_FLAG_AAC_LATM        0x02
#define MPEGTS_FLAG_PAT_PMT_AT_FRAMES           0x04
#define MPEGTS_FLAG_SYSTEM_B        0x08
#define MPEGTS_FLAG_DISCONT         0x10
    int flags;
    int copyts;
    int tables_version;
    double pat_period;
    double sdt_period;
    int64_t last_pat_ts;
    int64_t last_sdt_ts;

    int omit_video_pes_length;
} MpegTSWrite;

/* a PES packet header is generated every DEFAULT_PES_HEADER_FREQ packets */
#define DEFAULT_PES_HEADER_FREQ  16
#define DEFAULT_PES_PAYLOAD_SIZE ((DEFAULT_PES_HEADER_FREQ - 1) * 184 + 170)

/* The section length is 12 bits. The first 2 are set to 0, the remaining
 * 10 bits should not exceed 1021. */
#define SECTION_LENGTH 1020

/* NOTE: 4 bytes must be left at the end for the crc32 */
static void mpegts_write_section(MpegTSSection *s, uint8_t *buf, int len)
{
    unsigned int crc;
    unsigned char packet[TS_PACKET_SIZE];
    const unsigned char *buf_ptr;
    unsigned char *q;
    int first, b, len1, left;

    crc = av_bswap32(av_crc(av_crc_get_table(AV_CRC_32_IEEE),
                            -1, buf, len - 4));

    buf[len - 4] = (crc >> 24) & 0xff;
    buf[len - 3] = (crc >> 16) & 0xff;
    buf[len - 2] = (crc >>  8) & 0xff;
    buf[len - 1] =  crc        & 0xff;

    /* send each packet */
    buf_ptr = buf;
    while (len > 0) {
        first = buf == buf_ptr;
        q     = packet;
        *q++  = 0x47;
        b     = s->pid >> 8;
        if (first)
            b |= 0x40;
        *q++  = b;
        *q++  = s->pid;
        s->cc = s->cc + 1 & 0xf;
        *q++  = 0x10 | s->cc;
        if (s->discontinuity) {
            q[-1] |= 0x20;
            *q++ = 1;
            *q++ = 0x80;
            s->discontinuity = 0;
        }
        if (first)
            *q++ = 0; /* 0 offset */
        len1 = TS_PACKET_SIZE - (q - packet);
        if (len1 > len)
            len1 = len;
        memcpy(q, buf_ptr, len1);
        q += len1;
        /* add known padding data */
        left = TS_PACKET_SIZE - (q - packet);
        if (left > 0)
            memset(q, 0xff, left);

        s->write_packet(s, packet);

        buf_ptr += len1;
        len     -= len1;
    }
}

static inline void put16(uint8_t **q_ptr, int val)
{
    uint8_t *q;
    q      = *q_ptr;
    *q++   = val >> 8;
    *q++   = val;
    *q_ptr = q;
}

static int mpegts_write_section1(MpegTSSection *s, int tid, int id,
                                 int version, int sec_num, int last_sec_num,
                                 uint8_t *buf, int len)
{
    uint8_t section[1024], *q;
    unsigned int tot_len;
    /* reserved_future_use field must be set to 1 for SDT */
    unsigned int flags = tid == SDT_TID ? 0xf000 : 0xb000;

    tot_len = 3 + 5 + len + 4;
    /* check if not too big */
    if (tot_len > 1024)
        return AVERROR_INVALIDDATA;

    q    = section;
    *q++ = tid;
    put16(&q, flags | (len + 5 + 4)); /* 5 byte header + 4 byte CRC */
    put16(&q, id);
    *q++ = 0xc1 | (version << 1); /* current_next_indicator = 1 */
    *q++ = sec_num;
    *q++ = last_sec_num;
    memcpy(q, buf, len);

    mpegts_write_section(s, section, tot_len);
    return 0;
}

/*********************************************/
/* mpegts writer */

#define DEFAULT_PROVIDER_NAME   "FFmpeg"
#define DEFAULT_SERVICE_NAME    "Service01"

/* we retransmit the SI info at this rate */
#define SDT_RETRANS_TIME 500
#define PAT_RETRANS_TIME 100
#define PCR_RETRANS_TIME 20

typedef struct MpegTSWriteStream {
    struct MpegTSService *service;
    int pid; /* stream associated pid */
    int cc;
    int discontinuity;
    int payload_size;
    int first_pts_check; ///< first pts check needed
    int prev_payload_key;
    int64_t payload_pts;
    int64_t payload_dts;
    int payload_flags;
    uint8_t *payload;
    AVFormatContext *amux;
    AVRational user_tb;

    /* For Opus */
    int opus_queued_samples;
    int opus_pending_trim_start;
} MpegTSWriteStream;

static void mpegts_write_pat(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSService *service;
    uint8_t data[SECTION_LENGTH], *q;
    int i;

    q = data;
    for (i = 0; i < ts->nb_services; i++) {
        service = ts->services[i];
        put16(&q, service->sid);
        put16(&q, 0xe000 | service->pmt.pid);
    }
    mpegts_write_section1(&ts->pat, PAT_TID, ts->tsid, ts->tables_version, 0, 0,
                          data, q - data);
}

/* NOTE: !str is accepted for an empty string */
static void putstr8(uint8_t **q_ptr, const char *str, int write_len)
{
    uint8_t *q;
    int len;

    q = *q_ptr;
    if (!str)
        len = 0;
    else
        len = strlen(str);
    if (write_len)
        *q++ = len;
    memcpy(q, str, len);
    q     += len;
    *q_ptr = q;
}

static int mpegts_write_pmt(AVFormatContext *s, MpegTSService *service)
{
    MpegTSWrite *ts = s->priv_data;
    uint8_t data[SECTION_LENGTH], *q, *desc_length_ptr, *program_info_length_ptr;
    int val, stream_type, i, err = 0;

    q = data;
    put16(&q, 0xe000 | service->pcr_pid);

    program_info_length_ptr = q;
    q += 2; /* patched after */

    /* put program info here */

    val = 0xf000 | (q - program_info_length_ptr - 2);
    program_info_length_ptr[0] = val >> 8;
    program_info_length_ptr[1] = val;

    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st = s->streams[i];
        MpegTSWriteStream *ts_st = st->priv_data;
        AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);

        if (s->nb_programs) {
            int k, found = 0;
            AVProgram *program = service->program;

            for (k = 0; k < program->nb_stream_indexes; k++)
                if (program->stream_index[k] == i) {
                    found = 1;
                    break;
                }

            if (!found)
                continue;
        }

        if (q - data > SECTION_LENGTH - 32) {
            err = 1;
            break;
        }
        switch (st->codecpar->codec_id) {
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
            stream_type = STREAM_TYPE_VIDEO_MPEG2;
            break;
        case AV_CODEC_ID_MPEG4:
            stream_type = STREAM_TYPE_VIDEO_MPEG4;
            break;
        case AV_CODEC_ID_H264:
            stream_type = STREAM_TYPE_VIDEO_H264;
            break;
        case AV_CODEC_ID_HEVC:
            stream_type = STREAM_TYPE_VIDEO_HEVC;
            break;
        case AV_CODEC_ID_CAVS:
            stream_type = STREAM_TYPE_VIDEO_CAVS;
            break;
        case AV_CODEC_ID_DIRAC:
            stream_type = STREAM_TYPE_VIDEO_DIRAC;
            break;
        case AV_CODEC_ID_VC1:
            stream_type = STREAM_TYPE_VIDEO_VC1;
            break;
        case AV_CODEC_ID_MP2:
        case AV_CODEC_ID_MP3:
            if (   st->codecpar->sample_rate > 0
                && st->codecpar->sample_rate < 32000) {
                stream_type = STREAM_TYPE_AUDIO_MPEG2;
            } else {
                stream_type = STREAM_TYPE_AUDIO_MPEG1;
            }
            break;
        case AV_CODEC_ID_AAC:
            stream_type = (ts->flags & MPEGTS_FLAG_AAC_LATM)
                          ? STREAM_TYPE_AUDIO_AAC_LATM
                          : STREAM_TYPE_AUDIO_AAC;
            break;
        case AV_CODEC_ID_AAC_LATM:
            stream_type = STREAM_TYPE_AUDIO_AAC_LATM;
            break;
        case AV_CODEC_ID_AC3:
            stream_type = (ts->flags & MPEGTS_FLAG_SYSTEM_B)
                          ? STREAM_TYPE_PRIVATE_DATA
                          : STREAM_TYPE_AUDIO_AC3;
            break;
        case AV_CODEC_ID_EAC3:
            stream_type = (ts->flags & MPEGTS_FLAG_SYSTEM_B)
                          ? STREAM_TYPE_PRIVATE_DATA
                          : STREAM_TYPE_AUDIO_EAC3;
            break;
        case AV_CODEC_ID_DTS:
            stream_type = STREAM_TYPE_AUDIO_DTS;
            break;
        case AV_CODEC_ID_TRUEHD:
            stream_type = STREAM_TYPE_AUDIO_TRUEHD;
            break;
        case AV_CODEC_ID_OPUS:
            stream_type = STREAM_TYPE_PRIVATE_DATA;
            break;
        case AV_CODEC_ID_TIMED_ID3:
            stream_type = STREAM_TYPE_METADATA;
            break;
        default:
            stream_type = STREAM_TYPE_PRIVATE_DATA;
            break;
        }

        *q++ = stream_type;
        put16(&q, 0xe000 | ts_st->pid);
        desc_length_ptr = q;
        q += 2; /* patched after */

        /* write optional descriptors here */
        switch (st->codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            if (st->codecpar->codec_id==AV_CODEC_ID_AC3 && (ts->flags & MPEGTS_FLAG_SYSTEM_B)) {
                *q++=0x6a; // AC3 descriptor see A038 DVB SI
                *q++=1; // 1 byte, all flags sets to 0
                *q++=0; // omit all fields...
            }
            if (st->codecpar->codec_id==AV_CODEC_ID_EAC3 && (ts->flags & MPEGTS_FLAG_SYSTEM_B)) {
                *q++=0x7a; // EAC3 descriptor see A038 DVB SI
                *q++=1; // 1 byte, all flags sets to 0
                *q++=0; // omit all fields...
            }
            if (st->codecpar->codec_id==AV_CODEC_ID_S302M) {
                *q++ = 0x05; /* MPEG-2 registration descriptor*/
                *q++ = 4;
                *q++ = 'B';
                *q++ = 'S';
                *q++ = 'S';
                *q++ = 'D';
            }
            if (st->codecpar->codec_id==AV_CODEC_ID_OPUS) {
                /* 6 bytes registration descriptor, 4 bytes Opus audio descriptor */
                if (q - data > SECTION_LENGTH - 6 - 4) {
                    err = 1;
                    break;
                }

                *q++ = 0x05; /* MPEG-2 registration descriptor*/
                *q++ = 4;
                *q++ = 'O';
                *q++ = 'p';
                *q++ = 'u';
                *q++ = 's';

                *q++ = 0x7f; /* DVB extension descriptor */
                *q++ = 2;
                *q++ = 0x80;

                if (st->codecpar->extradata && st->codecpar->extradata_size >= 19) {
                    if (st->codecpar->extradata[18] == 0 && st->codecpar->channels <= 2) {
                        /* RTP mapping family */
                        *q++ = st->codecpar->channels;
                    } else if (st->codecpar->extradata[18] == 1 && st->codecpar->channels <= 8 &&
                               st->codecpar->extradata_size >= 21 + st->codecpar->channels) {
                        static const uint8_t coupled_stream_counts[9] = {
                            1, 0, 1, 1, 2, 2, 2, 3, 3
                        };
                        static const uint8_t channel_map_a[8][8] = {
                            {0},
                            {0, 1},
                            {0, 2, 1},
                            {0, 1, 2, 3},
                            {0, 4, 1, 2, 3},
                            {0, 4, 1, 2, 3, 5},
                            {0, 4, 1, 2, 3, 5, 6},
                            {0, 6, 1, 2, 3, 4, 5, 7},
                        };
                        static const uint8_t channel_map_b[8][8] = {
                            {0},
                            {0, 1},
                            {0, 1, 2},
                            {0, 1, 2, 3},
                            {0, 1, 2, 3, 4},
                            {0, 1, 2, 3, 4, 5},
                            {0, 1, 2, 3, 4, 5, 6},
                            {0, 1, 2, 3, 4, 5, 6, 7},
                        };
                        /* Vorbis mapping family */

                        if (st->codecpar->extradata[19] == st->codecpar->channels - coupled_stream_counts[st->codecpar->channels] &&
                            st->codecpar->extradata[20] == coupled_stream_counts[st->codecpar->channels] &&
                            memcmp(&st->codecpar->extradata[21], channel_map_a[st->codecpar->channels-1], st->codecpar->channels) == 0) {
                            *q++ = st->codecpar->channels;
                        } else if (st->codecpar->channels >= 2 && st->codecpar->extradata[19] == st->codecpar->channels &&
                                   st->codecpar->extradata[20] == 0 &&
                                   memcmp(&st->codecpar->extradata[21], channel_map_b[st->codecpar->channels-1], st->codecpar->channels) == 0) {
                            *q++ = st->codecpar->channels | 0x80;
                        } else {
                            /* Unsupported, could write an extended descriptor here */
                            av_log(s, AV_LOG_ERROR, "Unsupported Opus Vorbis-style channel mapping");
                            *q++ = 0xff;
                        }
                    } else {
                        /* Unsupported */
                        av_log(s, AV_LOG_ERROR, "Unsupported Opus channel mapping for family %d", st->codecpar->extradata[18]);
                        *q++ = 0xff;
                    }
                } else if (st->codecpar->channels <= 2) {
                    /* Assume RTP mapping family */
                    *q++ = st->codecpar->channels;
                } else {
                    /* Unsupported */
                    av_log(s, AV_LOG_ERROR, "Unsupported Opus channel mapping");
                    *q++ = 0xff;
                }
            }

            if (lang) {
                char *p;
                char *next = lang->value;
                uint8_t *len_ptr;

                *q++     = 0x0a; /* ISO 639 language descriptor */
                len_ptr  = q++;
                *len_ptr = 0;

                for (p = lang->value; next && *len_ptr < 255 / 4 * 4; p = next + 1) {
                    if (q - data > SECTION_LENGTH - 4) {
                        err = 1;
                        break;
                    }
                    next = strchr(p, ',');
                    if (strlen(p) != 3 && (!next || next != p + 3))
                        continue; /* not a 3-letter code */

                    *q++ = *p++;
                    *q++ = *p++;
                    *q++ = *p++;

                    if (st->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
                        *q++ = 0x01;
                    else if (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
                        *q++ = 0x02;
                    else if (st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
                        *q++ = 0x03;
                    else
                        *q++ = 0; /* undefined type */

                    *len_ptr += 4;
                }

                if (*len_ptr == 0)
                    q -= 2; /* no language codes were written */
            }
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        {
           const char default_language[] = "und";
           const char *language = lang && strlen(lang->value) >= 3 ? lang->value : default_language;

           if (st->codecpar->codec_id == AV_CODEC_ID_DVB_SUBTITLE) {
               uint8_t *len_ptr;
               int extradata_copied = 0;

               *q++ = 0x59; /* subtitling_descriptor */
               len_ptr = q++;

               while (strlen(language) >= 3) {
                   if (sizeof(data) - (q - data) < 8) { /* 8 bytes per DVB subtitle substream data */
                       err = 1;
                       break;
                   }
                   *q++ = *language++;
                   *q++ = *language++;
                   *q++ = *language++;
                   /* Skip comma */
                   if (*language != '\0')
                       language++;

                   if (st->codecpar->extradata_size - extradata_copied >= 5) {
                       *q++ = st->codecpar->extradata[extradata_copied + 4]; /* subtitling_type */
                       memcpy(q, st->codecpar->extradata + extradata_copied, 4); /* composition_page_id and ancillary_page_id */
                       extradata_copied += 5;
                       q += 4;
                   } else {
                       /* subtitling_type:
                        * 0x10 - normal with no monitor aspect ratio criticality
                        * 0x20 - for the hard of hearing with no monitor aspect ratio criticality */
                       *q++ = (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED) ? 0x20 : 0x10;
                       if ((st->codecpar->extradata_size == 4) && (extradata_copied == 0)) {
                           /* support of old 4-byte extradata format */
                           memcpy(q, st->codecpar->extradata, 4); /* composition_page_id and ancillary_page_id */
                           extradata_copied += 4;
                           q += 4;
                       } else {
                           put16(&q, 1); /* composition_page_id */
                           put16(&q, 1); /* ancillary_page_id */
                       }
                   }
               }

               *len_ptr = q - len_ptr - 1;
           } else if (st->codecpar->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
               uint8_t *len_ptr = NULL;
               int extradata_copied = 0;

               /* The descriptor tag. teletext_descriptor */
               *q++ = 0x56;
               len_ptr = q++;

               while (strlen(language) >= 3 && q - data < sizeof(data) - 6) {
                   *q++ = *language++;
                   *q++ = *language++;
                   *q++ = *language++;
                   /* Skip comma */
                   if (*language != '\0')
                       language++;

                   if (st->codecpar->extradata_size - 1 > extradata_copied) {
                       memcpy(q, st->codecpar->extradata + extradata_copied, 2);
                       extradata_copied += 2;
                       q += 2;
                   } else {
                       /* The Teletext descriptor:
                        * teletext_type: This 5-bit field indicates the type of Teletext page indicated. (0x01 Initial Teletext page)
                        * teletext_magazine_number: This is a 3-bit field which identifies the magazine number.
                        * teletext_page_number: This is an 8-bit field giving two 4-bit hex digits identifying the page number. */
                       *q++ = 0x08;
                       *q++ = 0x00;
                   }
               }

               *len_ptr = q - len_ptr - 1;
            }
        }
        break;
        case AVMEDIA_TYPE_VIDEO:
            if (stream_type == STREAM_TYPE_VIDEO_DIRAC) {
                *q++ = 0x05; /*MPEG-2 registration descriptor*/
                *q++ = 4;
                *q++ = 'd';
                *q++ = 'r';
                *q++ = 'a';
                *q++ = 'c';
            } else if (stream_type == STREAM_TYPE_VIDEO_VC1) {
                *q++ = 0x05; /*MPEG-2 registration descriptor*/
                *q++ = 4;
                *q++ = 'V';
                *q++ = 'C';
                *q++ = '-';
                *q++ = '1';
            }
            break;
        case AVMEDIA_TYPE_DATA:
            if (st->codecpar->codec_id == AV_CODEC_ID_SMPTE_KLV) {
                *q++ = 0x05; /* MPEG-2 registration descriptor */
                *q++ = 4;
                *q++ = 'K';
                *q++ = 'L';
                *q++ = 'V';
                *q++ = 'A';
            } else if (st->codecpar->codec_id == AV_CODEC_ID_TIMED_ID3) {
                const char *tag = "ID3 ";
                *q++ = 0x26; /* metadata descriptor */
                *q++ = 13;
                put16(&q, 0xffff);    /* metadata application format */
                putstr8(&q, tag, 0);
                *q++ = 0xff;        /* metadata format */
                putstr8(&q, tag, 0);
                *q++ = 0;            /* metadata service ID */
                *q++ = 0xF;          /* metadata_locator_record_flag|MPEG_carriage_flags|reserved */
            }
            break;
        }

        val = 0xf000 | (q - desc_length_ptr - 2);
        desc_length_ptr[0] = val >> 8;
        desc_length_ptr[1] = val;
    }

    if (err)
        av_log(s, AV_LOG_ERROR,
               "The PMT section cannot fit stream %d and all following streams.\n"
               "Try reducing the number of languages in the audio streams "
               "or the total number of streams.\n", i);

    mpegts_write_section1(&service->pmt, PMT_TID, service->sid, ts->tables_version, 0, 0,
                          data, q - data);
    return 0;
}

static void mpegts_write_sdt(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSService *service;
    uint8_t data[SECTION_LENGTH], *q, *desc_list_len_ptr, *desc_len_ptr;
    int i, running_status, free_ca_mode, val;

    q = data;
    put16(&q, ts->onid);
    *q++ = 0xff;
    for (i = 0; i < ts->nb_services; i++) {
        service = ts->services[i];
        put16(&q, service->sid);
        *q++              = 0xfc | 0x00; /* currently no EIT info */
        desc_list_len_ptr = q;
        q                += 2;
        running_status    = 4; /* running */
        free_ca_mode      = 0;

        /* write only one descriptor for the service name and provider */
        *q++         = 0x48;
        desc_len_ptr = q;
        q++;
        *q++         = ts->service_type;
        putstr8(&q, service->provider_name, 1);
        putstr8(&q, service->name, 1);
        desc_len_ptr[0] = q - desc_len_ptr - 1;

        /* fill descriptor length */
        val = (running_status << 13) | (free_ca_mode << 12) |
              (q - desc_list_len_ptr - 2);
        desc_list_len_ptr[0] = val >> 8;
        desc_list_len_ptr[1] = val;
    }
    mpegts_write_section1(&ts->sdt, SDT_TID, ts->tsid, ts->tables_version, 0, 0,
                          data, q - data);
}

static MpegTSService *mpegts_add_service(MpegTSWrite *ts, int sid,
                                         const char *provider_name,
                                         const char *name)
{
    MpegTSService *service;

    service = av_mallocz(sizeof(MpegTSService));
    if (!service)
        return NULL;
    service->pmt.pid       = ts->pmt_start_pid + ts->nb_services;
    service->sid           = sid;
    service->pcr_pid       = 0x1fff;
    service->provider_name = av_strdup(provider_name);
    service->name          = av_strdup(name);
    if (!service->provider_name || !service->name)
        goto fail;
    if (av_dynarray_add_nofree(&ts->services, &ts->nb_services, service) < 0)
        goto fail;

    return service;
fail:
    av_freep(&service->provider_name);
    av_freep(&service->name);
    av_free(service);
    return NULL;
}

static int64_t get_pcr(const MpegTSWrite *ts, AVIOContext *pb)
{
    return av_rescale(avio_tell(pb) + 11, 8 * PCR_TIME_BASE, ts->mux_rate) +
           ts->first_pcr;
}

static void mpegts_prefix_m2ts_header(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    if (ts->m2ts_mode) {
        int64_t pcr = get_pcr(s->priv_data, s->pb);
        uint32_t tp_extra_header = pcr % 0x3fffffff;
        tp_extra_header = AV_RB32(&tp_extra_header);
        avio_write(s->pb, (unsigned char *) &tp_extra_header,
                   sizeof(tp_extra_header));
    }
}

static void section_write_packet(MpegTSSection *s, const uint8_t *packet)
{
    AVFormatContext *ctx = s->opaque;
    mpegts_prefix_m2ts_header(ctx);
    avio_write(ctx->pb, packet, TS_PACKET_SIZE);
}

static int mpegts_init(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSWriteStream *ts_st;
    MpegTSService *service;
    AVStream *st, *pcr_st = NULL;
    AVDictionaryEntry *title, *provider;
    int i, j;
    const char *service_name;
    const char *provider_name;
    int *pids;
    int ret;

    if (s->max_delay < 0) /* Not set by the caller */
        s->max_delay = 0;

    // round up to a whole number of TS packets
    ts->pes_payload_size = (ts->pes_payload_size + 14 + 183) / 184 * 184 - 14;

    ts->tsid = ts->transport_stream_id;
    ts->onid = ts->original_network_id;
    if (!s->nb_programs) {
        /* allocate a single DVB service */
        title = av_dict_get(s->metadata, "service_name", NULL, 0);
        if (!title)
            title = av_dict_get(s->metadata, "title", NULL, 0);
        service_name  = title ? title->value : DEFAULT_SERVICE_NAME;
        provider      = av_dict_get(s->metadata, "service_provider", NULL, 0);
        provider_name = provider ? provider->value : DEFAULT_PROVIDER_NAME;
        service       = mpegts_add_service(ts, ts->service_id,
                                           provider_name, service_name);

        if (!service)
            return AVERROR(ENOMEM);

        service->pmt.write_packet = section_write_packet;
        service->pmt.opaque       = s;
        service->pmt.cc           = 15;
        service->pmt.discontinuity= ts->flags & MPEGTS_FLAG_DISCONT;
    } else {
        for (i = 0; i < s->nb_programs; i++) {
            AVProgram *program = s->programs[i];
            title = av_dict_get(program->metadata, "service_name", NULL, 0);
            if (!title)
                title = av_dict_get(program->metadata, "title", NULL, 0);
            service_name  = title ? title->value : DEFAULT_SERVICE_NAME;
            provider      = av_dict_get(program->metadata, "service_provider", NULL, 0);
            provider_name = provider ? provider->value : DEFAULT_PROVIDER_NAME;
            service       = mpegts_add_service(ts, program->id,
                                               provider_name, service_name);

            if (!service)
                return AVERROR(ENOMEM);

            service->pmt.write_packet = section_write_packet;
            service->pmt.opaque       = s;
            service->pmt.cc           = 15;
            service->pmt.discontinuity= ts->flags & MPEGTS_FLAG_DISCONT;
            service->program          = program;
        }
    }

    ts->pat.pid          = PAT_PID;
    /* Initialize at 15 so that it wraps and is equal to 0 for the
     * first packet we write. */
    ts->pat.cc           = 15;
    ts->pat.discontinuity= ts->flags & MPEGTS_FLAG_DISCONT;
    ts->pat.write_packet = section_write_packet;
    ts->pat.opaque       = s;

    ts->sdt.pid          = SDT_PID;
    ts->sdt.cc           = 15;
    ts->sdt.discontinuity= ts->flags & MPEGTS_FLAG_DISCONT;
    ts->sdt.write_packet = section_write_packet;
    ts->sdt.opaque       = s;

    pids = av_malloc_array(s->nb_streams, sizeof(*pids));
    if (!pids) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    /* assign pids to each stream */
    for (i = 0; i < s->nb_streams; i++) {
        AVProgram *program;
        st = s->streams[i];

        ts_st = av_mallocz(sizeof(MpegTSWriteStream));
        if (!ts_st) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        st->priv_data = ts_st;

        ts_st->user_tb = st->time_base;
        avpriv_set_pts_info(st, 33, 1, 90000);

        ts_st->payload = av_mallocz(ts->pes_payload_size);
        if (!ts_st->payload) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        program = av_find_program_from_stream(s, NULL, i);
        if (program) {
            for (j = 0; j < ts->nb_services; j++) {
                if (ts->services[j]->program == program) {
                    service = ts->services[j];
                    break;
                }
            }
        }

        ts_st->service = service;
        /* MPEG pid values < 16 are reserved. Applications which set st->id in
         * this range are assigned a calculated pid. */
        if (st->id < 16) {
            ts_st->pid = ts->start_pid + i;
        } else if (st->id < 0x1FFF) {
            ts_st->pid = st->id;
        } else {
            av_log(s, AV_LOG_ERROR,
                   "Invalid stream id %d, must be less than 8191\n", st->id);
            ret = AVERROR(EINVAL);
            goto fail;
        }
        if (ts_st->pid == service->pmt.pid) {
            av_log(s, AV_LOG_ERROR, "Duplicate stream id %d\n", ts_st->pid);
            ret = AVERROR(EINVAL);
            goto fail;
        }
        for (j = 0; j < i; j++) {
            if (pids[j] == ts_st->pid) {
                av_log(s, AV_LOG_ERROR, "Duplicate stream id %d\n", ts_st->pid);
                ret = AVERROR(EINVAL);
                goto fail;
            }
        }
        pids[i]                = ts_st->pid;
        ts_st->payload_pts     = AV_NOPTS_VALUE;
        ts_st->payload_dts     = AV_NOPTS_VALUE;
        ts_st->first_pts_check = 1;
        ts_st->cc              = 15;
        ts_st->discontinuity   = ts->flags & MPEGTS_FLAG_DISCONT;
        /* update PCR pid by using the first video stream */
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            service->pcr_pid == 0x1fff) {
            service->pcr_pid = ts_st->pid;
            pcr_st           = st;
        }
        if (st->codecpar->codec_id == AV_CODEC_ID_AAC &&
            st->codecpar->extradata_size > 0) {
            AVStream *ast;
            ts_st->amux = avformat_alloc_context();
            if (!ts_st->amux) {
                ret = AVERROR(ENOMEM);
                goto fail;
            }
            ts_st->amux->oformat =
                av_guess_format((ts->flags & MPEGTS_FLAG_AAC_LATM) ? "latm" : "adts",
                                NULL, NULL);
            if (!ts_st->amux->oformat) {
                ret = AVERROR(EINVAL);
                goto fail;
            }
            if (!(ast = avformat_new_stream(ts_st->amux, NULL))) {
                ret = AVERROR(ENOMEM);
                goto fail;
            }
            ret = avcodec_parameters_copy(ast->codecpar, st->codecpar);
            if (ret != 0)
                goto fail;
            ast->time_base = st->time_base;
            ret = avformat_write_header(ts_st->amux, NULL);
            if (ret < 0)
                goto fail;
        }
        if (st->codecpar->codec_id == AV_CODEC_ID_OPUS) {
            ts_st->opus_pending_trim_start = st->codecpar->initial_padding * 48000 / st->codecpar->sample_rate;
        }
    }

    av_freep(&pids);

    /* if no video stream, use the first stream as PCR */
    if (service->pcr_pid == 0x1fff && s->nb_streams > 0) {
        pcr_st           = s->streams[0];
        ts_st            = pcr_st->priv_data;
        service->pcr_pid = ts_st->pid;
    } else
        ts_st = pcr_st->priv_data;

    if (ts->mux_rate > 1) {
        service->pcr_packet_period = (int64_t)ts->mux_rate * ts->pcr_period /
                                     (TS_PACKET_SIZE * 8 * 1000);
        ts->sdt_packet_period      = (int64_t)ts->mux_rate * SDT_RETRANS_TIME /
                                     (TS_PACKET_SIZE * 8 * 1000);
        ts->pat_packet_period      = (int64_t)ts->mux_rate * PAT_RETRANS_TIME /
                                     (TS_PACKET_SIZE * 8 * 1000);

        if (ts->copyts < 1)
            ts->first_pcr = av_rescale(s->max_delay, PCR_TIME_BASE, AV_TIME_BASE);
    } else {
        /* Arbitrary values, PAT/PMT will also be written on video key frames */
        ts->sdt_packet_period = 200;
        ts->pat_packet_period = 40;
        if (pcr_st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            int frame_size = av_get_audio_frame_duration2(pcr_st->codecpar, 0);
            if (!frame_size) {
                av_log(s, AV_LOG_WARNING, "frame size not set\n");
                service->pcr_packet_period =
                    pcr_st->codecpar->sample_rate / (10 * 512);
            } else {
                service->pcr_packet_period =
                    pcr_st->codecpar->sample_rate / (10 * frame_size);
            }
        } else {
            // max delta PCR 0.1s
            // TODO: should be avg_frame_rate
            service->pcr_packet_period =
                ts_st->user_tb.den / (10 * ts_st->user_tb.num);
        }
        if (!service->pcr_packet_period)
            service->pcr_packet_period = 1;
    }

    ts->last_pat_ts = AV_NOPTS_VALUE;
    ts->last_sdt_ts = AV_NOPTS_VALUE;
    // The user specified a period, use only it
    if (ts->pat_period < INT_MAX/2) {
        ts->pat_packet_period = INT_MAX;
    }
    if (ts->sdt_period < INT_MAX/2) {
        ts->sdt_packet_period = INT_MAX;
    }

    // output a PCR as soon as possible
    service->pcr_packet_count = service->pcr_packet_period;
    ts->pat_packet_count      = ts->pat_packet_period - 1;
    ts->sdt_packet_count      = ts->sdt_packet_period - 1;

    if (ts->mux_rate == 1)
        av_log(s, AV_LOG_VERBOSE, "muxrate VBR, ");
    else
        av_log(s, AV_LOG_VERBOSE, "muxrate %d, ", ts->mux_rate);
    av_log(s, AV_LOG_VERBOSE,
           "pcr every %d pkts, sdt every %d, pat/pmt every %d pkts\n",
           service->pcr_packet_period,
           ts->sdt_packet_period, ts->pat_packet_period);

    if (ts->m2ts_mode == -1) {
        if (av_match_ext(s->filename, "m2ts")) {
            ts->m2ts_mode = 1;
        } else {
            ts->m2ts_mode = 0;
        }
    }

    return 0;

fail:
    av_freep(&pids);
    return ret;
}

/* send SDT, PAT and PMT tables regularly */
static void retransmit_si_info(AVFormatContext *s, int force_pat, int64_t dts)
{
    MpegTSWrite *ts = s->priv_data;
    int i;

    if (++ts->sdt_packet_count == ts->sdt_packet_period ||
        (dts != AV_NOPTS_VALUE && ts->last_sdt_ts == AV_NOPTS_VALUE) ||
        (dts != AV_NOPTS_VALUE && dts - ts->last_sdt_ts >= ts->sdt_period*90000.0)
    ) {
        ts->sdt_packet_count = 0;
        if (dts != AV_NOPTS_VALUE)
            ts->last_sdt_ts = FFMAX(dts, ts->last_sdt_ts);
        mpegts_write_sdt(s);
    }
    if (++ts->pat_packet_count == ts->pat_packet_period ||
        (dts != AV_NOPTS_VALUE && ts->last_pat_ts == AV_NOPTS_VALUE) ||
        (dts != AV_NOPTS_VALUE && dts - ts->last_pat_ts >= ts->pat_period*90000.0) ||
        force_pat) {
        ts->pat_packet_count = 0;
        if (dts != AV_NOPTS_VALUE)
            ts->last_pat_ts = FFMAX(dts, ts->last_pat_ts);
        mpegts_write_pat(s);
        for (i = 0; i < ts->nb_services; i++)
            mpegts_write_pmt(s, ts->services[i]);
    }
}

static int write_pcr_bits(uint8_t *buf, int64_t pcr)
{
    int64_t pcr_low = pcr % 300, pcr_high = pcr / 300;

    *buf++ = pcr_high >> 25;
    *buf++ = pcr_high >> 17;
    *buf++ = pcr_high >>  9;
    *buf++ = pcr_high >>  1;
    *buf++ = pcr_high <<  7 | pcr_low >> 8 | 0x7e;
    *buf++ = pcr_low;

    return 6;
}

/* Write a single null transport stream packet */
static void mpegts_insert_null_packet(AVFormatContext *s)
{
    uint8_t *q;
    uint8_t buf[TS_PACKET_SIZE];

    q    = buf;
    *q++ = 0x47;
    *q++ = 0x00 | 0x1f;
    *q++ = 0xff;
    *q++ = 0x10;
    memset(q, 0x0FF, TS_PACKET_SIZE - (q - buf));
    mpegts_prefix_m2ts_header(s);
    avio_write(s->pb, buf, TS_PACKET_SIZE);
}

/* Write a single transport stream packet with a PCR and no payload */
static void mpegts_insert_pcr_only(AVFormatContext *s, AVStream *st)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSWriteStream *ts_st = st->priv_data;
    uint8_t *q;
    uint8_t buf[TS_PACKET_SIZE];

    q    = buf;
    *q++ = 0x47;
    *q++ = ts_st->pid >> 8;
    *q++ = ts_st->pid;
    *q++ = 0x20 | ts_st->cc;   /* Adaptation only */
    /* Continuity Count field does not increment (see 13818-1 section 2.4.3.3) */
    *q++ = TS_PACKET_SIZE - 5; /* Adaptation Field Length */
    *q++ = 0x10;               /* Adaptation flags: PCR present */
    if (ts_st->discontinuity) {
        q[-1] |= 0x80;
        ts_st->discontinuity = 0;
    }

    /* PCR coded into 6 bytes */
    q += write_pcr_bits(q, get_pcr(ts, s->pb));

    /* stuffing bytes */
    memset(q, 0xFF, TS_PACKET_SIZE - (q - buf));
    mpegts_prefix_m2ts_header(s);
    avio_write(s->pb, buf, TS_PACKET_SIZE);
}

static void write_pts(uint8_t *q, int fourbits, int64_t pts)
{
    int val;

	//11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS

    val  = fourbits << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    *q++ = val;
    val  = (((pts >> 15) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
    val  = (((pts) & 0x7fff) << 1) | 1;
    *q++ = val >> 8;
    *q++ = val;
}

/* Set an adaptation field flag in an MPEG-TS packet*/
static void set_af_flag(uint8_t *pkt, int flag) //0x80   1 0 0 0 | 0 0 0 0
{
    // expect at least one flag to set
    av_assert0(flag);

	//pkt[3]  transport_scrambling_control(2bit) | adaptation_field_control(2bit) | continuity_counter(4bit)  0 0 1 0 | 0 0 0 0 
	//pkt[3]	传输加扰控制(2bit) | 自适应字段控制(2bit) | 连续计数器(4bit)
	//自适应字段控制 == 10 ‘10’为仅含自适应域，无有效负载
    if ((pkt[3] & 0x20) == 0) {
        // no AF yet, set adaptation field flag
        pkt[3] |= 0x20;
		
        // 1 byte length, no flags
        //pkt[4] adaptation_field_length(8bit) 自适应域长度，后面的字节数
        pkt[4] = 1;

		//pkt[5] discontinue_indicator(1bit) | random_access_indicator(1bit) | elementary_streem_priority_indicator(1bit) | 5flags(5bit)
		//        非连续指示符(1bit) | 随机存取指示符(1bit) | 基本流优先级指示符(1bit) |  5个标志(5bit)
		// flag   取0x50表示包含PCR或0x40表示不包含PCR
		// PCR    Program Clock Reference，节目时钟参考，用于恢复出与编码端一致的系统时序时钟STC（System Time Clock）。
        pkt[5] = 0;
    }

	// pkt[5] 1 0 0 0 | 0 0 0 0    非连续指示符=1
    pkt[5] |= flag;
}

/* Extend the adaptation field by size bytes */
static void extend_af(uint8_t *pkt, int size)
{
    // expect already existing adaptation field
    av_assert0(pkt[3] & 0x20);
    pkt[4] += size;
}

/* Get a pointer to MPEG-TS payload (right after TS packet header) */
static uint8_t *get_ts_payload_start(uint8_t *pkt)
{
	//pkt[3]	传输加扰控制(2bit) | 自适应字段控制(2bit) | 连续计数器(4bit)
	///自适应字段控制 == 10 ‘10’为仅含自适应域，无有效负载
    if (pkt[3] & 0x20)
        return pkt + 5 + pkt[4];  //pkt + 5 跳过自适应长度 + pkt[4]自适应长度
    else
        return pkt + 4;	//跳过头
}

/* Add a PES header to the front of the payload, and segment into an integer
 * number of TS packets. The final TS packet is padded using an oversized
 * adaptation header to exactly fill the last TS packet.
 * NOTE: 'payload' contains a complete PES payload. */
static void mpegts_write_pes(AVFormatContext *s, AVStream *st,
                             const uint8_t *payload, int payload_size,
                             int64_t pts, int64_t dts, int key, int stream_id)
{
    MpegTSWriteStream *ts_st = st->priv_data;
    MpegTSWrite *ts = s->priv_data;
    uint8_t buf[TS_PACKET_SIZE];
    uint8_t *q;
    int val, is_start, len, header_len, write_pcr, is_dvb_subtitle, is_dvb_teletext, flags;
    int afc_len, stuffing_len;
    int64_t pcr = -1; /* avoid warning */
    int64_t delay = av_rescale(s->max_delay, 90000, AV_TIME_BASE);
    int force_pat = st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && key && !ts_st->prev_payload_key;

    av_assert0(ts_st->payload != buf || st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO);
    if (ts->flags & MPEGTS_FLAG_PAT_PMT_AT_FRAMES && st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        force_pat = 1;
    }

    is_start = 1;
    while (payload_size > 0) {
        retransmit_si_info(s, force_pat, dts);
        force_pat = 0;

        write_pcr = 0;
        if (ts_st->pid == ts_st->service->pcr_pid) {
            if (ts->mux_rate > 1 || is_start) // VBR pcr period is based on frames
                ts_st->service->pcr_packet_count++;
            if (ts_st->service->pcr_packet_count >=
                ts_st->service->pcr_packet_period) {
                ts_st->service->pcr_packet_count = 0;
                write_pcr = 1;
            }
        }

        if (ts->mux_rate > 1 && dts != AV_NOPTS_VALUE &&
            (dts - get_pcr(ts, s->pb) / 300) > delay) {
            /* pcr insert gets priority over null packet insert */
            if (write_pcr)
                mpegts_insert_pcr_only(s, st);
            else
                mpegts_insert_null_packet(s);
            /* recalculate write_pcr and possibly retransmit si_info */
            continue;
        }

/*
二.RTP Over UDP：
	一般RTP是基于UDP的，因为UDP发送的是数据报文。规定最大报文为1500byte（字节），即MTU最大传输单元为1500byte。RTP数据长度最多为1400字节。
	
三.RTP Over TCP：
从上面RTP格式可以看到RTP如果使用TCP有个致命的缺点，就是没有长度。而TCP是传输的数据流。而RTP是数据报文。如果使用TCP传输就无法知道RTP包有多长。不知如何区分RTP包。
这里用到RTSP。以下是截取的TCP协议中的一段
TCP segment data：

|-----------|-----------------|----------------|----------------------|
| $ (1byte) | channel (1byte) | length (2byte) | payload (Rtp_Packet) |
|-----------|-----------------|----------------|----------------------|

1. $ (占1byte) 
	表示在TCP中是RTP协议即RTP Over TCP。

2. channel (占1byte)
	0X00表示音频,
	0X01表示视频,
	0X表示application大致是这个意思
	
3. length (占2byte)
	代表RTP的长度

4. Payload
	代表跟着是length长度的RTP数据包内容。
*/

/*
	1. AU -- Access_Unit (存取单元)，AU头部和编码数据。
		1)1个AU相当于编码的1幅视频图像或1个音频帧，也可以说，每个AU实际上是编码数据流的显示单位，即相当于解码的1幅视频图像或1个音频帧的取样。
		
	2. ES -- Elementary_Streams （原始流）是直接从编码器出来的数据流，可所以编码过的视频数据流（H.264，MJPEG等），音频数据流（AAC），或其他编码数据流的统称。
		1)ES流经过PES打包器之后，被转换成PES包。
		2)ES是只包含一种内容的数据流，如只含视频或只含音频等，打包之后的PES也是只含一种性质的ES，如只含视频ES的PES，只含音频ES的PES等。
		3)每个ES都由若干个存取单位（AU）构成。
		4)一般1个PES包包含1个AU，因为每帧音视频帧的 (PTS/DTS) 不同，而且会封装到 PES 包头里面。
		
	3. PES -- Packetized_Elementary_Streams（分组的ES），ES形成的分组称为PES分组，是用来传递ES的一种数据布局。
		PES流是 ES 流经过 PES打包器 处理后形成的数据流，在这个过程中完成了将ES流分组、打包、参加包头信息等操纵（对ES流的第一次打包）。
		PES流的基本单位是PES包。PES包由包头和payload构成。

	4. PS -- Program_Stream（节目流）PS流由PS包构成，而一个PS包又由若干个PES包构成（到这里，ES经过了两层的封装）。PS包的包头中包含了同步信息与时钟恢复信息。
		一个PS包最多可包含具有同一时钟基准的16个视频PES包和32个音频PES包。

	5. TS -- Transport_Stream（传输流）由定长的TS包构成（188字节），而TS包是对PES包的一个从头封装（到这里，ES也经过了两层的封装）。
		PES包的包头信息依然存在于TS包中。

	6. TS流 与 PS流 的差别在于TS流的包布局是固定长度的，而PS流的包布局是可变长度的。 PS包因为长度是变更的，一旦丧失某一PS包的同步信息，接管机就会进入失步状况，
		从而导致严重的信息损出事务。而TS码流因为采取了固定长度的包布局， 当传输误码破损了某一TS包的同步信息时，接管机可在固定的地位检测它后面包中的同步信息，
		从而恢复同步，避免了信息丧失。是以在信道景象较为恶劣、传输 误码较高时一般采取TS码流，而在信景象较好、传输误码较低时一般采取PS码流。

*/

/*
1. 解析ts流要先找到PAT表，只要找到PAT就可以找到PMT，然后就可以找到音视频流了。PAT表的PID值固定为0。
2. PAT表和PMT表需要定期插入ts流，因为用户随时可能加入ts流，这个间隔比较小，通常每隔几个视频帧就要加入PAT和PMT。
3. PAT和PMT表是必须的，还可以加入其它表如SDT（业务描述表）等，不过hls流只要有PAT和PMT就可以播放了。
4. PAT表：他主要的作用就是指明了PMT表的PID值。
5. PMT表：他主要的作用就是指明了音视频流的PID值。

6. PID是TS流中唯一识别标志，Packet Data是什么内容就是由PID决定的。

--------------------
     table | PID
-----------|--------
     PAT   | 0x0000
-----------|--------
     CAT   | 0x0001
-----------|--------
     TSDT  | 0x0002
-----------|--------
    EIT,ST | 0x0012
-----------|--------
    RST,ST | 0x0013
-----------|--------
TDT,TOT,ST | 0x0014
--------------------
*/

/*
	PAT格式(Program Association Table，节目关联表)

----------------------------------------------------------------------------------------------------
1. table_id				8bit		PAT表固定为0x00
----------------------------------------------------------------------------------------------------1byte
2. section_syntax_indicator	1bit		固定为1
----------------------------------------------------------------------------------------------------
3. zero				1bit		固定为0
----------------------------------------------------------------------------------------------------
4. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
5. section_length			12bit		后面数据的长度
----------------------------------------------------------------------------------------------------2byte
6. transport_stream_id		16bit		传输流ID，固定为0x0001
----------------------------------------------------------------------------------------------------2byte
7. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
8. version_number			5bit		版本号，固定为00000，如果PAT有变化则版本号加1
----------------------------------------------------------------------------------------------------
9. current_next_indicator	1bit		固定为1，表示这个PAT表可以用，如果为0则要等待下一个PAT表
----------------------------------------------------------------------------------------------------1byte
10. section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte
11. last_section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte
开始循环	 	 
====================================================================================================
12. program_number		16bit		节目号为 0x0000 时,表示这是 NetWork_ID		--NIT
							节目号为 0x0001 时,表示这是 Program_Map_PID	--PMT
----------------------------------------------------------------------------------------------------2byte
13. reserved			3bit		固定为111
----------------------------------------------------------------------------------------------------
14. PID				13bit		节目号对应内容的 NetWork_ID 值
    NIT 						节目号对应内容的 Program_Map_PID 值
====================================================================================================2byte
结束循环	 	 
----------------------------------------------------------------------------------------------------
15. CRC32				32bit		前面数据的CRC32校验码
----------------------------------------------------------------------------------------------------4byte

*/

/*
	PMT格式( Program Map Table，节目映射表 )
	
----------------------------------------------------------------------------------------------------
1. table_id				8bit		PMT表取值随意，0x02
----------------------------------------------------------------------------------------------------1byte
2. section_syntax_indicator	1bit		固定为1
----------------------------------------------------------------------------------------------------
3. zero				1bit		固定为0
----------------------------------------------------------------------------------------------------
4. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
5. section_length			12bit		后面数据的长度   段长度,从program_number到CRC_32(含)的字节总数
----------------------------------------------------------------------------------------------------2byte
6. program_number			16bit		节目号，表示当前的 PMT 关联到的 节目号，取值0x0001
----------------------------------------------------------------------------------------------------2byte
7. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
8. version_number			5bit		版本号，固定为00000，如果PAT有变化则版本号加1
							如果PMT内容有更新,则它会递增1通知解复用程序需要重新接收节目信息
----------------------------------------------------------------------------------------------------
9. current_next_indicator	1bit		固定为1
----------------------------------------------------------------------------------------------------1byte
10. section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte
11. last_section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte
12. reserved			3bit		固定为111
----------------------------------------------------------------------------------------------------
13. PCR_PID				13bit		PCR (节目参考时钟) 所在TS分组的PID，指定为视频PID
----------------------------------------------------------------------------------------------------2byte
14. reserved			4bit		固定为1111
----------------------------------------------------------------------------------------------------
15. program_info_length		12bit		节目描述信息，指定为0x000表示没有
							节目信息长度(之后的是N个描述符结构,一般可以忽略掉,这个字段就代表描述符总的长度,单位是Bytes)
							紧接着就是频道内部包含的节目类型和对应的PID号码了
----------------------------------------------------------------------------------------------------2byte
开始循环
====================================================================================================
16. stream_type			8bit		流类型，标志是 Video 还是 Audio 还是其他数据
							h.264 编码对应 0x1b
							aac   编码对应 0x0f
							mp3   编码对应 0x03
----------------------------------------------------------------------------------------------------1byte
17. reserved			3bit		固定为111
----------------------------------------------------------------------------------------------------
18. elementary_PID		13bit		与 stream_type 对应的 PID
----------------------------------------------------------------------------------------------------2byte
19. reserved			4bit		固定为1111
----------------------------------------------------------------------------------------------------
20. ES_info_length		12bit		描述信息，指定为 0x000 表示没有
====================================================================================================2byte
结束循环
----------------------------------------------------------------------------------------------------
21. CRC32				32bit		前面数据的CRC32校验码
----------------------------------------------------------------------------------------------------4byte

*/

/*
	H264相关
	
	https://blog.csdn.net/yibu_refresh/article/details/52829643
	
	----------------------------------------------------------------------------------------------------
		Nal_Unit_Type	|			Naul 类型				|		C
	------------------------|-----------------------------------------------|---------------------------
			0		|	未使用						|	
	------------------------|-----------------------------------------------|---------------------------
			1		|	不分区、非IDR 图像的片				|	2, 3, 4
	------------------------|-----------------------------------------------|---------------------------
			2		|	片分区A						|	2
	------------------------|-----------------------------------------------|---------------------------
			3		|	片分区B						|	3
	------------------------|-----------------------------------------------|---------------------------
			4		|	片分区C						|	4
	------------------------|-----------------------------------------------|---------------------------
			5		|	IDR 图像的分片					|	2, 3
	------------------------|-----------------------------------------------|---------------------------
			6		|	补充增强信息单元 (SEI)				|	5
	------------------------|-----------------------------------------------|---------------------------
			7		|	序列参数结 (SPS)					|	0
	------------------------|-----------------------------------------------|---------------------------
			8		|	图像参数集 (PPS)					|	1
	------------------------|-----------------------------------------------|---------------------------
			9		|	分界符						|	6
	------------------------|-----------------------------------------------|---------------------------
			10		|	序列结束						|	7
	------------------------|-----------------------------------------------|---------------------------
			11		|	码流结束						|	8
	------------------------|-----------------------------------------------|---------------------------
			12		|	填充							|	9
	------------------------|-----------------------------------------------|---------------------------
			13 .. 23	|	保留							|
	------------------------|-----------------------------------------------|---------------------------
			24 .. 31	|	未使用						|
	------------------------|-----------------------------------------------|---------------------------


// H.264 NAL type
enum H264NALTYPE
{
	H264NT_NAL = 0,
	H264NT_SLICE, 		//P 帧
	H264NT_SLICE_DPA,
	H264NT_SLICE_DPB,
	H264NT_SLICE_DPC,
	H264NT_SLICE_IDR, 	// I 帧
	H264NT_SEI,
	H264NT_SPS,
	H264NT_PPS,
};

一、 名词解释
	1. 场和帧 ：视频的一场或一帧可用来产生一个编码图像。在电视中，为减少大面积闪烁现象，把一帧分成两个隔行的场。

	2. 片：每个图象中，若干宏块被排列成片的形式。片分为I片、B片、P片和其他一些片。
             I片只包含I宏块，P片可包含P和I宏块，而B片可包含B和I宏块。
             I宏块利用从当前片中已解码的像素作为参考进行帧内预测。
             P宏块利用前面已编码图象作为参考图象进行帧内预测。
             B宏块则利用双向的参考图象（前一帧和后一帧）进行帧内预测。

             片的目的是为了限制误码的扩散和传输，使编码片相互间是独立的。
             某片的预测不能以其它片中的宏块为参考图像，这样某一片中的预测误差才不会传播到其它片中去。

	3. 宏块 ：一个编码图像通常划分成若干宏块组成，一个宏块由一个16×16亮度像素和附加的一个8×8 Cb和一个8×8 Cr彩色像素块组成。

二、 数据之间的关系：
	H264结构中，一个视频图像编码后的数据叫做一帧，一帧由一个片（slice）或多个片组成，一个片由一个或多个宏块（MB）组成，一个宏块由16x16的yuv数据组成。
	宏块作为H264编码的基本单位。
	
三、 H264编码过程中的三种不同的数据形式：
	SODB       数据比特串 ----＞最原始的编码数据，即VCL数据；
	RBSP　     原始字节序列载荷 ----＞在SODB的后面填加了结尾比特（RBSP trailing bits　一个bit“1”）若干比特“0”,以便字节对齐；
	EBSP　     扩展字节序列载荷 ---- > 在RBSP基础上填加了仿校验字节（0X03）它的原因是：　
			在 NALU 加到 Annexb 上时，需要添加每组 NALU 之前的开始码 StartCodePrefix, 如果该 NALU 对应的 slice 为一帧的开始则用4位字节表示，ox00000001,
			否则用3位字节表示ox000001（是一帧的一部分）。另外，为了使NALU主体中不包括与开始码相冲突的，在编码时，
			每遇到两个字节连续为0，就插入一个字节的0x03。解码时将0x03去掉。也称为脱壳操作。

一、 H264/AVC 的分层结构
	1.VCL   video coding layer    	视频编码层；
	2.NAL   network abstraction layer   网络提取层；

	其中，VCL层是对核心算法引擎，块，宏块及片的语法级别的定义，他最终输出编码完的数据 SODB；
	NAL层定义片级以上的语法级别（如序列参数集和图像参数集，针对网络传输），
	同时支持以下功能：独立片解码，起始码唯一保证，SEI以及流格式编码数据传送，NAL 层将 SODB 打包成 RBSP 然后加上NAL头，组成一个NALU（NAL单元）。

二、 H264网络传输的结构
	H264在网络传输的是 NALU，NALU 的结构是：NAL头 + RBSP，实际传输中的数据流如图所示：

	--------------------------------------------------------------------------------------
	... | NAL Header |		RBSP		| NAL Header |		RBSP		| ...
	--------------------------------------------------------------------------------------

	1. NALU_Header 用来标识后面的RBSP是什么类型的数据，他是否会被其他帧参考以及网络传输是否有错误。
	
	1.1 NALU_Header 头结构 (长度：1byte)
		Forbidden_Bit(1bit) + Nal_Reference_Bit(2bit) + Nal_Unit_Type(5bit)
		|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|
		| 	  0		|	   1		|	   2		|	   3		|	   4		|	   5		|	   6		|	   7		|
		|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|
		|   禁止位(1bit) 	| 		   参考级别 		|	 						NALU 单元类型						|
		|-----------------|-----------------------------------|-----------------------------------------------------------------------------------------|
	
	1.1.1 Forbidden_Bit：         禁止位，初始为0，当网络发现NAL单元有比特错误时可设置该比特为1，以便接收方纠错或丢掉该单元。
	1.1.2 Nal_Reference_Bit：     Nal重要性指示，标志该NAL单元的重要性，值越大，越重要，解码器在解码处理不过来的时候，可以丢掉重要性为0的NALU。

	1.1 NALU 头由一个字节组成, 它的语法如下:
		+---------------+
	      |0|1|2|3|4|5|6|7|
	      +-+-+-+-+-+-+-+-+
	      |F|NRI|  Type   |
	      +---------------+

RBSP
	RBSP数据是下表中的一种
	---------------------------------------------------------------------------------
	RBSP类型	 |	所写		|			描述
	-------------|----------------|---------------------------------------------------
	参数集	 |	PS		|	序列的全局信息，如图像尺寸，视频格式等
	-------------|----------------|---------------------------------------------------
	增强信息	 |	SEI 		|	视频序列解码的增强信息
	-------------|----------------|---------------------------------------------------
	图像界定符	 |	PD		|	视频图像的边界
	-------------|----------------|---------------------------------------------------
	编码片	 |	SLICE		|	编码片的头信息和数据
	-------------|----------------|---------------------------------------------------
	数据分割	 |			|	DP片层的数据，用于错误恢复解码
	-------------|----------------|---------------------------------------------------
	序列结束符	 |			|	表明一个序列的结束，下一个图像为IDR图像
	-------------|----------------|---------------------------------------------------
	流结束符	 |			|	表明该流中已没有图像
	-------------|----------------|---------------------------------------------------
	填充数据	 |			|	亚元数据，用于填充字节
	-------------|----------------|----------------------------------------------------


	不同类型的NALU的重要性指示如下表所示。
	----------------------------------------------------------------------------------------------------
		Nal_Unit_Type	|			Nal_Unit_Type			|	Nal_Reference_Bit
	------------------------|-----------------------------------------------|---------------------------
			0		|	未使用						|	0
	------------------------|-----------------------------------------------|---------------------------
			1		|	不分区、非IDR 图像的片				|	此片属于参考帧，则不等于0，不属于参考帧，则等与0
	------------------------|-----------------------------------------------|---------------------------
			2		|	片分区A						|	同上
	------------------------|-----------------------------------------------|---------------------------
			3		|	片分区B						|	同上
	------------------------|-----------------------------------------------|---------------------------
			4		|	片分区C						|	同上
	------------------------|-----------------------------------------------|---------------------------
			5		|	IDR 图像的分片					|	5
	------------------------|-----------------------------------------------|---------------------------
			6		|	补充增强信息单元 (SEI)				|	0
	------------------------|-----------------------------------------------|---------------------------
			7		|	序列参数结 (SPS)					|	非0
	------------------------|-----------------------------------------------|---------------------------
			8		|	图像参数集 (PPS)					|	非0
	------------------------|-----------------------------------------------|---------------------------
			9		|	分界符						|	0
	------------------------|-----------------------------------------------|---------------------------
			10		|	序列结束						|	0
	------------------------|-----------------------------------------------|---------------------------
			11		|	码流结束						|	0
	------------------------|-----------------------------------------------|---------------------------
			12		|	填充							|	0
	------------------------|-----------------------------------------------|---------------------------
			13 .. 23	|	保留							|	0
	------------------------|-----------------------------------------------|---------------------------
			24 .. 31	|	未使用						|	0
	------------------------|-----------------------------------------------|---------------------------

	

	forbidden_bit(1bit) + nal_reference_bit(2bit) + nal_unit_type(5bit)

	(Frame-> 67 & 0x1f == 7)  	=====> SPS
	(Frame-> 68 & 0x1f == 8)	=====> PPS
	(Frame-> 65 & 0x1f == 5)	=====> IDR
	(Frame-> 41 & 0x1f == 1)	=====> 不分区、非IDR 图像的片  baseline 中，对应 P帧

	1.2 参数集
		 参数集:包括序列参数集 SPS  和图像参数集 PPS
	       SPS 包含的是针对一连续编码视频序列的参数，如标识符 seq_parameter_set_id、帧数及 POC 的约束、参考帧数目、解码图像尺寸和帧场编码模式选择标识等等。
	       PPS对应的是一个序列中某一幅图像或者某几幅图像，
	       其参数如标识符 pic_parameter_set_id、可选的 seq_parameter_set_id、熵编码模式选择标识、片组数目、初始量化参数和去方块滤波系数调整标识等等。

	1.3 NAL的开始和结束
		编码器将每个NAL各自独立、完整地放入一个分组，因为分组都有头部，解码器可以方便地检测出NAL的分界，并依次取出NAL进行解码。
		每个NAL前有一个起始码 0x00 00 01（或者0x00 00 00 01），解码器检测每个起始码，作为一个NAL的起始标识，当检测到下一个起始码时，当前NAL结束。

		同时H.264规定，当检测到0x000000时，也可以表征当前NAL的结束。那么NAL中数据出现0x000001或0x000000时怎么办？H.264引入了防止竞争机制，
		如果编码器检测到NAL数据存在0x000001或0x000000时，编码器会在最后个字节前插入一个新的字节0x03，这样：
		0x000000－>0x00000300
		0x000001－>0x00000301
		0x000002－>0x00000302
		0x000003－>0x00000303
		解码器检测到0x000003时，把03抛弃，恢复原始数据（脱壳操作）。解码器在解码时，首先逐个字节读取NAL的数据，统计NAL的长度，然后再开始解码。



三、  NALU的顺序要求
	1.序列参数集NAL单元       
		必须在传送所有以此参数集为参考的其他NAL单元之前传送，不过允许这些NAL单元中间出现重复的序列参数集NAL单元。
		所谓重复的详细解释为：序列参数集NAL单元都有其专门的标识，如果两个序列参数集NAL单元的标识相同，就可以认为后一个只不过是前一个的拷贝，
		而非新的序列参数集。
	2.图像参数集NAL单元      
		必须在所有以此参数集为参考的其他NAL单元之前传送，不过允许这些NAL单元中间出现重复的图像参数集NAL单元，这一点与上述的序列参数集NAL单元是相同的。
	3.不同基本编码图像中的片段（slice）单元和数据划分片段（data partition）单元在顺序上不可以相互交叉，
		即不允许属于某一基本编码图像的一系列片段（slice）单元和数据划分片段（data partition）单元中忽然出现另一个基本编码图像的片段（slice）单元片段和
		数据划分片段（data partition）单元。
	4.参考图像的影响：
		如果一幅图像以另一幅图像为参考，则属于前者的所有片段（slice）单元和数据划分片段（data partition）单元必须在属于后者的片段和数据划分片段之后，
		无论是基本编码图像还是冗余编码图像都必须遵守这个规则。
	5.基本编码图像的所有片段（slice）单元和数据划分片段（data partition）单元必须
		在属于相应冗余编码图像的片段（slice）单元和数据划分片段（data partition）单元之前。
		
	6.如果数据流中出现了连续的无参考基本编码图像，则图像序号小的在前面。
	
	7.如果arbitrary_slice_order_allowed_flag置为1，一个基本编码图像中的片段（slice）单元和数据划分片段（data partition）单元的顺序是任意的，
	  如果arbitrary_slice_order_allowed_flag置为0，则要按照片段中第一个宏块的位置来确定片段的顺序，若使用数据划分，则A类数据划分片段在B类数据划分片段之前，
	  	B类数据划分片段在C类数据划分片段之前，而且对应不同片段的数据划分片段不能相互交叉，也不能与没有数据划分的片段相互交叉。
	8.如果存在SEI（补充增强信息）单元的话，它必须在它所对应的基本编码图像的片段（slice）单元和数据划分片段（data partition）单元之前，
		并同时必须紧接在上一个基本编码图像的所有片段（slice）单元和数据划分片段（data partition）单元后边。
		假如SEI属于多个基本编码图像，其顺序仅以第一个基本编码图像为参照。
	9.如果存在图像分割符的话，它必须在所有SEI 单元、基本编码图像的所有片段slice）单元和数据划分片段（data partition）单元之前，
		并且紧接着上一个基本编码图像那些NAL单元。
	10.如果存在序列结束符，且序列结束符后还有图像，则该图像必须是IDR（即时解码器刷新）图像。序列结束符的位置应当在属于这个IDR图像的分割符、SEI 单元等数据之前，
		且紧接着前面那些图像的NAL单元。如果序列结束符后没有图像了，那么它的就在比特流中所有图像数据之后。
	11.流结束符在比特流中的最后。
*/

/*

一、第一个TS包   这一帧数据(也就是一个PES包),共有119756个字节,一共是637个TS包(119756 / 188 = 637).

1. TS Header : 47 41 01 30 
2. TS Header AdaptationField : 07 10 00 07 24 00 7E 00

3. PES Header : 00 00 01 E0 00 00 
4. PES Optional Header : 80 C0 0A 31 00 39 EE 05 11 00 39 90 81

	//在每一帧的视频帧被打包到pes的时候，其开头一定要加上 00 00 00 01 09 F0 这个NALU AUD. 
5. NALU AUD : 00 00 00 01 09 F0 (固定，无分隔符)

	//(SEI -> 06 & 0x1f == 6)     //Supplementary Enhancement Information //补充增强信息 (可变长)
6. NALU Delimiter : 00 00 00 01 	//NALU 分隔符
7. NALU Unit : 06 00 07 80 D8 31 80 87 0D C0 01 07 00 00 18 00 00 03 00 04 80 00 

	//(SPS -> 27 & 0x1f == 7) (可变长)  (SPS -> 67 & 0x1f == 7)
8. NALU Delimiter : 00 00 00 01 
9. NALU Unit : 27 64 00 28 AC 2B 60 3C 01 13 F2 E0 22 00 00 03 00 02 00 00 03 00 3D C0 80 00 64 30 00 00 64 19 37 BD F0 76 87 0C B8 00 


	//(PPS-> 28 & 0x1f == 8) (可变长)  (PPS-> 68 & 0x1f == 8)
10. NALU Delimiter : 00 00 00 01 
11. NALU Unit : 28 EE 3C B0 


	//(IDR Frame -> 25 & 0x1f == 5)   (IDR Frame -> 65 & 0x1f == 5)
12. NALU Delimiter : 00 00 00 01 
13. NALU Unit : 25 88 80 0E 3F D5 2E 71 35 C8 A5 E1 CE F4 89 B3 F2 CA D2 65 75 33 63 B1 BA B6 33 B0 7B 80 A8 26 D0 77 01 FF 9A CB 85 C7 D1 DC A8 22 E9 BE 10 89 F9 CF 1A BA 6D 12 3D 19 0C 77 33 1B 7C 03 9B 3D F1 FF 02 AB C6 73 8A DB 51 

二、 第二个TS包到本帧倒数第二个TS包:

中间的TS包(第2个-第636个),固定的格式:
TsPacket=TsHeader(4bytes)+TsPayload(184bytes)
TsPacket=TsHeader(4bytes)+TsPayload(184bytes)
唯一变化的就是TsHeader中的字段ContinuityCounter,从0-15循环变化.


三、最后一个TS包 = TS头 + TS自适应字段 + 填充字段 + TS Payload

*/

	/*
		////////////////////////////////////////////////////////////////////////////////////////////
								ts header
		////////////////////////////////////////////////////////////////////////////////////////////
	*/
/*
TS Header ts包固定长度188byte

------------------------------------------------------------------------------------
1. 	Sync_Byte 			  | 8bit |	固定为 0x47
	同步字节    		  |	   |
--------------------------------|------|------------------------------------------------------1byte
2. Transport_Error_Indicator 	  | 1bit |  表明在ts头的adapt域后由一个无用字节，通常都为0，这个字节算在adapt域长度内
	传输错误指示符         	  |	   |
--------------------------------|------|----------------------------------------------
3. Payload_Unit_Start_Indicator | 1bit |	 一个完整的数据包开始时标记为1
	负载单元起始标示符	  |      |	在前4个字节后会有一个调整字节。所以实际数据应该为去除第一个字节后的数据。Data部分去掉第一个字节
--------------------------------|------|----------------------------------------------
4. Transport_Priority 		  | 1bit |	0为低优先级，1为高优先级，通常取0
	传输优先级			  |      |
--------------------------------|------|----------------------------------------------
5. 		Pid			  | 13bit|	pid值(Packet ID号码，唯一的号码对应不同的包)
--------------------------------|------|------------------------------------------------------2byte
6. Transport_Scrambling_Control | 2bit |	00表示未加密
	传输加扰控制		  |	   |
--------------------------------|------|---------------------------------------------
7. Adaptation_Field_Control 	  | 2bit |	‘00’保留；‘01’为无自适应域，仅含有效负载；
	是否包含自适应区		  |	   |			‘10’为仅含自适应域，无有效负载；
					  |	   |  		‘11’为同时带有自适应域和有效负载。
--------------------------------|------|---------------------------------------------
8. Continuity_Counter 		  | 4bit |	 从0-f，起始值不一定取0，但必须是连续的
	递增计数器			  |	   |
---------------------------------------------------------------------------------------------- 1byte
9. AdaptationField 
	1. Adaptation_Field_Length (8bit)			自适应域长度，后面的字节数 (不包含此字段的1byte)
	---------------------------------------------------------------------------------------- 1byte
	2. Discontinue_Indicator  (1bit)			非连续指示符		|
	3. Random_Access_Indicator (1bit)   		随机存取指示符		|
	4. Elementary_Streem_Priority_Indicator(1bit)	基本流优先级指示符	|
	5. 5Flags											|
		1. PCR_Flag (1bit)								|  flags == 0x00 时, 
		2. OPCR_Flag (1bit)								|	自适应字段为填充字段
		3. Splicing_Point_Flag (1bit)							|		取值为 0xFF
		4. Trasport_Private_Data_Flag (1bit)					|		长度为 自适应长度 (Adaptation_Field_Length)
		5. Adaptation_Field_Extension_Flag (1bit)					|
	---------------------------------------------------------------------------------------- 1byte
	6. OptionFileds 						任选字段
		1. PCR (42bit)
		2. OPCR (42bit)
		3. 拼接倒数 (8bit)
		4. 传输专用数据长度 (8bit)
		5. 传输专用数据
		6. 自适应字段扩展长度 (8bit)
		7. 3个标志
		8. 任选字段
			1. ltw 有效标志 (1bit)
			2. ltw 补偿
			3. (2bit)
			4. 分段速率 (22bit)
			5. 拼接类型 (4bit)
			6. DTS_next_au (33bit)
	----------------------------------------------------------------------------------------
	7. Stuffing_bytes						填充字节		flags == 0x00, 0xFF
	----------------------------------------------------------------------------------------
*/
		

        /* prepare packet header */
        q    = buf;

		//同步字节，固定为0x47
        *q++ = 0x47;	

		//pid值(Packet ID号码，唯一的号码对应不同的包)
        val  = ts_st->pid >> 8;	//高5bit pid


		//transport_error_indicator 		传输错误指示符，表明在ts头的adapt域后由一个无用字节，通常都为0，这个字节算在adapt域长度内
		//payload_unit_start_indicator 	负载单元起始标示符，一个完整的数据包开始时标记为1
		//transport_priority 			传输优先级，0为低优先级，1为高优先级，通常取0

		//	传输误差提示符(1bit)  |  有效荷载单元起始指示符(1bit)  |  传输优先级(1bit)
		//	TransportErrorIndicator(1bit) | PayloadUnitStartIndicator(1bit) | TransportPriority(1bit) | pid(高5bit)
		//  8 4 2
        if (is_start)
            val |= 0x40;	//PayloadUnitStartIndicator(1bit) 有效
        *q++      = val;

		//pid 低8bit
        *q++      = ts_st->pid;


		//continuity_counter(4bit)	递增计数器，从0-f，起始值不一定取0，但必须是连续的
		//连续计数器 低4bit
        ts_st->cc = ts_st->cc + 1 & 0xf;


		//transport_scrambling_control 传输加扰控制(2bit)  00表示未加密
		//adaptation_field_control     自适应字段控制
		//是否包含自适应区，‘00’保留；‘01’为无自适应域，仅含有效负载；‘10’为仅含自适应域，无有效负载；‘11’为同时带有自适应域和有效负载。


		//仅有有效负载，无自适应字段
        *q++      = 0x10 | ts_st->cc; // payload indicator + CC	


		//非连续指示符(1bit) | 随机存取指示符(1bit) | 基本流优先级指示符(1bit) |  5个标志(5bit)
		//5个标志(5bit)   === PCRFlag(1bit) | OPCRFlag(1bit) | SplicingPointFlag(1bit) | TrasportPrivateDataFlag(1bit) | AdaptationFieldExtensionFlag(1bit)
        if (ts_st->discontinuity) {
            set_af_flag(buf, 0x80);
            q = get_ts_payload_start(buf);
            ts_st->discontinuity = 0;
        }
        if (key && is_start && pts != AV_NOPTS_VALUE) {
            // set Random Access for key frames
            if (ts_st->pid == ts_st->service->pcr_pid)
                write_pcr = 1;
            set_af_flag(buf, 0x40); //0x40表示不包含PCR
            q = get_ts_payload_start(buf);
        }
        if (write_pcr) {
            set_af_flag(buf, 0x10);  //PCRFlag(1bit)
            q = get_ts_payload_start(buf);
            // add 11, pcr references the last byte of program clock reference base
            if (ts->mux_rate > 1)
                pcr = get_pcr(ts, s->pb);
            else
                pcr = (dts - delay) * 300;

			
            if (dts != AV_NOPTS_VALUE && dts < pcr / 300)
                av_log(s, AV_LOG_WARNING, "dts < pcr, TS is invalid\n");
			
            extend_af(buf, write_pcr_bits(q, pcr));
            q = get_ts_payload_start(buf);
        }



	/*
		////////////////////////////////////////////////////////////////////////////////////////////
								pes header
		////////////////////////////////////////////////////////////////////////////////////////////
	*/

/*
PES Header

Packet_start_code_prefix(3byte)|Stream_id(1byte)|PES_Packet_length(2byte)|Optional_PES_header(length >= 3)|Stuffing bytes()|Data
------------------------------------------------------------------------------------
1. 包起始码前缀(24bit) 	| 			3byte 	0x00 0x00 0x01
------------------------------------------------------------------------------------3byte
2. 流id(8bit) 		| 			1byte
------------------------------------------------------------------------------------1byte
3. PES包长度(16bit) 	| 			2byte		0 值指示PES 包长度既未指示也未限定
				|						-- 该PES 包的有效载荷由来自传输流包中所包含的视频基本流的字节组成
------------------------------------------------------------------------------------2byte
4. 基本流特有信息(3-259byte)  | 
	1. '10' 			| 
	2. PES加扰控制(2bit) 	| 
	3. PES优先级(1bit) 	| 		1byte		--val 0x80 | 0x04   1 0 0 0 - 0 1 0 0
	4. 数据定位指示符(1bit) | 
	5. 版本(1bit) 		| 
	6. 原始的或复制的(1bit) | 
	------------------------------------------------------------------------------1byte
	7. 7个标志位(8bit) 			  	| 	1byte		--flags
		1. PTS_DTS_flags(2bit) 		  	| 	11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS
		2. ESCR_flag(1bit) 		  	|	1表示首部有ESCR字段，0则无此字段
		3. ES_rate_flag(1bit) 		  	|	占位1bit；1表示首部有此字段，0无此字段
		4. DSM_trick_mode_flag(1bit) 	  	| 	占位1bit；1表示有8位的DSM_trick_mode_flag字段，0无此字段
		5. Additional_copy_info_flag(1bit) 	| 	占位1bit；1表示首部有此字段，0表示无此字段；
		6. PES_CRC_flag(1bit) 		  	| 	占位1bit；置1表示PES分组有CRC字段，0无此字段；
		7. PES_extension_flag(1bit) 	  	|	占位1bit；扩展标志位，置1表示有扩展字段，0无此字段；
	------------------------------------------------------------------------------1byte
	8. PES头数据长度(8bit) | 		1byte			--header_len  后面的数据长度
	------------------------------------------------------------------------------1byte
	9. 任选字段 | 
		1. PTS/DTS(33bit) 		| 	PTS和DTS的内容是在这40bit中取33位，方式相同；
			1. start_code：起始码，占位4bit；若PTS_DTS_flags == ‘10’，则说明只有PTS，起始码为0010；
				若PTS_DTS_flags == ‘11’，则PTS和DTS都存在，PTS的起始码为0011，DTS的起始码为0001；(PTS的起始码后2个bit与flag相同)
			2. PTS[32..30]：占位3bit；
			3. marker_bit：占位1bit；
			4. PTS[29..15]：占位15bit；
			5. marker_bit：占位1bit；
			6. PTS[14..0]：占位15bit；
			7. marker_bit：占位1bit；
			-------------------------------------------------------------------------------------------------------
			                     1byte                     |           2byte          |          2byte          |                                                                 
			-------------------------------------------------------------------------------------------------------
			start_code(4bit) |    PTS(3bit)   | marker_bit |  PTS(15bit)  | marker_bit |  PTS(15bit) | marker_bit |
			-----------------|----------------|------------|--------------|------------|-------------|------------|-
			[0] [0] [1] [1]  | [32] [31] [30] |    [1]     | [29] .. [15] |     [1]    | [14] .. [0] |    [1]     |
			--------------------------------------------------------------------------------------------------------
			-------------------------------------------------------------------------------------------------------
			start_code(4bit) |    DTS(3bit)   | marker_bit |  DTS(15bit)  | marker_bit |  DTS(15bit) | marker_bit |
			-----------------|----------------|------------|--------------|------------|-------------|------------|-
			[0] [0] [1] [1]  | [32] [31] [30] |    [1]     | [29] .. [15] |     [1]    | [14] .. [0] |    [1]     |
			--------------------------------------------------------------------------------------------------------
		2. ESCR(48bit) 			|  由33bit的ESCR_base字段和9bit的ESCR_extension字段组成，ESCR_flag == 1时此字段存在；
			1. Reserved：保留字段，		占位2bit；
			2. ESCR_base[32..30]：		占位3bit；
			3. marker_bit：			占位1bit；
			4. ESCR_base[29..15]：		占位15bit；
			5. marker_bit：			占位1bit；
			6. ESCR_base[14..0]：		占位15bit；
			7. marker_bit：			占位1bit；
			8. ESCR_extension：(UI)		占位9bit；周期数，取值范围0~299；循环一次，base+1；
			9. marker_bit：			占位1bit；
		3. ES速率(24bit) 			|  目标解码器接收PES分组字节速率，禁止为0，占位24bit，ES_rate_flag == 1时此字段存在；
			1. marker_bit：	占位1bit；
			2. ES_rate：	占位22bit；
			3. marker_bit：	占位1bit；
		4. DSM特技方式(8bit) 		|  表示哪种trick mode被应用于相应的视频流，占位8个bit，DSM_trick_mode_flag == 1时此字段存在；
								其中trick_mode_control占前3个bit，根据其值后面有5个bit的不同内容；
			1. 如果trick_mode_control == ‘000’，依次字节顺序为：
				1. field_id：占位2bit；
				2. intra_slice_refresh ：占位1bit；
				3. frequency_truncation：占位2bit；
			2. 如果trick_mode_control == ‘001’，依次字节顺序为：
				1. rep_cntrl：占位5bit；
			3. 如果trick_mode_control == ‘010’，依次字节顺序为：
				1. field_id：占位2bit；
				2. Reserved：占位3bit；
			4. 如果trick_mode_control == ‘011’，依次字节顺序为：
				1. field_id：占位2bit；
				2. intra_slice_refresh：占位1bit；
				3. frequency_truncation：占位2bit；
			5. 如果trick_mode_control== ‘100’，依次字节顺序为：
				1. rep_cntrl：占位5bit；
			6. 其他情况，字节顺序为：
				1. reserved ：占位5bit；
		5. 附加的复制信息(8bit) 	|  占8个bit，Additional_copy_info_flag == 1时此字段存在；
			1. marker_bit：占位1bit；
			2. copy info字段：占位7bit；表示和版权相关的私有数据；
		6. 前PES_CRC(16bit) 		|  占位16bit字段，包含CRC值，PES_CRC_flag == 1时此字段存在；
		7. PES扩展				|  PES扩展字段，PES_extension_flag == 1时此字段存在；
			1. 5个标志 | 
				1. PES_private_data_flag：				占位1bit，置1表示有私有数据，0则无；
				2. Pack_header_field_flag：				占位1bit，置1表示有Pack_header_field字段，0则无；
				3. Program_packet_sequence_counter_flag：		占位1bit，置1表示有此字段，0则无；
				4. P-STD_buffer_flag：					占位1bit，置1表示有P-STD_buffer字段，0则无此字段；
				5. Reserved字段：						3个bit；
				6. PES_extension_flag_2：				占位1bit，置1表示有扩展字段，0则无此字段；
			2. 任选字段|
				1. PES专用数据(128bit) |	私有数据内容，占位128bit，PES_private_data_flag == 1时此字段存在； 
				2. 包头字段(8bit) | 		Pack_header_field_flag == 1时此字段存在；字段组成顺序如下：
					1. Pack_field_length字段：(UI)指定后面的field的长度，占位8bit；
				3. 节目表顺序控制(16bit) |   计数器字段，16个bit；当flag字段Program_packet_sequence_counter_flag == 1时此字段存在；字节顺序依次为：
					1. marker_bit：					占位1bit；
					2. packet_sequence_counter字段：(UI)	占位7bit；
					3. marker_bit：					占位1bit；
					4. MPEG1_MPEG2_identifier：			占位1bit；置位1表示此PES包的负载来自MPEG1流，置位0表示此PES包的负载来自PS流；
					5. original_stuff_length：(UI)		占位6bit；表示PES头部填充字节长度；
				4. P-STD缓冲器(16bit) |  表示P-STD_buffer内容，占位16bit；P-STD_buffer_flag == '1'时此字段存在；字节顺序依次为：
					1. '01' 字段：占位2bit；
					2. P-STD_buffer_scale：占位1bit；表示用来解释后面P-STD_buffer_size字段的比例因子；
						如果之前的stream_id表示音频流，则此值应为0，若之前的stream_id表示视频流，则此值应为1，对于其他stream类型，此值可以0或1；
					3. P-STD_buffer_size：占位13bit；无符号整数；大于或等于所有P-STD输入缓冲区大小BSn的最大值；
						若P-STD_buffer_scale == 0，则P-STD_buffer_size以128字节为单位；若P-STD_buffer_scale == 1，则P-STD_buffer_size以1024字节为单位；
				5. PES扩展字段长度(8bit) |  扩展字段的扩展字段；占用N*8个bit，PES_extension_flag_2 == '1'时此字段存在；字节顺序依次为：
					1. marker_bit：占位1bit；
					2. PES_extension_field_length：占位7bit，表示扩展区域的长度；
				6. PES扩展字段数据 | Reserved字段 | 占位8*PES_extension_field_length个bit；
	------------------------------------------------------------------------------
	10.填充字节(0xFF) | 填充字段，固定为0xFF；不能超过32个字节；
------------------------------------------------------------------------------------
5. 可变长数据包(最大65536byte) | PES_packet_data_byte：PES包负载中的数据，即ES原始流数据；
------------------------------------------------------------------------------------


数据定位指示符(1bit): 1 indicates that the PES packet header is immediately followed by the video start code or audio syncword


*/
        if (is_start) {
            int pes_extension = 0;
            int pes_header_stuffing_bytes = 0;

			
            /* write PES header */

			//start_code: 0x00 00 01 (3byte)
            *q++ = 0x00;
            *q++ = 0x00;
            *q++ = 0x01;
			
            is_dvb_subtitle = 0;
            is_dvb_teletext = 0;

			//流id (1byte)
            if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (st->codecpar->codec_id == AV_CODEC_ID_DIRAC)
                    *q++ = 0xfd; //  1 1 1 1 - 1 0 1 1
                else
                    *q++ = 0xe0; //  1 1 1 0 - 0 0 0 0 
            } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                       (st->codecpar->codec_id == AV_CODEC_ID_MP2 ||
                        st->codecpar->codec_id == AV_CODEC_ID_MP3 ||
                        st->codecpar->codec_id == AV_CODEC_ID_AAC)) {
                *q++ = 0xc0;	//   1 1 0 0 - 0 0 0 0
            } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                        st->codecpar->codec_id == AV_CODEC_ID_AC3 &&
                        ts->m2ts_mode) {
                *q++ = 0xfd;	//  1 1 1 1 - 1 0 1 1
            } else if (st->codecpar->codec_type == AVMEDIA_TYPE_DATA &&
                       st->codecpar->codec_id == AV_CODEC_ID_TIMED_ID3) {
                *q++ = 0xbd;	//  1 0 1 1 - 1 0 1 1
            } else if (st->codecpar->codec_type == AVMEDIA_TYPE_DATA) {
                *q++ = stream_id != -1 ? stream_id : 0xfc;	// 1 1 1 1 - 1 1 0 0

                if (stream_id == 0xbd) /* asynchronous KLV */
                    pts = dts = AV_NOPTS_VALUE;
            } else {
                *q++ = 0xbd;
                if (st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                    if (st->codecpar->codec_id == AV_CODEC_ID_DVB_SUBTITLE) {
                        is_dvb_subtitle = 1;
                    } else if (st->codecpar->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
                        is_dvb_teletext = 1;
                    }
                }
            }

			
            header_len = 0;
            flags      = 0;
            if (pts != AV_NOPTS_VALUE) {
                header_len += 5;
                flags      |= 0x80;
            }
            if (dts != AV_NOPTS_VALUE && pts != AV_NOPTS_VALUE && dts != pts) {
                header_len += 5;
                flags      |= 0x40;
            }
            if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                st->codecpar->codec_id == AV_CODEC_ID_DIRAC) {
                /* set PES_extension_flag */
                pes_extension = 1;
                flags        |= 0x01;

                /*	 One byte for PES2 extension flag +
		                 * one byte for extension length +
		                 * one byte for extension id */
                header_len += 3;
            }
            /* for Blu-ray AC3 Audio the PES Extension flag should be as follow
	             * otherwise it will not play sound on blu-ray
	             */
            if (ts->m2ts_mode &&
                st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
                st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                        /* set PES_extension_flag */
                        pes_extension = 1;
                        flags |= 0x01;
                        header_len += 3;
            }
            if (is_dvb_teletext) {
                pes_header_stuffing_bytes = 0x24 - header_len;
                header_len = 0x24;
            }
            len = payload_size + header_len + 3;
            /* 3 extra bytes should be added to DVB subtitle payload: 0x20 0x00 at the beginning and trailing 0xff */
            if (is_dvb_subtitle) {
                len += 3;
                payload_size++;
            }
            if (len > 0xffff)
                len = 0;
            if (ts->omit_video_pes_length && st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                len = 0;
            }

			//PES包长度
            *q++ = len >> 8;
            *q++ = len;
            val  = 0x80;
            /* data alignment indicator is required for subtitle and data streams */
            if (st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE || 
				st->codecpar->codec_type == AVMEDIA_TYPE_DATA)
                val |= 0x04;

			//Optional_PES_header
			//基本流特有信息(>=3byte)
            *q++ = val;
			//7个标志位
            *q++ = flags;
            *q++ = header_len;

			//pts
            if (pts != AV_NOPTS_VALUE) {
                write_pts(q, flags >> 6, pts);
                q += 5;
            }
			//dts
            if (dts != AV_NOPTS_VALUE && pts != AV_NOPTS_VALUE && dts != pts) {
                write_pts(q, 1, dts);
                q += 5;
            }
			
            if (pes_extension && st->codecpar->codec_id == AV_CODEC_ID_DIRAC) {
                flags = 0x01;  /* set PES_extension_flag_2 */
                *q++  = flags;
                *q++  = 0x80 | 0x01; /* marker bit + extension length */
                /* Set the stream ID extension flag bit to 0 and
                 * write the extended stream ID. */
                *q++ = 0x00 | 0x60;
            }
            /* For Blu-ray AC3 Audio Setting extended flags */
          if (ts->m2ts_mode &&
              pes_extension &&
              st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                      flags = 0x01; /* set PES_extension_flag_2 */
                      *q++ = flags;
                      *q++ = 0x80 | 0x01; /* marker bit + extension length */
                      *q++ = 0x00 | 0x71; /* for AC3 Audio (specifically on blue-rays) */
              }


            if (is_dvb_subtitle) {
                /* First two fields of DVB subtitles PES data:
                 * data_identifier: for DVB subtitle streams shall be coded with the value 0x20
                 * subtitle_stream_id: for DVB subtitle stream shall be identified by the value 0x00 */
                *q++ = 0x20;
                *q++ = 0x00;
            }
            if (is_dvb_teletext) {
                memset(q, 0xff, pes_header_stuffing_bytes);
                q += pes_header_stuffing_bytes;
            }
            is_start = 0;
        }
        /* header size */
        header_len = q - buf;
        /* data len */
        len = TS_PACKET_SIZE - header_len;
        if (len > payload_size)
            len = payload_size;
        stuffing_len = TS_PACKET_SIZE - header_len - len;
        if (stuffing_len > 0) {
            /* add stuffing with AFC */
            if (buf[3] & 0x20) {
                /* stuffing already present: increase its size */
                afc_len = buf[4] + 1;
                memmove(buf + 4 + afc_len + stuffing_len,
                        buf + 4 + afc_len,
                        header_len - (4 + afc_len));
                buf[4] += stuffing_len;
                memset(buf + 4 + afc_len, 0xff, stuffing_len);
            } else {
                /* add stuffing */
                memmove(buf + 4 + stuffing_len, buf + 4, header_len - 4);
                buf[3] |= 0x20;
                buf[4]  = stuffing_len - 1;
                if (stuffing_len >= 2) {
                    buf[5] = 0x00;
                    memset(buf + 6, 0xff, stuffing_len - 2);
                }
            }
        }

        if (is_dvb_subtitle && payload_size == len) {
            memcpy(buf + TS_PACKET_SIZE - len, payload, len - 1);
            buf[TS_PACKET_SIZE - 1] = 0xff; /* end_of_PES_data_field_marker: an 8-bit field with fixed contents 0xff for DVB subtitle */
        } else {
            memcpy(buf + TS_PACKET_SIZE - len, payload, len);
        }

        payload      += len;
        payload_size -= len;
        mpegts_prefix_m2ts_header(s);
        avio_write(s->pb, buf, TS_PACKET_SIZE);
    }
    ts_st->prev_payload_key = key;
}

int ff_check_h264_startcode(AVFormatContext *s, const AVStream *st, const AVPacket *pkt)
{
    if (pkt->size < 5 || AV_RB32(pkt->data) != 0x0000001 && AV_RB24(pkt->data) != 0x000001) {
        if (!st->nb_frames) {
            av_log(s, AV_LOG_ERROR, "H.264 bitstream malformed, "
                   "no startcode found, use the video bitstream filter 'h264_mp4toannexb' to fix it "
                   "('-bsf:v h264_mp4toannexb' option with ffmpeg)\n");
            return AVERROR_INVALIDDATA;
        }
        av_log(s, AV_LOG_WARNING, "H.264 bitstream error, startcode missing, size %d", pkt->size);
        if (pkt->size)
            av_log(s, AV_LOG_WARNING, " data %08"PRIX32, AV_RB32(pkt->data));
        av_log(s, AV_LOG_WARNING, "\n");
    }
    return 0;
}

static int check_hevc_startcode(AVFormatContext *s, const AVStream *st, const AVPacket *pkt)
{
    if (pkt->size < 5 || AV_RB32(pkt->data) != 0x0000001 && AV_RB24(pkt->data) != 0x000001) {
        if (!st->nb_frames) {
            av_log(s, AV_LOG_ERROR, "HEVC bitstream malformed, no startcode found\n");
            return AVERROR_PATCHWELCOME;
        }
        av_log(s, AV_LOG_WARNING, "HEVC bitstream error, startcode missing, size %d", pkt->size);
        if (pkt->size)
            av_log(s, AV_LOG_WARNING, " data %08"PRIX32, AV_RB32(pkt->data));
        av_log(s, AV_LOG_WARNING, "\n");
    }
    return 0;
}

/* Based on GStreamer's gst-plugins-base/ext/ogg/gstoggstream.c
 * Released under the LGPL v2.1+, written by
 * Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
 */
static int opus_get_packet_samples(AVFormatContext *s, AVPacket *pkt)
{
    static const int durations[32] = {
      480, 960, 1920, 2880,       /* Silk NB */
      480, 960, 1920, 2880,       /* Silk MB */
      480, 960, 1920, 2880,       /* Silk WB */
      480, 960,                   /* Hybrid SWB */
      480, 960,                   /* Hybrid FB */
      120, 240, 480, 960,         /* CELT NB */
      120, 240, 480, 960,         /* CELT NB */
      120, 240, 480, 960,         /* CELT NB */
      120, 240, 480, 960,         /* CELT NB */
    };
    int toc, frame_duration, nframes, duration;

    if (pkt->size < 1)
        return 0;

    toc = pkt->data[0];

    frame_duration = durations[toc >> 3];
    switch (toc & 3) {
    case 0:
        nframes = 1;
        break;
    case 1:
        nframes = 2;
        break;
    case 2:
        nframes = 2;
        break;
    case 3:
        if (pkt->size < 2)
            return 0;
        nframes = pkt->data[1] & 63;
        break;
    }

    duration = nframes * frame_duration;
    if (duration > 5760) {
        av_log(s, AV_LOG_WARNING,
               "Opus packet duration > 120 ms, invalid");
        return 0;
    }

    return duration;
}

static int mpegts_write_packet_internal(AVFormatContext *s, AVPacket *pkt)
{
    AVStream *st = s->streams[pkt->stream_index];
    int size = pkt->size;
    uint8_t *buf = pkt->data;
    uint8_t *data = NULL;
    MpegTSWrite *ts = s->priv_data;
    MpegTSWriteStream *ts_st = st->priv_data;
    const int64_t delay = av_rescale(s->max_delay, 90000, AV_TIME_BASE) * 2;
    int64_t dts = pkt->dts, pts = pkt->pts;
    int opus_samples = 0;
    int side_data_size;
    char *side_data = NULL;
    int stream_id = -1;

    side_data = av_packet_get_side_data(pkt,
                                        AV_PKT_DATA_MPEGTS_STREAM_ID,
                                        &side_data_size);
    if (side_data)
        stream_id = side_data[0];

    if (ts->reemit_pat_pmt) {
        av_log(s, AV_LOG_WARNING,
               "resend_headers option is deprecated, use -mpegts_flags resend_headers\n");
        ts->reemit_pat_pmt = 0;
        ts->flags         |= MPEGTS_FLAG_REEMIT_PAT_PMT;
    }

    if (ts->flags & MPEGTS_FLAG_REEMIT_PAT_PMT) {
        ts->pat_packet_count = ts->pat_packet_period - 1;
        ts->sdt_packet_count = ts->sdt_packet_period - 1;
        ts->flags           &= ~MPEGTS_FLAG_REEMIT_PAT_PMT;
    }

    if (ts->copyts < 1) {
        if (pts != AV_NOPTS_VALUE)
            pts += delay;
        if (dts != AV_NOPTS_VALUE)
            dts += delay;
    }

    if (ts_st->first_pts_check && pts == AV_NOPTS_VALUE) {
        av_log(s, AV_LOG_ERROR, "first pts value must be set\n");
        return AVERROR_INVALIDDATA;
    }
    ts_st->first_pts_check = 0;

    if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        const uint8_t *p = buf, *buf_end = p + size;
        uint32_t state = -1;
        int extradd = (pkt->flags & AV_PKT_FLAG_KEY) ? st->codecpar->extradata_size : 0;
        int ret = ff_check_h264_startcode(s, st, pkt);
        if (ret < 0)
            return ret;

        if (extradd && AV_RB24(st->codecpar->extradata) > 1)
            extradd = 0;

        do {
            p = avpriv_find_start_code(p, buf_end, &state);
            av_log(s, AV_LOG_TRACE, "nal %"PRId32"\n", state & 0x1f);
            if ((state & 0x1f) == 7)
                extradd = 0;
        } while (p < buf_end && (state & 0x1f) != 9 &&
                 (state & 0x1f) != 5 && (state & 0x1f) != 1);

        if ((state & 0x1f) != 5)
            extradd = 0;
        if ((state & 0x1f) != 9) { // AUD NAL
            data = av_malloc(pkt->size + 6 + extradd);
            if (!data)
                return AVERROR(ENOMEM);
            memcpy(data + 6, st->codecpar->extradata, extradd);
            memcpy(data + 6 + extradd, pkt->data, pkt->size);
            AV_WB32(data, 0x00000001);
            data[4] = 0x09;
            data[5] = 0xf0; // any slice type (0xe) + rbsp stop one bit
            buf     = data;
            size    = pkt->size + 6 + extradd;
        }
    } else if (st->codecpar->codec_id == AV_CODEC_ID_AAC) {
        if (pkt->size < 2) {
            av_log(s, AV_LOG_ERROR, "AAC packet too short\n");
            return AVERROR_INVALIDDATA;
        }
        if ((AV_RB16(pkt->data) & 0xfff0) != 0xfff0) {
            int ret;
            AVPacket pkt2;

            if (!ts_st->amux) {
                av_log(s, AV_LOG_ERROR, "AAC bitstream not in ADTS format "
                                        "and extradata missing\n");
            } else {
            av_init_packet(&pkt2);
            pkt2.data = pkt->data;
            pkt2.size = pkt->size;
            av_assert0(pkt->dts != AV_NOPTS_VALUE);
            pkt2.dts = av_rescale_q(pkt->dts, st->time_base, ts_st->amux->streams[0]->time_base);

            ret = avio_open_dyn_buf(&ts_st->amux->pb);
            if (ret < 0)
                return AVERROR(ENOMEM);

            ret = av_write_frame(ts_st->amux, &pkt2);
            if (ret < 0) {
                ffio_free_dyn_buf(&ts_st->amux->pb);
                return ret;
            }
            size            = avio_close_dyn_buf(ts_st->amux->pb, &data);
            ts_st->amux->pb = NULL;
            buf             = data;
            }
        }
    } else if (st->codecpar->codec_id == AV_CODEC_ID_HEVC) {
        const uint8_t *p = buf, *buf_end = p + size;
        uint32_t state = -1;
        int extradd = (pkt->flags & AV_PKT_FLAG_KEY) ? st->codecpar->extradata_size : 0;
        int ret = check_hevc_startcode(s, st, pkt);
        if (ret < 0)
            return ret;

        if (extradd && AV_RB24(st->codecpar->extradata) > 1)
            extradd = 0;

        do {
            p = avpriv_find_start_code(p, buf_end, &state);
            av_log(s, AV_LOG_TRACE, "nal %"PRId32"\n", (state & 0x7e)>>1);
            if ((state & 0x7e) == 2*32)
                extradd = 0;
        } while (p < buf_end && (state & 0x7e) != 2*35 &&
                 (state & 0x7e) >= 2*32);

        if ((state & 0x7e) < 2*16 && (state & 0x7e) >= 2*24)
            extradd = 0;
        if ((state & 0x7e) != 2*35) { // AUD NAL
            data = av_malloc(pkt->size + 7 + extradd);
            if (!data)
                return AVERROR(ENOMEM);
            memcpy(data + 7, st->codecpar->extradata, extradd);
            memcpy(data + 7 + extradd, pkt->data, pkt->size);
            AV_WB32(data, 0x00000001);
            data[4] = 2*35;
            data[5] = 1;
            data[6] = 0x50; // any slice type (0x4) + rbsp stop one bit
            buf     = data;
            size    = pkt->size + 7 + extradd;
        }
    } else if (st->codecpar->codec_id == AV_CODEC_ID_OPUS) {
        if (pkt->size < 2) {
            av_log(s, AV_LOG_ERROR, "Opus packet too short\n");
            return AVERROR_INVALIDDATA;
        }

        /* Add Opus control header */
        if ((AV_RB16(pkt->data) >> 5) != 0x3ff) {
            uint8_t *side_data;
            int side_data_size;
            int i, n;
            int ctrl_header_size;
            int trim_start = 0, trim_end = 0;

            opus_samples = opus_get_packet_samples(s, pkt);

            side_data = av_packet_get_side_data(pkt,
                                                AV_PKT_DATA_SKIP_SAMPLES,
                                                &side_data_size);

            if (side_data && side_data_size >= 10) {
                trim_end = AV_RL32(side_data + 4) * 48000 / st->codecpar->sample_rate;
            }

            ctrl_header_size = pkt->size + 2 + pkt->size / 255 + 1;
            if (ts_st->opus_pending_trim_start)
              ctrl_header_size += 2;
            if (trim_end)
              ctrl_header_size += 2;

            data = av_malloc(ctrl_header_size);
            if (!data)
                return AVERROR(ENOMEM);

            data[0] = 0x7f;
            data[1] = 0xe0;
            if (ts_st->opus_pending_trim_start)
                data[1] |= 0x10;
            if (trim_end)
                data[1] |= 0x08;

            n = pkt->size;
            i = 2;
            do {
                data[i] = FFMIN(n, 255);
                n -= 255;
                i++;
            } while (n >= 0);

            av_assert0(2 + pkt->size / 255 + 1 == i);

            if (ts_st->opus_pending_trim_start) {
                trim_start = FFMIN(ts_st->opus_pending_trim_start, opus_samples);
                AV_WB16(data + i, trim_start);
                i += 2;
                ts_st->opus_pending_trim_start -= trim_start;
            }
            if (trim_end) {
                trim_end = FFMIN(trim_end, opus_samples - trim_start);
                AV_WB16(data + i, trim_end);
                i += 2;
            }

            memcpy(data + i, pkt->data, pkt->size);
            buf     = data;
            size    = ctrl_header_size;
        } else {
            /* TODO: Can we get TS formatted data here? If so we will
             * need to count the samples of that too! */
            av_log(s, AV_LOG_WARNING, "Got MPEG-TS formatted Opus data, unhandled");
        }
    }

    if (pkt->dts != AV_NOPTS_VALUE) {
        int i;
        for(i=0; i<s->nb_streams; i++) {
            AVStream *st2 = s->streams[i];
            MpegTSWriteStream *ts_st2 = st2->priv_data;
            if (   ts_st2->payload_size
               && (ts_st2->payload_dts == AV_NOPTS_VALUE || dts - ts_st2->payload_dts > delay/2)) {
                mpegts_write_pes(s, st2, ts_st2->payload, ts_st2->payload_size,
                                 ts_st2->payload_pts, ts_st2->payload_dts,
                                 ts_st2->payload_flags & AV_PKT_FLAG_KEY, stream_id);
                ts_st2->payload_size = 0;
            }
        }
    }

    if (ts_st->payload_size && (ts_st->payload_size + size > ts->pes_payload_size ||
        (dts != AV_NOPTS_VALUE && ts_st->payload_dts != AV_NOPTS_VALUE &&
         av_compare_ts(dts - ts_st->payload_dts, st->time_base,
                       s->max_delay, AV_TIME_BASE_Q) >= 0) ||
        ts_st->opus_queued_samples + opus_samples >= 5760 /* 120ms */)) {
        mpegts_write_pes(s, st, ts_st->payload, ts_st->payload_size,
                         ts_st->payload_pts, ts_st->payload_dts,
                         ts_st->payload_flags & AV_PKT_FLAG_KEY, stream_id);
        ts_st->payload_size = 0;
        ts_st->opus_queued_samples = 0;
    }

    if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO || size > ts->pes_payload_size) {
        av_assert0(!ts_st->payload_size);
        // for video and subtitle, write a single pes packet
        mpegts_write_pes(s, st, buf, size, pts, dts,
                         pkt->flags & AV_PKT_FLAG_KEY, stream_id);
        ts_st->opus_queued_samples = 0;
        av_free(data);
        return 0;
    }

    if (!ts_st->payload_size) {
        ts_st->payload_pts   = pts;
        ts_st->payload_dts   = dts;
        ts_st->payload_flags = pkt->flags;
    }

    memcpy(ts_st->payload + ts_st->payload_size, buf, size);
    ts_st->payload_size += size;
    ts_st->opus_queued_samples += opus_samples;

    av_free(data);

    return 0;
}

static void mpegts_write_flush(AVFormatContext *s)
{
    int i;

    /* flush current packets */
    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st = s->streams[i];
        MpegTSWriteStream *ts_st = st->priv_data;
        if (ts_st->payload_size > 0) {
            mpegts_write_pes(s, st, ts_st->payload, ts_st->payload_size,
                             ts_st->payload_pts, ts_st->payload_dts,
                             ts_st->payload_flags & AV_PKT_FLAG_KEY, -1);
            ts_st->payload_size = 0;
            ts_st->opus_queued_samples = 0;
        }
    }
}

static int mpegts_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    if (!pkt) {
        mpegts_write_flush(s);
        return 1;
    } else {
        return mpegts_write_packet_internal(s, pkt);
    }
}

static int mpegts_write_end(AVFormatContext *s)
{
    if (s->pb)
        mpegts_write_flush(s);

    return 0;
}

static void mpegts_deinit(AVFormatContext *s)
{
    MpegTSWrite *ts = s->priv_data;
    MpegTSService *service;
    int i;

    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st = s->streams[i];
        MpegTSWriteStream *ts_st = st->priv_data;
        if (ts_st) {
            av_freep(&ts_st->payload);
            if (ts_st->amux) {
                avformat_free_context(ts_st->amux);
                ts_st->amux = NULL;
            }
        }
    }

    for (i = 0; i < ts->nb_services; i++) {
        service = ts->services[i];
        av_freep(&service->provider_name);
        av_freep(&service->name);
        av_freep(&service);
    }
    av_freep(&ts->services);
}

static int mpegts_check_bitstream(struct AVFormatContext *s, const AVPacket *pkt)
{
    int ret = 1;
    AVStream *st = s->streams[pkt->stream_index];

    if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        if (pkt->size >= 5 && AV_RB32(pkt->data) != 0x0000001 &&
                             (AV_RB24(pkt->data) != 0x000001 ||
                              (st->codecpar->extradata_size > 0 &&
                               st->codecpar->extradata[0] == 1)))
            ret = ff_stream_add_bitstream_filter(st, "h264_mp4toannexb", NULL);
    } else if (st->codecpar->codec_id == AV_CODEC_ID_HEVC) {
        if (pkt->size >= 5 && AV_RB32(pkt->data) != 0x0000001 &&
                             (AV_RB24(pkt->data) != 0x000001 ||
                              (st->codecpar->extradata_size > 0 &&
                               st->codecpar->extradata[0] == 1)))
            ret = ff_stream_add_bitstream_filter(st, "hevc_mp4toannexb", NULL);
    }

    return ret;
}

static const AVOption options[] = {
    { "mpegts_transport_stream_id", "Set transport_stream_id field.",
      offsetof(MpegTSWrite, transport_stream_id), AV_OPT_TYPE_INT,
      { .i64 = 0x0001 }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_original_network_id", "Set original_network_id field.",
      offsetof(MpegTSWrite, original_network_id), AV_OPT_TYPE_INT,
      { .i64 = DVB_PRIVATE_NETWORK_START }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_service_id", "Set service_id field.",
      offsetof(MpegTSWrite, service_id), AV_OPT_TYPE_INT,
      { .i64 = 0x0001 }, 0x0001, 0xffff, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_service_type", "Set service_type field.",
      offsetof(MpegTSWrite, service_type), AV_OPT_TYPE_INT,
      { .i64 = 0x01 }, 0x01, 0xff, AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "digital_tv", "Digital Television.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_DIGITAL_TV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "digital_radio", "Digital Radio.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_DIGITAL_RADIO }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "teletext", "Teletext.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_TELETEXT }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_radio", "Advanced Codec Digital Radio.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_RADIO }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "mpeg2_digital_hdtv", "MPEG2 Digital HDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_MPEG2_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_sdtv", "Advanced Codec Digital SDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_SDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "advanced_codec_digital_hdtv", "Advanced Codec Digital HDTV.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_ADVANCED_CODEC_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "hevc_digital_hdtv", "HEVC Digital Television Service.",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_SERVICE_TYPE_HEVC_DIGITAL_HDTV }, 0x01, 0xff,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_service_type" },
    { "mpegts_pmt_start_pid", "Set the first pid of the PMT.",
      offsetof(MpegTSWrite, pmt_start_pid), AV_OPT_TYPE_INT,
      { .i64 = 0x1000 }, 0x0010, 0x1f00, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_start_pid", "Set the first pid.",
      offsetof(MpegTSWrite, start_pid), AV_OPT_TYPE_INT,
      { .i64 = 0x0100 }, 0x0010, 0x0f00, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_m2ts_mode", "Enable m2ts mode.",
      offsetof(MpegTSWrite, m2ts_mode), AV_OPT_TYPE_BOOL,
      { .i64 = -1 }, -1, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "muxrate", NULL,
      offsetof(MpegTSWrite, mux_rate), AV_OPT_TYPE_INT,
      { .i64 = 1 }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "pes_payload_size", "Minimum PES packet payload in bytes",
      offsetof(MpegTSWrite, pes_payload_size), AV_OPT_TYPE_INT,
      { .i64 = DEFAULT_PES_PAYLOAD_SIZE }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_flags", "MPEG-TS muxing flags",
      offsetof(MpegTSWrite, flags), AV_OPT_TYPE_FLAGS, { .i64 = 0 }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "resend_headers", "Reemit PAT/PMT before writing the next packet",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_REEMIT_PAT_PMT }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "latm", "Use LATM packetization for AAC",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_AAC_LATM }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "pat_pmt_at_frames", "Reemit PAT and PMT at each video frame",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_PAT_PMT_AT_FRAMES}, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "system_b", "Conform to System B (DVB) instead of System A (ATSC)",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_SYSTEM_B }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    { "initial_discontinuity", "Mark initial packets as discontinuous",
      0, AV_OPT_TYPE_CONST, { .i64 = MPEGTS_FLAG_DISCONT }, 0, INT_MAX,
      AV_OPT_FLAG_ENCODING_PARAM, "mpegts_flags" },
    // backward compatibility
    { "resend_headers", "Reemit PAT/PMT before writing the next packet",
      offsetof(MpegTSWrite, reemit_pat_pmt), AV_OPT_TYPE_INT,
      { .i64 = 0 }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "mpegts_copyts", "don't offset dts/pts",
      offsetof(MpegTSWrite, copyts), AV_OPT_TYPE_BOOL,
      { .i64 = -1 }, -1, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "tables_version", "set PAT, PMT and SDT version",
      offsetof(MpegTSWrite, tables_version), AV_OPT_TYPE_INT,
      { .i64 = 0 }, 0, 31, AV_OPT_FLAG_ENCODING_PARAM },
    { "omit_video_pes_length", "Omit the PES packet length for video packets",
      offsetof(MpegTSWrite, omit_video_pes_length), AV_OPT_TYPE_BOOL,
      { .i64 = 1 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "pcr_period", "PCR retransmission time in milliseconds",
      offsetof(MpegTSWrite, pcr_period), AV_OPT_TYPE_INT,
      { .i64 = PCR_RETRANS_TIME }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "pat_period", "PAT/PMT retransmission time limit in seconds",
      offsetof(MpegTSWrite, pat_period), AV_OPT_TYPE_DOUBLE,
      { .dbl = INT_MAX }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "sdt_period", "SDT retransmission time limit in seconds",
      offsetof(MpegTSWrite, sdt_period), AV_OPT_TYPE_DOUBLE,
      { .dbl = INT_MAX }, 0, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL },
};

static const AVClass mpegts_muxer_class = {
    .class_name = "MPEGTS muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_mpegts_muxer = {
    .name           = "mpegts",
    .long_name      = NULL_IF_CONFIG_SMALL("MPEG-TS (MPEG-2 Transport Stream)"),
    .mime_type      = "video/MP2T",
    .extensions     = "ts,m2t,m2ts,mts",
    .priv_data_size = sizeof(MpegTSWrite),
    .audio_codec    = AV_CODEC_ID_MP2,
    .video_codec    = AV_CODEC_ID_MPEG2VIDEO,
    .init           = mpegts_init,
    .write_packet   = mpegts_write_packet,
    .write_trailer  = mpegts_write_end,
    .deinit         = mpegts_deinit,
    .check_bitstream = mpegts_check_bitstream,
    .flags          = AVFMT_ALLOW_FLUSH | AVFMT_VARIABLE_FPS,
    .priv_class     = &mpegts_muxer_class,
};
