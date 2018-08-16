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
	h264_mp4toannexb   -----------------  aac_adtstoasc
	
	h264有两种封装:
		1) 一种是annexb模式，传统模式，有startcode，SPS和PPS是在ES中
		2) 一种是mp4模式，一般mp4 mkv会有，没有startcode，SPS和PPS以及其它信息被封装在container中，每一个frame前面是这个frame的长度

	在ffmpeg中用h264_mp4toannexb_filter可以做转换:
		1) 注册filter, avcbsfc = av_bitstream_filter_init("h264_mp4toannexb");
		2) 转换bitstream
			av_bitstream_filter_filter(AVBitStreamFilterContext *bsfc,
                               AVCodecContext *avctx, const char *args,
                     uint8_t **poutbuf, int *poutbuf_size, const uint8_t *buf, int buf_size, int keyframe)
*/

/*
    nal_unit( NumBytesInNALunit ) {
        forbidden_zero_bit,             //f(1)
*/

/*
    http://yeyingxian.blog.163.com/blog/static/344712420134485613752/
    
    AAC over RTP            < rfc3640 标准 mpeg4-generic >

    +---------+-----------+-----------+---------------+
    | RTP     | AU Header | Auxiliary | Access Unit   |
    | Header  | Section   | Section   | Data Section  |
    +---------+-----------+-----------+---------------+
              |<----------RTP Packet Payload--------->|

    一、 RTP头       : RTP-Header
         RTP-Payload : AU Header Section , Auxiliary Section , Access Unit Data Section

         1) Payload Type (PT)  关于 RTP Payload 的类型 应该定义MPEG4类型

    二、 从MPEG4 Encoder输出的每个Packet ， 被称为 Access Unit , 简写成 AU.
        1) AU Header 就是描述 AU 信息的标准格式。
        2) Auxiliary Section 是用来描述一些辅助信息的, 在RFC中， 这个结构没有被定义，可以由用户来自定义这块内容，也可以完全忽略。
        3) Access Unit Data Section 就是 MPEG Encoder 的输出内容

    三、 The AU Header Section
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+
    |AU-headers-length|AU-header|AU-header|      |AU-header|padding|
    |                 |   (1)   |   (2)   |      |   (n)   | bits  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+

        1) AU-h_length, (2byte)  就是 AU-Header_Length, 表示后面的 (AU-headers) 的长度, 不包括填充, 单位是 bit, 
                                AU-Header_Length == AU-header1+AU-header2+ ... +AU-headern
        2) AU-header, (2byte) (13 bits = length of frame, 3 bits = AU-Index(-delta) ), 每个AU Header 是与 Access Unit( 或者是 AU Fragment) 相对应的

        3) Padding Bits 是为了让 AU Header Section的长度(bits) 是 8的整数倍

    四、 The AU-header
    +---------------------------------------+
    |     AU-size                           |
    +---------------------------------------+
    |     AU-Index / AU-Index-delta         |
    +---------------------------------------+
    |     CTS-flag                          |
    +---------------------------------------+
    |     CTS-delta                         |
    +---------------------------------------+
    |     DTS-flag                          |
    +---------------------------------------+
    |     DTS-delta                         |
    +---------------------------------------+
    |     RAP-flag                          |
    +---------------------------------------+
    |     Stream-state                      |
    +---------------------------------------+

        1) AU-Size : 就是指 与AU Header 对应的那个 Access Unit 的长度 , in octets 
            如果在RTP包中的 Payload 是 AU Fragment， AU-Size 也 应该是完整的 AU 的长度, 不应该是Fragment的长度，
            在接收端， 可以看接受到的 AU 的长度是不是跟 AU Header 中定义的 AU-Size 一致， 来判断这个 AU 是一个完整的单元 还是只是碎片。
            并且可以通过这个字段来判断接受到 Access Unit 是不是完整。

        2) AU-Index : 就是 Access Unit 或者 AU Fragment 的 serial number。相邻的两个AU（Fragment), 应该是AU-Index[n+1] = AU-Index[n] + 1。
            如果一个RTP Packet 中包含多个 Access Unit , 那第一个 AU Header 必须有 AU-Index 字段，
            接下来的几个 AU-Header 则不允许有 AU-Index 字段，而是用 AU-Index-Delta 字段来取代。

        3) AU-Index-Delta ： AU-Index-Delta 是根据上一个 AU-Index 的值来计算当前的 AU-Index。
            计算公式为 AU-Index[n+1] = AU-Index[n] + AU-Index-Delta + 1 , 所以通常情况下 , AU-Index-Delta 应该是零， 
            如果 AU-Index-Delta 的值不是零，则说明在 RTP Packet 封装的时候使用了 Interleave 模式， 关于 Interleave 模式， 我们以后再来解释

        4) CTS-Flag ： 1 表示接下来的数据是CTS ， 0 表示没有CTS
            CTS ： the composition time stamp

        5) DTS-Flag : 1 表示接下来的数据是DTS ， 0 表示没有DTS
            DTS:  the decoding time stamp

        6) RAP-Flag:  1 表示对应的 Access Unit 是一个 Random Access Unit , 也就是所谓的 key-packet
            如果 对应的是 Access Unit Fragment , 那么只有第一个Fragment 的 RAP-Flag 的值是1, 其他 Fragment 的值应该是 0.

        7) Stream-state :  表示编码器状态。如果编码器状态改变，这个值+1。
            RFC中没有明确说明这个状态指那些内容， 我估计应该是resolution , fps, bps 等参数的改变吧


    五、 The Auxiliary Section
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+
    | auxiliary-data-size   | auxiliary-data       |padding bits |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+

    六、 The Access Unit Data Section
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |AU(1)                                                          |
    +                                                               |
    |                                                               |
    |               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |               |AU(2)                                          |
    +-+-+-+-+-+-+-+-+                                               |
    |                                                               |
    |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                               | AU(n)                         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |AU(n) continued|
    |-+-+-+-+-+-+-+-+
    
*/

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
5. section_length			12bit		后面数据的长度, 从 transport_stream_id 到 CRC32 的长度
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
----------------------------------------------------------------------------------------------------1byte	0x02
2. section_syntax_indicator	1bit		固定为1
----------------------------------------------------------------------------------------------------
3. zero				1bit		固定为0
----------------------------------------------------------------------------------------------------
4. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
5. section_length			12bit		后面数据的长度   段长度,从program_number到CRC_32(含)的字节总数
----------------------------------------------------------------------------------------------------2byte	0xb0 0x17
6. program_number			16bit		节目号，表示当前的 PMT 关联到的 节目号，取值0x0001
----------------------------------------------------------------------------------------------------2byte	0x00 0x01
7. reserved				2bit		固定为11
----------------------------------------------------------------------------------------------------
8. version_number			5bit		版本号，固定为00000，如果PAT有变化则版本号加1
							如果PMT内容有更新,则它会递增1通知解复用程序需要重新接收节目信息
----------------------------------------------------------------------------------------------------
9. current_next_indicator	1bit		固定为1
----------------------------------------------------------------------------------------------------1byte	0xc1
10. section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte	0x00
11. last_section_number		8bit		固定为0x00
----------------------------------------------------------------------------------------------------1byte	0x00
12. reserved			3bit		固定为111
----------------------------------------------------------------------------------------------------
13. PCR_PID				13bit		PCR (节目参考时钟) 所在TS分组的PID，指定为视频PID
----------------------------------------------------------------------------------------------------2byte	0xe1 0x00
14. reserved			4bit		固定为1111
----------------------------------------------------------------------------------------------------
15. program_info_length		12bit		节目描述信息，指定为0x000表示没有
							节目信息长度(之后的是N个描述符结构,一般可以忽略掉,这个字段就代表描述符总的长度,单位是Bytes)
							紧接着就是频道内部包含的节目类型和对应的PID号码了
----------------------------------------------------------------------------------------------------2byte	0xf0 0x00
开始循环
====================================================================================================
16. stream_type			8bit		流类型，标志是 Video 还是 Audio 还是其他数据
							h.264 编码对应 0x1b
							aac   编码对应 0x0f
							mp3   编码对应 0x03
----------------------------------------------------------------------------------------------------1byte	0x1b			0x0f
17. reserved			3bit		固定为111
----------------------------------------------------------------------------------------------------
18. elementary_PID		13bit		与 stream_type 对应的 PID
----------------------------------------------------------------------------------------------------2byte	0xe1 0x00		0xe1 0x01
19. reserved			4bit		固定为1111
----------------------------------------------------------------------------------------------------
20. ES_info_length		12bit		描述信息，指定为 0x000 表示没有
====================================================================================================2byte	0xf0 0x00		0xf0 0x00
结束循环
----------------------------------------------------------------------------------------------------
21. CRC32				32bit		前面数据的CRC32校验码
----------------------------------------------------------------------------------------------------4byte	0x2f 0x44 0xb9 0x9b

*/

/*
    RTCP 包结构
    
    0            8            16          24           32
    +------------+------------+------------+------------+
    |V=2|P|  RC  |     PT     |        Length           |
    +------------+------------+------------+------------+
    |                   SSRC of Sender                  |
    +------------+------------+------------+------------+
    |       NTP timestamp, most  significant word       |
    +------------+------------+------------+------------+
    |       NTP timestamp, least significant word       |
    +------------+------------+------------+------------+
    |                   RTP timestamp                   |
    +------------+------------+------------+------------+
    |               Sender's packet count               |
    +------------+------------+------------+------------+
    |               Sender's octet  count               |
    +------------+------------+------------+------------+
    |            SSRC_1 (SSRC of first source)          |
    +------------+------------+------------+------------+
    |Fractionlost|   Cumulative number of packet lost   |
    +------------+------------+------------+------------+
    |    Extended highest sequence number recieved      |
    +------------+------------+------------+------------+
    |               Inter arrival jitter                |
    +------------+------------+------------+------------+
    |                   LastSR (LSR)                    |
    +------------+------------+------------+------------+
    |             Delay since LastSR (DLSR)             |
    +------------+------------+------------+------------+
    |            SSRC_2 (SSRC of second source)         |
    +------------+------------+------------+------------+
    |                       ...                         |
    +------------+------------+------------+------------+
    |           Profile-specific extensions             |
    +------------+------------+------------+------------+

    1. V: (2bits) 版本, 同RTP包头域
    2. P: (1bit)  填充, 同RTP包头域
    3. RC:(5bits) 接收报告计数器, 该SR包中的接收报告块的数目，可以为零
    4. PT:(8bit)  消息类型, (SR=200/RR=201/SDES=202/APP=204)
    4. Length: (16bits) 长度域, 其中存放的是该SR包以32比特为单位的总长度减一
    5. SSRC_of_Sender: (32bits) 同步源, SR包发送者的同步源标识符
    6. NTP_Timestamp: (32*2bits) (Network time protocol) SR包发送时的绝对时间值, NTP的作用是同步不同的RTP媒体流
    7. RTP_Timestamp: (32bits) 与NTP时间戳对应，与RTP数据包中的RTP时间戳具有相同的单位和随机初始值。
    8. Sender's_packet_count: (32bits) 从开始发送包到产生这个SR包这段时间里, 发送者发送的RTP数据包的总数. SSRC改变时, 这个域清零.
    9. Sender's_octet_count: (32bits) 从开始发送包到产生这个SR包这段时间里, 发送者发送的净荷数据的总字节数 (不包括头部和填充). 发送者改变其SSRC时,这个域要清零.
    10.SSRC_n: (32bits) 同步源n的SSRC标识符 该报告块中包含的是从该源接收到的包的统计信息
    11.Fractionlost: (8bits) 丢失率 表明从上一个SR或RR包发出以来从同步源n(SSRC_n)来的RTP数据包的丢失率
    12.Cumulative_number_of_packet_lost: (24bits) 累计的包丢失数目 从开始接收到SSRC_n的包到发送SR,从SSRC_n传过来的RTP数据包的丢失总数
    13.Extended_highest_sequence_number_recieved: (32bits) 收到的扩展最大序列号 从SSRC_n收到的RTP数据包中最大的序列号
    14.Inter_arrival_jitter: (32bits) 接收抖动 RTP数据包接受时间的统计方差估计
    15.LastSR: (32bits) 上次SR时间戳 取最近从SSRC_n收到的SR包中的NTP时间戳的中间32比特。如果目前还没收到SR包，则该域清零
    16.DLSR: (32bits) 上次SR以来的延时 上次从SSRC_n收到SR包到发送本报告的延时


    RTCP(){
        //for SR/RR/SD/APP (4byte)
        version,                //(2bit), 版本
        padding,                //(1bit), 填充
        recieve_count,          //(5bit), 接收报告计数器, 该SR包中的接收报告块的数目, 可以为零.
        packet_type,            //(8bit), 包类型 (SR=200/RR=201/SDES=202/GoodBye=203/APP=204)
        length,                 //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长
        
        ssrc_of_sender,         //(4byte), 此RTCP包发送者的标识符

        //(20bytes) only for SR
        //发送源自身的发送情况报告
        (SR)ntp_timestamp1,         //(32bit), 绝对时间 (MSW) (单位: 秒) 就是从1970年开始至现在的秒值.
        (SR)ntp_timestamp2,         //(32bit), 绝对时间 (LSW) (单位: 1,000,000,000,000/(2^32) = 232.83064365386962890625 picoseconds)
        (SR)rtp_timestamp,          //(32bit), 与RTP数据包中的RTP时间戳具有相同的单位和随机初始值
        (SR)sender's_packet_count,  //(32bit), 从开始发送包到产生这个SR包这段时间里, 发送者发送的RTP数据包的总数. SSRC改变时, 这个域清零.
        (SR)sender's_octet_count,   //(32bit), 从开始发送包到产生这个SR包这段时间里, 发送者发送的净荷数据的总字节数 (不包括头部和填充). 
                                            //发送者改变其SSRC时,这个域要清零.
        //only for RR (vlc) (6 * 4byte)
        //接收端对指定源的接收情况报告
        for(i=0;i<N;++i){
            ssrc_$i,                                    //(4byte), 指定源的SSRC标识符
            fractionlost,                               //(8bit),  丢失率 表明从上一个SR或RR包发出以来从同步源n(SSRC_n)来的RTP数据包的丢失率
            cumulative_number_of_packet_lost,           //(24bit), 累计的包丢失数目 从开始接收到SSRC_n的包到发送SR,从SSRC_n传过来的RTP数据包的丢失总数
            extended_highest_sequence_number_recieved,  //(32bit), 收到的扩展最大序列号 从SSRC_n收到的RTP数据包中最大的序列号
            interarrival_jitter,                        //(32bit), 接收抖动 RTP数据包接受时间的统计方差估计
            last_SR_timestamp,                          //(32bit), 上次SR时间戳 取最近从SSRC_n收到的SR包中的NTP时间戳的中间32比特。
                                                                    //如果目前还没收到SR包，则该域清零
            delay_since_last_SR_timestamp,              //(32bit), 上次SR以来的延时 上次从SSRC_n收到SR包到发送本报告的延时
        }

        profile_specific_extensions,    //(32bit), 
    }
*/

/*
    RTCP --> SR

    Sender_Report(){
        header(){
            version,                    //(2bit), 版本
            padding,                    //(1bit), 填充
            reception_report_count=0,   //(5bit), 接收报告计数器, 该SR包中的接收报告块的数目, 可以为零.
            packet_type,                //(8bit), 包类型 (SR=200)
            length,                     //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长
        }

        ssrc_of_sender,                 //(4byte), 此RTCP包发送者的标识符

        ntp_timestamp1,                 //(4byte)
        ntp_timestamp2,                 //(4byte)
        rtp_timestamp,                  //(4byte)
        sender's_packet_count,          //(4byte)
        sender's_octet_count,           //(4byte)
    }
*/

/*
    RTCP --> RR

    Reciver_Report(){
        header(){
            version,                    //(2bit), 版本
            padding,                    //(1bit), 填充
            reception_report_count=1,   //(5bit), 接收报告计数器, 该SR包中的接收报告块的数目, 可以为零.
            packet_type,                //(8bit), 包类型 (RR=201)
            length,                     //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长
        }

        ssrc_of_sender,                 //(4byte), 此RTCP包发送者的标识符

        Source1(){
            identifier,                 //(4byte), 指定源的SSRC标识符

            SSRC_Contents(){
                fraction_lost,                      //(1byte), 丢失率 表明从上一个SR或RR包发出以来从同步源n(SSRC_n)来的RTP数据包的丢失率
                cumulative_number_of_packet_lost,   //(3byte), 累计的包丢失数目 从开始接收到SSRC_n的包到发送SR,从SSRC_n传过来的RTP数据包的丢失总数
            }

            // extended_highest_sequence_number_recieved == Sequence_number_cycles_count << 16 | Highest_sequence_number_recieved;
            extended_highest_sequence_number_recieved(){
                Sequence_number_cycles_count,               //(2byte), 
                Highest_sequence_number_recieved,           //(2byte), 收到的扩展最大序列号 从SSRC_n收到的RTP数据包中最大的序列号
            }

            interarrival_jitter,                            //(4byte), 接收抖动 RTP数据包接受时间的统计方差估计

            last_SR_timestamp,                              //(4byte), 上次SR时间戳 取最近从 SSRC_n 收到的SR包中的NTP时间戳的中间32比特

            delay_since_last_SR_timestamp,                  //(4byte), 上次SR以来的延时 上次从SSRC_n收到SR包到发送本报告的延时
        }
    }
*/

/*

    RTCP --> SDES (源描述包)
    //接收端自身信息报告消息

          0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
header  | V=2 |P |    SC     |  PT=SDES=202          |                      length                      |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
chunk1  |                                           SSRC_1                                              |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                                                                                               |
        |                                           SDES items                                          |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
chunk2  |                                           SSRC_2                                              |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                                                                                               |
        |                                           SDES items                                          |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

        1. SDES items

        RTCP(){
            header(){
                version,                //(2bit), 版本
                padding,                //(1bit), 填充
                source_count=1,         //(5bit), chunk 的数目, 可以为零.
                packet_type,            //(8bit), 包类型 (SDES=202)
                length,                 //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长
            }
            chunk(){
                SSRC,           //(32bit) SSRC 
                sdes_itmes(){
                    type,       //(8bit) 类型 type==0x01, CNAME
                    Length,     //(8bit)    text_length=15
                    text,       //(length bytes)    text:XTZ-01709051023
                    type,       //(8bit) 类型 type==0x00, END
                }
            }
        }
*/

/*
    RTCP --> APP

    APP(){
        header(){
            version,                //(2bit), 版本
            padding,                //(1bit), 填充
            subtype=0,              //(5bit), 
            packet_type,            //(8bit), 包类型 (Application specific=204)
            length,                 //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长, 4byte为单位
        }

        identifier,                 //(4byte), 此RTCP包发送者的标识符

        Name,                       //(4byte), QTSS

        Application_specific_Data,  //((length-2)*4byte),
    }
*/

/*
    BYE (BYE包)

          0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
header  | V=2 |P |    SC     |  PT=GoodBye=203       |                      length                      |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                                     Identifier (SSRC)                                         |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                            .   .   .   .   .
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
(opt)   |         length        |                         reason for leaving	                           |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

        RTCP(){
            header(){
                version,                //(2bit), 版本
                padding,                //(1bit), 填充
                source_count=1,         //(5bit), SSRC 的数目, 可以为零.
                packet_type,            //(8bit), 包类型 (GoodBye=203)
                length=1,               //(16bit), 长度域, 其中存放的是该SR包以32比特为单位的总长度减一, 该字段后面的数据总长
            }
            SSRC,           //(4byte), 指定要离开的源的标识符, 接收端自身的SSRC
        }
        
*/

/*
    TCP Handshake

    一、 SYN=1,ACK=0, seq=0
    二、 SYN=1,ACK=1, seq=0, ack=1
    三、 SYN=0,ACK=1, seq=1, ack=1


    Fin Handshake

    一、FIN=1,ACK=1, seq=50, ack=100
    二、FIN=0,ACK=1, seq=100, ack=51
    三、FIN=1,ACK=1, seq=100, ack=51 ( 二、三组合为一次ack, FIN=1,ACK=1, seq=100, ack=51 )
    四、FIN=0,ACK=1, seq=51, ack=101
*/

/*
    TCP 数据段格式 (20字节)

    +------------------------------------------------+------------------------------------------------+
    |                源端口号 (16bit)                |              目的端口号 (16bit)     	      |    //4byte
    +------------------------------------------------+------------------------------------------------+
    |                                            顺序号 (32bit)                                       |    //8byte
    +-------------------------------------------------------------------------------------------------+
    |                                            确认号 (32bit)                 	                  |    //12byte
    +------------------|-----------------|-|-|-|-|-|-|------------------------------------------------+
    |                  |                 |U|A|P|R|S|F|                                                |
    |TCP_H_Length(4bit)| reserved (6bit) |R|C|S|S|Y|I|              窗口大小 (16bit)	            |    //16byte
    |                  |                 |G|K|H|T|N|N|                                                |
    +------------------|-----------------|-|-|-|-|-|-|------------------------------------------------+
    |               校验和 (16bit)                   |              紧急指针 (16bit)	            |    //20byte
    +------------------------------------------------+------------------------------------------------+
    |                                           可选项 (8bit 的倍数)                                  |
    +-------------------------------------------------------------------------------------------------+
    |                                                                                                 |
    |                                              数据                                               |
    |                                                                                                 |
    +-------------------------------------------------------------------------------------------------+


    1. 源端口和目的端口
        1) 各占2字节, 端口号加上IP地址, 共同构成socket. 
        2) 互相通信的进程使用一对socket. 包括协议、源IP、源端口、目的IP、目的端口，这五个元素唯一确定一个TCP连接

    2. 顺序号
        1) 占4字节, 是TCP段所发送的数据部分第一个字节的序号.
        2) 在TCP传送的数据流中, 每一个字节都有一个序号. 
        3) 建立连接时, 发送方将初始序号（Initial Sequence Number, ISN）填写到第一个发送的TCP段序号中.

    3. 确认号
        1) 占4字节, 是期望收到对方下次发送的数据的第一个字节的序号, 也就是期望收到的下一个TCP段的首部中的序号. 
        2) 等于已经成功收到的TCP段的最后一个字节序号加1. 
        3) 确认号在 ACK标志 为1时有意义, 除了主动发起连接的第一个 TCP段 不设置 ACK标志 外, 其后发送的 TCP段 都会设置 ACK标志.

    4. TCP_H_Length: 数据偏移
        1) 占4比特, 表示数据开始的地方离 TCP段 的 起始处 有多远. 实际上就是 TCP段 首部的长度. 
        2) 由于首部长度不固定, 因此数据偏移字段是必要的. 
        3) 数据偏移以 (32bit) 为长度单位, 因此TCP首部的最大长度是60(15*4)个字节。

    5. 保留

    6. 控制位
        一共6个，占6比特，设置为1时有效。按顺序依次为：URG、ACK、PSH、RST、SYN、FIN。
        1) URG 紧急位, 为1时, 首部中的紧急指针有效
        2) ACK 确认位, 为1时, 首部中的确认号有效
        3) PSH   推位, 为1时, 要求把数据尽快交给应用程序
        4) RST 复位位, 为1时, 复位连接, 一般在出错或关闭连接时使用
        5) SYN 同步位, 在建立连接时使用, 
                当SYN=1而ACK=0时, 表明这是一个连接请求报文段. 
                对方若同意建立连接, 在发回的报文段中使SYN=1和ACK=1
        6) FIN 结束位, 为1时, 表示发送方完成了数据发送

    7. 窗口大小
        占2字节, 表示报文段发送方期望接收的字节数. 可接收的序号范围是从接收方的确认号开始到确认号加上窗口大小之间的数据.
        对方无需确认,可以发送的最大字节数

    8. 校验和
        校验和 包含了 伪首部、TCP首部和数据, 校验和 是TCP强制要求的, 由发送方计算, 接收方验证.
        伪首部,又称为伪包头(Pseudo Header): 是指在 TCP 的分段或 UDP 的数据报格式中, 
            在数据报首部前面增加
                源IP地址(32bit)+目的IP地址(32bit)+0值(8bit)+IP分组的协议字段(8bit)+TCP或UDP数据报的总长度(16bit) == 12字节, 
            所构成的扩展首部结构. 
            此伪首部是一个临时的结构, 它既不向上也不向下传递, 仅仅只是为了保证可以校验套接字的正确性

    9. 紧急指针
        1) URG标志 为1时,紧急指针有效, 表示数据需要优先处理. 
        2) 紧急指针指出在 TCP段 中的紧急数据的最后一个字节的序号, 使接收方可以知道紧急数据共有多长.

    10. 选项
        最常用的选项是最大段大小 (Maximum Segment Size，MSS), 向对方通知本机可以接收的最大TCP段长度. MSS选项只在建立连接的请求中发送
*/

/*
    UDP 数据报格式 (8字节)

    +------------------+------------------+
    | 源端口号 (16bit) | 目的端口号(16bit)|     //4byte
    +------------------+------------------+
    |   报文长度       |       校验和     |     //8byte
    +-------------------------------------+
    |                                     |
    |                 数据                |
    |                                     |
    +-------------------------------------+

    1. 源端口号: (16bit) 发送方端口号
    2. 目的端口: (16bit) 接收方端口号
    3. 报文长度: (16bit) UDP 用户数据报的总长度, 以字节为单位.
    4. 校验和:   (16bit) 检测 UDP 用户数据报在传输中是否有错, 有错就丢弃.
    5. 数据
        UDP 的数据部分如果不为偶数需要用 0 填补, 就是说, 如果数据长度为奇数, 数据长度加 "1"
*/

/*
    IP 数据包格式 (20字节)

    +-------------+--------------+---------------------------+------------------------------------------------------+
    |Version(4bit)|H_Length(4bit)| Type_of_Server(TOS)(8bit) |                  Total_Length   (16bit)              |    //4byte
    +-------------+--------------+---------------------------+----------+-------------------------------------------+
    |                     重组标识(16bit)                    |标志(3bit)|                片偏移(13bit)              |    //8byte
    +--------------------------------------------------------+----------+-------------------------------------------+
    |    生存时间(TTL)(8bit)     |     上层协议标识(8bit)    |                  头部校验和  (16bit)                 |    //12byte
    +--------------------------------------------------------+------------------------------------------------------+
    |                                           源 IP 地址 (32bit)                                                  |    //16byte
    +---------------------------------------------------------------------------------------------------------------+
    |                                         目的 IP 地址 (32bit)                                                  |    //20byte
    +---------------------------------------------------------------------------------------------------------------+
    |                                                     选项                                                      |
    +---------------------------------------------------------------------------------------------------------------+
    |                                                                                                               |
    |                                                     数据                                                      |
    |                                                                                                               |
    +---------------------------------------------------------------------------------------------------------------+

    1. Version: (4bit) 版本号
    2. H_Length: (4bit) 头长度, 它表示数据包头部包括多少个32位长整型，也就是多少个4字节的数据。无选项则为5
    3. Type_of_Server: (8bit) 服务类型

         0  1  2  3  4  5  6  7
        +--+--+--+--+--+--+--+--+
        | 优先权 | D| T| R| 保留|
        +--+--+--+--+--+--+--+--+

        1) 优先权, 取值(0--7), 数值越大优先权越高, 网络中路由器可以使用优先权进行拥塞控制, 如当网络发生拥塞时可以根据数据报的优先权来决定数据报的取舍.
        2) D (Delay), 短延时位, 取值(0/1)
            D == '1', 数据报请求以短延迟信道传输
            D == '0', 正常延时
        3) T (Throughput), 高吞吐量位,  取值(0/1)
            T == '1', 数据报请求以高吞吐量信道传输
            T == '0', 普通
        4) R (Reliability), 高可靠位, 取值(0/1)
            R == '1', 数据报请求以高可靠性信道传输
            R == '0', 普通

    4. Total_Length: (16bit) IP数据包总长度, 整个IP数据报的长度 (报头区+数据区), 以字节为单位
    
    5. 重组标识: 发送主机赋予的标识，以便接收方进行分片重组。
        1) 同一个分片的 标志值(ID值) 相同，不同的分片的 标识值(ID值) 不同。
        2) 每发送一个数据包他的值也逐渐递增
        3) 即使ID相同，如果目标地址、源地址或协议不同的话，也会被认为不同的分片。
        
    6. 标志3位:
               0    1   2
            +----+----+----+
            |保留|不分|更多|
            +----+----+----+

            0) 保留段位: 0 --> 未使用. 现在必须为0
            1) 不分段位: 0 --> 允许数据报分段, 1 --> 数据报不能分段
            2) 更多段位: 0 --> 数据包后面没有包，该包为最后的包,  1 --> 数据包后面有更多的包

    7. 段偏移量: 与更多段位组合, 帮助接收方组合分段的报文, 以字节为单位.
        1) 由13比特组成, 用来标识被分片的每一个分段相对于原始数据的位置. 第一个分片对应的值为0.
        2) 片偏移以8个字节为偏移单位. 也就是说, 每个分片的长度一定是8字节(64位)的整数倍.
    
    8. TTL (time to live): (8bit) 它指定了数据报可以在网络中传输的最长时间. 实际应用中把生存时间字段设置成了数据报可以经过的最大路由器数
            1) TTL的初始值由源主机设置(通常为32、64、128或256), 
            2) 一旦经过一个处理它的路由器,它的值就减1.
            3) 当该字段为0时,数据报就丢弃,并发送ICMP报文通知源主机.
                因此可以防止进入一个循环回路时,数据报无休止地传输下去。

    9. 上层协议标识: 表明使用该包裹的上层协议，如TCP=6，ICMP=1，UDP=17等。
            -----------+---------+-----------------
            十进制编码 |   协议  |     说明
            -----------+---------+-----------------
                0      |    无   |  保留
            -----------+---------+-----------------
                1      |  ICMP   |  网际控制报文协议
            -----------+---------+-----------------
                2      |  IGMP   |  网际组管理协议
            -----------+---------+-----------------
                3      |  GGP    |  网关-网关协议
            -----------+---------+-----------------
                4      |  无     |  未分配
            -----------+---------+-----------------
                5      |  ST     |  流
            -----------+---------+-----------------
                6      |  TCP    |  传输控制协议
            -----------+---------+-----------------
                8      |  EGB    |  外部网关协议
            -----------+---------+-----------------
                9      |  IGP    |  内部网关协议
            -----------+---------+-----------------
                11     |  NVP    |  网络声音协议
            -----------+---------+-----------------
                17     |  UDP    |  用户数据报协议
            -----------+---------+-----------------

    10. 头部校验和: (16bit)
        原理：发送端首先将检验和字段置0，然后对头部中每16位二进制数进行反码求和的运算，并将结果存在校验和字段中。 
        由于接收方在计算过程中包含了发送方放在头部的校验和，因此，如果头部在传输过程中没有发生任何差错，那么接收方计算的结果应该是全1。

    11. 源IP地址: (32bit), 发送端IP地址
    12. 目的IP地址: (32bit), 目的端IP地址
*/

/*
    MTU (Maximum Transmission Unit) 最大传输单元


    一、应用层  (dns)
    +--------------------+
    |        Data        |
    +--------------------+

    二、传输层 (TCP:数据段 Data Segment, UDP:数据报 Datagram) (icmp)
        1. 添加TCP/UDP头
    +-----------------------------------+
    |         IP_Data(1480byte)         |
    +--------------+--------------------+
    |TCP/UDP Header|        Data        |
    +--------------+--------------------+

    三、网络层 (数据包) (ip, arp)
        1. 添加IP头 (20byte) (20byte + 1480byte = 1500byte) 
    +------------------------------------+
    |      IP数据包(MTU)(1500byte)       |   ==> IP数据包
    +-----------------+------------------+
    |IP_Header(20byte)| IP_Data(1480byte)|
    +-----------------+------------------+

    四、数据链路层 (数据帧)  [MTU ==> (46, 1500)]
        1. 添加帧头,帧尾 (7byte + 1byte + 6byte + 6byte + 2byte + 4byte = 26byte)  (1500byte + 26byte = 1526byte)
        2. 网络抓包获取的数据是去掉帧头(前同步码和帧开始定界符部分)和帧尾的数据(6 + 6 + 2 + 1500 = 1514byte)
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
    |前同步码 (7byte)|帧开始定界符(1byte)|目的MAC地址(6byte)|源MAC地址(6byte)|  上层协议(2byte) |IP数据包(MTU)(1500byte)|CRC(4byte)|
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+

        //上一层协议类型，如0x0800代表上一层是IP协议，0x0806为arp
    
    五、物理层
        1. 传输byte流
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
    |前同步码 (7byte)|帧开始定界符(1byte)|目的MAC地址(6byte)|源MAC地址(6byte)|  上层协议(2byte) |IP数据包(MTU)(1500byte)|CRC(4byte)|
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
*/

/*
1. 网络抽象层单元类型 (NALU)

	NALU 头由一个字节组成, 它的语法如下:
	      +---------------+
	      |0|1|2|3|4|5|6|7|
	      +-+-+-+-+-+-+-+-+
	      |F|NRI|  Type   |
	      +---------------+

	1) F: 1 个比特. forbidden_zero_bit. 在 H.264 规范中规定了这一位必须为 0.
	
	2) NRI: 2 个比特.
		nal_ref_idc. 取 00 ~ 11, 似乎指示这个 NALU 的重要性, 如 00 的 NALU 解码器可以丢弃它而不影响图像的回放. 不过一般情况下不太关心这个属性.
		
	3) Type: 5 个比特.
		nal_unit_type. 这个 NALU 单元的类型. 简述如下:
		0     没有定义
		1-23  NAL单元  单个 NAL 单元包.
		24    STAP-A   单一时间的组合包
		25    STAP-B   单一时间的组合包
		26    MTAP16   多个时间的组合包
		27    MTAP24   多个时间的组合包
		28    FU-A     分片的单元
		29    FU-B     分片的单元
		30-31 没有定义

2. 打包模式
	下面是 RFC3550 中规定的 
	
	1) RTP 头的结构. 	固定部分共12字节
	
		0                   1                   2                   3
		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|V=2|P|X|  CC   |M|     PT      |       sequence number         |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                           timestamp                           |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|           synchronization source (SSRC) identifier            |
		+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
		|            contributing source (CSRC) identifiers             |
		|                             ....                              |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		V:(2bit)Version                                     (RTP协议的版本号)
		P:(1bit)Padding                                     (填充标志)
		X:(1bit)Extension                                   (扩展标志)
		CC:(4bit)Contribution source identifiers count CSRC (CSRC计数器)
		M:(1bit)Marker                                      (对于视频,标记一帧的结束; 对于音频,标记会话的开始)
		PT:(7bit)Payload type                               (封装类型)(RFC2250 建议96 表示PS 封装, 建议97 为MPEG-4, 建议98 为H264)
		SN:(16bit)Sequence number                           (同类型包序列号 递增)
		T:(32bit)Timestamp
		SSRC:(32bit)Synchronization source identifier       (同步信源(SSRC)标识符) 每路流不同 随机生成
		CSRC:(32bit)Contributing source identifiers         (所有信源标识符)(可以有0--15个)
		
		1. 负载类型 Payload type (PT): 7 bits 
		    DynamicRTP-type : 96 -- H264
		    DynamicRTP-type : 97 -- aac
		    
		2. 序列号 Sequence number (SN): 16 bits
		    相同类型荷载的 SN 依次递增
		    同一个包的不同分片的 SN 也依次递增
		    SN -- 对应 rtp 包的序列号
		    
		3. 时间戳 Timestamp: 32 bits 
		    1) 时间戳的单位采用的是采样频率的倒数 (1/90000)(1/44100)
		    2) 在RTP协议中并没有规定时间戳的粒度，这取决于有效载荷的类型。因此RTP的时间戳又称为媒体时间戳，以强调这种时间戳的粒度取决于信号的类型。
		    3) 在一次会话开始时的时间戳初值也是随机选择的
		    4) 即使是没有信号发送时，时间戳的数值也要随时间不断的增加

		4. 同步信源标识符 SSRC : 32bits
		    1) 不同类型的流对应不同的 SSRC, 如: 
		        video-SSRC: 0xEBE8F34F
		        audio-SSRC: 0x347122FF
		    2) SSRC 随机生成

		5. 起始标记 Maker : 1bit
		    1) video:
		        M == '1', 标志一帧视频的 结束
		    2) audio:
		        M == '1', 标志一帧音频的 开始

	2) 负载(Payload)结构 (RTP 荷载 H264裸流)

		RTP Payload 的第一个字节, 可以看出它和 H.264 的 NALU 头结构是一样的.

		+---------------+
		|0|1|2|3|4|5|6|7|
		+-+-+-+-+-+-+-+-+
		|F|NRI|  Type   |
		+---------------+

		Type :
			24    STAP-A   单一时间的组合包
			25    STAP-B   单一时间的组合包
			26    MTAP16   多个时间的组合包
			27    MTAP24   多个时间的组合包
			28    FU-A     分片的单元
			29    FU-B     分片的单元
			30-31 		没有定义

		可能的结构类型分别有:

			1、 单一 NAL 单元模式 
				即一个 RTP 包仅由一个完整的 NALU 组成. 这种情况下 RTP NAL 头类型字段和原始的 H.264的 NALU 头类型字段是一样的.

				1) 对于 NALU 的长度小于 MTU 大小的包, 一般采用单一 NAL 单元模式.

				2) 对于一个原始的 H.264 NALU 单元常由 [Start Code] [NALU Header] [NALU Payload] 三部分组成, 
					[Start Code] 用于标示这是一个 NALU 单元的开始, 必须是 "00 00 00 01" 或 "00 00 01", 
					[NALU Header] 仅一个字节, 
					[NALU Payload] 其后都是 NALU 单元内容.
					
				3) 打包时去除 [Start Code] "00 00 01" 或 "00 00 00 01" 的开始码, 把其他数据封包的 RTP 包即可.

				0                   1                   2                   3
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|F|NRI|  type   |                                               |
				+-+-+-+-+-+-+-+-+                               		    |
				|<-NALU Header->|<--- PayLoad ---                               |
				|               Bytes 2..n of a Single NAL unit                 |
				|                                                               |
				|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			2、 组合封包模式
				即可能是由多个 NAL 单元组成一个 RTP 包. 分别有4种组合方式: 
					STAP-A, 
					STAP-B, 
					MTAP16, 
					MTAP24.
					那么这里的类型值分别是 24, 25, 26 以及 27.

		          单时刻聚合包(STAP)应该用于当聚合在一起的NAL单元共享相同的NALU时刻.
        		          1) STAP-A荷载不包括 decoding order number (DON).
        		          2) STAP-B荷载包含一个16位的无符号解码顺序号(DON) (网络字节序)紧跟至少一个单时刻聚合单元.

        		          3) DON域指定STAP-B传输顺序中第一个NAL单元的DON值. 
        		            对每个后续出现在STAP-B中的NAL单元，它的DON值等于(STAP-B中前一个NAL的DON值+1)%65535, %是取模运算。

				1) STAP-A : 当 NALU 的长度特别小时, 可以把几个 NALU 单元封在一个 RTP 包中.

				0                   1                   2                   3
				 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                          RTP Header                           |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                         NALU 1 Data                           |
				:                                                               :
				+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|               | NALU 2 Size                   | NALU 2 HDR    |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                         NALU 2 Data                           |
				:                                                               :
				|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

					1. STAP-A_NAL_Header, (1byte)
					     0             7
					    +-+-+-+-+-+-+-+-+
					    |F|NRI|  type   |
					    +-+-+-+-+-+-+-+-+

					    1) F 和 NRI 取值 NALU_1_HDR中对应的 F 和 NRI
					    2) type = 24 (STAP-A)
					  
					2. STAP-A_NALU_1_Size, (2byte)(网络字节序) (单位:字节) (Header + Data)
						1) H264_NALU_1, 去除 [Start Code]， H264_NALU_1_Header (1byte), H264_NALU_1_PayLoad()
						2) STAP-A:单时刻聚合单元在RTP荷载中是字节对齐的, 单可以不是32位字边界对齐.

   
					3. STAP-A_NALU_2_Size, (2byte)
						H264_NALU_2, 去除 [Start Code]， H264_NALU_2_Header (1byte), H264_NALU_2_PayLoad()

				----------------------------------------------------------------------------------------------------------------------
				 RTP Header | STAP-A NAL Header(1byte) | H264_NALU Size (2byte) | H264_NALU PayLoad | size(2byte) | payload | ...
				-----------------------------------------------------------------------------------------------------------------------

				2) STAP-B:
				
				 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                          RTP Header                           |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|STAP-B NAL HDR |  decoding order number (DON)  |   NALU 1 Size |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				| NALU 1 Size   |  NALU 1 HDR   |                               |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
				|                         NALU 1 Data                           |
				:                                                               :
				+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|               |          NALU 2 Size          | NALU 2 HDR    |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                         NALU 2 Data                           |
				:                                                               :
				|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

				多时刻聚合包(MTAPs): 
				    1) 多时刻聚合包的NAL单元荷载有16位的无符号解码顺序号基址 decoding order number base (DONB) (网络字节序)
				    2) DONB 必须包含MTAP中NAL单元的第一个NAL的DON的值.
				    3) NAL解码顺序中的第一个NAL单元不必要是封装在MTAP中的第一个NAL单元

				本规范定义两个不同多时刻聚合单元.
				    1) size, 两个都有16位的无符号大小信息用于后续NAL单元(网络字节序)
				    2) DOND, 一个8位无符号解码序号差值(DOND),
				    3) Timestamp_Offset, n位 (网络字节序) 时戳位移(TS 位移)用于本NAL单元, n:16 ==> MTAP16, n:24 ==> MTAP24
				        时戳位移越大, MTAP的灵活性越大, 但是负担也越大.

				3) MTAP16:
				 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                          RTP Header                           |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|MTAP16 NAL HDR |  decoding order number (DON)  |   NALU 1 Size |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				| NALU 1 Size   |  NALU 1 DOND  |        NALU1 TS offset        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|   NALU 1 HDR  |                                               |
				+-+-+-+-+-+-+-+-+                                               |
				|                         NALU 1 Data                           |
				:                                                               :
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|          NALU 2 Size          |  NALU 2 DOND  |NALU2 TS offset|
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|NALU2 TS offset|   NALU 2 HDR  |                               |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
				|                         NALU 2 Data                           |
				:                                                               :
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

				4) MTAP24:
				 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                          RTP Header                           |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|MTAP24 NAL HDR |  decoding order number (DON)  |   NALU 1 Size |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				| NALU 1 Size   |  NALU 1 DOND  |        NALU1 TS offset        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|NALU1 TS offset|   NALU 1 HDR  |                               |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
				|                         NALU 1 Data                           |
				:                                                               :
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|          NALU 2 Size          |  NALU 2 DOND  |NALU2 TS offset|
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|         NALU2 TS offset       |   NALU 2 HDR  |               |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
				|                         NALU 2 Data                           |
				:                                                               :
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			3、 分片封包模式
				用于把一个 NALU 单元封装成多个 RTP 包. 存在两种类型 FU-A 和 FU-B. 类型值分别是 28 和 29.

				1) 当 NALU 的长度超过 MTU 时, 就必须对 NALU 单元进行分片封包. 也称为 Fragmentation Units (FUs).

					RealDataLen = MTU(1500) - IP数据报首部(20) - UDP数据报首部(8) = 1472

					RealDataLen = MTU(1500) - IP数据报首部(20) - UDP数据报首部(8) - RTP固定头(12) = 1460

				0                   1                   2                   3
				0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				| FU indicator  |   FU header   |                               |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
				|                                                               |
				|                         FU payload                            |
				|                                                               |
				|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
				|                               :...OPTIONAL RTP padding        |
				+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

				1、 The FU indicator octet has the following format:
					+---------------+
					|0|1|2|3|4|5|6|7|
					+-+-+-+-+-+-+-+-+
					|F|NRI|  Type   | -->FU-Type
					+---------------+
				2、 The FU header has the following format:
					+---------------+
					|0|1|2|3|4|5|6|7|
					+-+-+-+-+-+-+-+-+
					|S|E|R|  Type   | -->H264-Nal-type
					+---------------+

					S: 1 bit 当设置成1,开始位指示分片NAL单元的开始。当跟随的FU荷载不是分片NAL单元荷载的开始，开始位设为0。

					E: 1 bit 当设置成1, 结束位指示分片NAL单元的结束，即, 荷载的最后字节也是分片NAL单元的最后一个字节。
						当跟随的 FU 荷载不是分片NAL单元的最后分片,结束位设置为0。

					R: 1 bit 保留位必须设置为0，接收者必须忽略该位

					打包时，原始的 NAL 头的前三位为 FU indicator 的前三位，原始的 NAL 头的后五位为 FU header 的后五位。

		            fu_indicator = buf[0];
                        fu_header    = buf[1];
                        start_bit    = fu_header >> 7;
                        nal_type     = fu_header & 0x1f;
                        nal          = fu_indicator & 0xe0 | nal_type;
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
	H264NT_SLICE, 		//P 帧		//slice_layer_without_partitioning
	H264NT_SLICE_DPA,				//slice_data_partition_a_layer
	H264NT_SLICE_DPB,				//slice_data_partition_b_layer
	H264NT_SLICE_DPC,				//slice_data_partition_c_layer
	H264NT_SLICE_IDR, 	// I 帧		//slice_layer_without_partitioning
	H264NT_SEI,
	H264NT_SPS,						//seq_parameter_set
	H264NT_PPS,						//pic_parameter_set
	H264NT_AU_DELIMITER, 			//access_unit_delimiter
	H264NT_EOSEQUENCE,				//end_of_sequence
	H264NT_EOSTREAM,				//end_of_stream
	H264NT_FILLER_DATA,				//filler_data
	H264NT_SPSE = 13,				//seq_parameter_set_extension
	... 14-18 reserved,
	... 24-31 unused
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
	1.VCL   video coding layer    	     视频编码层
	2.NAL   network abstraction layer   网络提取层

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
		|   禁止位(1bit) 	| 		 参考级别(2bit) 		|	 					NALU 单元类型(5bit)						|
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
			9		|	分界符 (AUD)					|	0
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

	(Frame-> 67 & 0x1f == 7)	=====> SPS
	(Frame-> 68 & 0x1f == 8)	=====> PPS
	(Frame-> 65 & 0x1f == 5)	=====> IDR
	(Frame-> 09 & 0x1f == 9)	=====> AUD
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



三、  NALU 的顺序要求
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

	//(AUD -> 09 & 0x1f == 9)   //在每一帧的视频帧被打包到pes的时候，其开头一定要加上 00 00 00 01 09 F0 这个NALU AUD. 
5. NALU AUD : 00 00 00 01 09 F0 (必选) 分界符

	//(SEI -> 06 & 0x1f == 6)     //Supplementary Enhancement Information //补充增强信息 (可变长)
6. NALU Delimiter : 00 00 00 01 	//NALU 分隔符
7. NALU Unit : 06 00 07 80 D8 31 80 87 0D C0 01 07 00 00 18 00 00 03 00 04 80 00 

	//(SPS -> 27 & 0x1f == 7) (可变长)  (SPS -> 67 & 0x1f == 7)
8. NALU Delimiter : 00 00 00 01 
9. NALU Unit : 27 64 00 28 AC 2B 60 3C 01 13 F2 E0 22 00 00 03 00 02 00 00 03 00 3D C0 80 00 64 30 00 00 64 19 37 BD F0 76 87 0C B8 00 


	//(PPS-> 28 & 0x1f == 8) (可变长)  (PPS-> 68 & 0x1f == 8)
10. NALU Delimiter : 00 00 00 01 
11. NALU Unit : 28 EE 3C B0 


	//(IDR Frame -> 41 & 0x1f == 1)   i 帧

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

    一、图片编码后 ( 一帧包含一片或多片 )

                               +------切片1
                               |
                               |
                H264编码 ==>   |
    一帧图像 ------------------+------切片2
                               |
                               |
                               |
                               |------切片3

    1) 片的主要作用是用作 宏块（Macroblock）的载体
    2) 片之所以被创造出来，主要目的是为限制误码的扩散和传输
    3) 每个片（slice）都应该是互相独立被传输的
    4) 某片的预测（片（slice）内预测和片（slice）间预测）不能以其它片中的宏块（Macroblock）为参考图像

    二、NALU ( 一个NALU 对应一个切片 )

        +---------+-----------+
        | NALU 头 |  一个切片 |
        +---------+-----------+
                 /             \
                /               \
               /                 \
              +--------+----------+
              | 切片头 | 切片数据 |
              +--------+----------+
                      /            \
                     /              \
                    /                \
                   +-------------------+
                   |宏块|宏块|宏块|宏块|
                   +-------------------+

    1) 每一个分片(Slice)包含 整数个 宏块(Macroblock), 每片（slice）至少一个 宏块(Macroblock), 最多时每片包 整个图像的宏块
    2) 每个分片也包含着 头和 数据 两部分
        1. 分片头中包含着分片类型、分片中的宏块类型、分片帧的数量、分片属于那个图像以及对应的帧的设置和参数等信息
        2. 分片数据中则是宏块，这里就是我们要找的存储像素数据的地方


    三、宏块
        
        +----------+----------+-------+----+----------+
        | 宏块类型 | 预测类型 | C P B | QP | 宏块数据 |
        +----------+----------+-------+----+----------+
                                          /            \
                                         /              \
                                        /                \
                                       +--------+----+----+
                                       |   Y    | Cb | CR |
                                       +--------+----+----+

    
    1) 宏块是视频信息的主要承载者，因为它包含着每一个像素的亮度和色度信息
    2) 视频解码最主要的工作则是提供高效的方式从码流中获得宏块中的像素阵列
    3) 一个宏块由一个16×16亮度像素和附加的一个8×8 Cb和一个 8×8 Cr 彩色像素块组成。每个图象中，若干宏块被排列成片的形式
*/


/*
    一、 描述子是指从比特流提取句法元素的方法，即句法元素的解码算法
            由于 H.264 编码的最后一步是熵编码，所以这里的描述子大多是熵编码的解码算法

    H.264定义了如下几种描述子 :
        a)  ae(v) 基于上下文自适应的二进制 算术熵编码
        b)  b(8) 读进连续的 8 个比特
        c)  ce(v) 基于上下文自适应的 可变长熵编码
        d)  f(n) 读进连续的 n 个比特
        e)  i(n)/i(v) 读进连续的若干比特，并把它们解释为有符号整数
        f)  me(v) 映射指数 Golomb 熵编码
        g)  se(v) 有符号指数 Golomb 熵编码
        h)  te(v) 截断指数 Golomb 熵编码
        i)  u(n)/u(v) 读进连续的若干比特，并将它们解释为无符号整数
        j)  ue(v)  无符号指数 Golomb 熵编码
    
    1. ue(v)：无符号整数指数哥伦布码编码的语法元素，左位在先
    2. se(v)：有符号整数指数哥伦布码编码的语法元素，左位在先
    3. u(n)：n位无符号整数
    4. 在语法表中, 如果n是'v', 其比特数由其它语法元素值确定. 解析过程由函 数 read_bits(n) 的返回值规定, 该返回值用最高有效位在前的二进制表示
*/

/*
    NAL_Unit( NumBytesInNALunit ) {
        forbidden_zero_bit,                 //f(1)
        nal_ref_idc,                        //u(2)
        nal_unit_type,                      //u(5)

        NumBytesInRBSP = 0,

        for( i = 1; i < NumBytesInNALunit; i++ ) {
            if( i + 2 < NumBytesInNALunit && next_bits( 24 ) = = 0x000003 ) {
                rbsp_byte[ NumBytesInRBSP++ ],                                      //b(8)
                rbsp_byte[ NumBytesInRBSP++ ],                                      //b(8)
                i += 2,
                emulation_prevention_three_byte, // equal to 0x03               	//f(8)
            } else 
                rbsp_byte[ NumBytesInRBSP++ ],                                      //b(8)
        }
    }

    //SPS
    Seq_Parameter_Set_rbsp(){
    
        profile_idc,                                //u(8) 标识当前 H.264 码流的 profile
                                                            //66 -- baseline    profile
                                                            //77 -- main        profile
                                                            //88 -- extended    profile
        
        constraint_set0_flag,                       //u(1)
        constraint_set1_flag,                       //u(1)
        constraint_set2_flag,                       //u(1)
        reserved_zero_5bits,                        //u(5) equal to 0

        level_idc,                                  //u(8) 标识当前码流的 Level, 编码的Level定义了某种条件下的最大视频分辨率、最大视频帧率等参数，码流所遵从的 level由 level_idc 指定
        
        seq_parameter_set_id,                       //ue(v) 表示当前的序列参数集的id。通过该id值，图像参数集 pps 可以引用其代表的 sps 中的参数
        
        log2_max_frame_num_minus4,                  //ue(v) 用于计算 MaxFrameNum 的值, 计算公式为 MaxFrameNum = 2^( log2_max_frame_num_minus4 + 4 )
                                                            // MaxFrameNum 是 frame_num 的上限值，frame_num 是图像序号的一种表示方法，
                                                            // 在帧间编码中常用作一种参考帧标记的手段
        
        pic_order_cnt_type,                         //ue(v) 表示解码 picture order count(POC) 的方法
                                                            // POC是另一种计量图像序号的方式，与frame_num 有着不同的计算方法。该语法元素的取值为0、1或2

        if ( pic_order_cnt_type == 0 ) {
            log2_max_pic_order_cnt_lsb_minus4,      //ue(v) 用于计算 MaxPicOrderCntLsb 的值，该值表示 POC 的上限。
                                                        // 计算方法为 MaxPicOrderCntLsb = 2^(log2_max_pic_order_cnt_lsb_minus4 + 4)
        } else if ( pic_order_cnt_type == 1 ) {
            delta_pic_order_always_zero_flag,       //u(1)
            offset_for_non_ref_pic,                 //se(v)
            offset_for_top_to_bottom_field,         //se(v)
            num_ref_frames_in_pic_order_cnt_cycle,  //ue(v)

            for ( i=0; i<num_ref_frames_in_pic_order_cnt_cycle; ++i ) {
                offset_for_ref_frame[i],            //se(v)
            }
        }

        num_ref_frames,                             //ue(v) 用于表示参考帧的最大数目
        
        gaps_in_frame_num_value_allowed_flag,       //u(1) 标识位，说明 frame_num 中是否允许不连续的值
        
        pic_width_in_mbs_minus1,                    //ue(v) 用于计算图像的宽度。单位为宏块个数，因此图像的实际宽度为 : 
                                                            // frame_width = 16 × (pic_width_in_mbs_minus1 + 1)
                                                            
        pic_height_in_map_units_minus1,             //ue(v) 使用 PicHeightInMapUnits 来度量视频中一帧图像的高度。
                                                            // PicHeightInMapUnits 并非图像明确的以像素或宏块为单位的高度，而需要考虑该宏块是帧编码或场编码。
                                                            // PicHeightInMapUnits 的计算方式为：
                                                            // PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1
                                                            
        frame_mbs_only_flag,                        //u(1) 标识位，说明宏块的编码方式。
                                                            // 该标识位为 0时，宏块可能为帧编码或场编码；
                                                            // 该标识位为 1时，所有宏块都采用帧编码。
                                                            // 根据该标识位取值不同，PicHeightInMapUnits 的含义也不同，
                                                            // PicHeightInMapUnits == 0时 表示一场数据按宏块计算的高度，
                                                            // PicHeightInMapUnits == 1时 表示一帧数据按宏块计算的高度
                                                            // 按照宏块计算的图像实际高度 FrameHeightInMbs 的计算方法为：
                                                            // FrameHeightInMbs = ( 2 - frame_mbs_only_flag ) * PicHeightInMapUnits

        if( frame_mbs_only_flag ) {
            mb_adaptive_frame_field_flag,           //u(1) 标识位，说明是否采用了宏块级的帧场自适应编码。
                                                            // 当标识位为0时，不存在帧编码和场编码之间的切换；
                                                            // 当标识位为1时，宏块可能在帧编码和场编码模式之间进行选择
        }

        direct_8x8_inference_flag,                  //u(1) 标识位，用于B_Skip、B_Direct模式运动矢量的推导计算
        
        frame_cropping_flag,                        //u(1) 标识位，说明是否需要对输出的图像帧进行裁剪

        if ( frame_cropping_flag ) {
            frame_crop_left_offset,                 //ue(v)
            frame_crop_right_offset,                //ue(v)
            frame_crop_top_offset,                  //ue(v)
            frame_crop_bottom_offset,               //ue(v)
        }

        vui_parameters_present_flag,                //u(1) 标识位，说明 SPS 中是否存在 VUI 信息

        if ( vui_parameters_present_flag ) {
            vui_parameters(),                       //
        }

        rbsp_trailing_bits(),                       //
    }
*/

/*
    PPS
    Pic_Parameter_Set_rbsp () {
    
        pic_parameter_set_id,                       //ue(v) 表示当前 PPS 的 id, 某个PPS在码流中会被相应的 slice 引用，
                                                            // slice 引用 PPS 的方式就是在 Slice header 中保存 PPS 的 id 值。该值的取值范围为[0,255]。
                                                            
        seq_parameter_set_id,                       //ue(v) 表示当前 PPS 所引用的激活的 SPS 的 id。通过这种方式，PPS 中也可以取到对应 SPS 中的参数。
                                                            // 该值的取值范围为[0,31]。
                                                            
        entropy_coding_mode_flag,                   //u(1) 熵编码模式标识，该标识位表示码流中熵编码/解码选择的算法。
                                                            // 对于部分语法元素，在不同的编码配置下，选择的熵编码方式不同。
                                                            // 例如在一个宏块语法载中，宏块类型 m b_type 的语法元素描述符为“ue(v) | ae(v)”，
                                                            // 在 baseline profile 等设置下采用指数哥伦布编码，
                                                            // 在 main profile 等设置下采用 CABAC 编码。
                                                            // 标识位 entropy_coding_mode_flag 的作用就是控制这种算法选择。
                                                            // 当该值为 0 时，选择左边的算法，通常为指数哥伦布编码或者 CAVLC；
                                                            // 当该值为 1 时，选择右边的算法，通常为 CABAC。
                                                            
        pic_order_present_flag,                     //u(1) 标识位，用于表示另外条带头中的两个语法元素 delta_pic_order_cnt_bottom 和 delta_pic_order_cn 是否存在的标识。
                                                            // 这两个语法元素表示了某一帧的底场的 POC 的计算方法。
                                                            
        num_slice_groups_minus1,                    //ue(v) 表示某一帧中 slice group 的个数。
                                                            // 当该值为 0 时，一帧中所有的 slice 都属于一个slice group。slice group 是一帧中宏块的组合方式，定义在协议文档的3.141部分。

        if ( num_slice_groups_minus1 > 0 ) {
            slice_group_map_type,                   //ue(v)

            if ( slice_group_map_type == 0 ) {
                for( iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                    run_length_minus1[ iGroup ],    //ue(v)
                }
            } else if ( slice_group_map_type == 2 ) {
                for( iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                    top_left[ iGroup ],             //ue(v)
                    bottom_right[ iGroup ],         //ue(v)
                }
            } else if ( slice_group_map_type == 3 ||
                        slice_group_map_type == 4 ||
                        slice_group_map_type == 5 ) {
                slice_group_change_direction_flag,  //u(1)
                slice_group_change_rate_minus1,     //ue(v)
            } else if ( slice_group_map_type == 6 ) {
                pic_size_in_map_units_minus1,       //ue(v)

                for( i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                    slice_group_id[ i ],            //u(v)
                }
            }
        }

        num_ref_idx_l0_active_minus1,               //ue(v) 表示当 Slice Header 中的 num_ref_idx_active_override_flag 标识位为 0 时，
                                                            // P/SP/B slice 的语法元素 num_ref_idx_l0_active_minus1 和 num_ref_idx_l1_active_minus1 的默认值
        num_ref_idx_l1_active_minus1,               //ue(v)

        weighted_pred_flag,                         //u(1) 标识位，表示在P/SP slice 中是否开启加权预测
        
        weighted_bipred_idc,                        //u(2) 表示在 B Slice 中加权预测的方法，取值范围为[0,2]。
                                                            // 0 表示默认加权预测，1 表示显式加权预测，2 表示隐式加权预测。

        pic_init_qp_minus26,                        //se(v) relative to 26 表示初始的量化参数。
                                                            // 实际的量化参数由该参数、slice header 中的 slice_qp_delta/slice_qs_delta 计算得到。
        pic_init_qs_minus26,                        //se(v) relative to 26
        
        chroma_qp_index_offset,                     //se(v) 用于计算色度分量的量化参数，取值范围为[-12,12]。

        deblocking_filter_control_present_flag,     //u(1) 标识位，用于表示 Slice header 中是否存在用于去块滤波器控制的信息。
                                                            // 当该标志位为 1 时，slice header 中包含去块滤波相应的信息；
                                                            // 当该标识位为 0 时，slice header 中没有相应的信息。
                                                            
        constrained_intra_pred_flag,                //u(1) 若该标识为 1，表示 I 宏块在进行帧内预测时只能使用来自 I 和 SI 类型宏块的信息；
                                                         //若该标识位 0，表示 I 宏块可以使用来自 Inter 类型宏块的信息。
                                                         
        redundant_pic_cnt_present_flag,             //u(1) 标识位，用于表示 Slice header 中是否存在 redundant_pic_cnt 语法元素。
                                                        // 当该标志位为 1 时，slice header 中包含 redundant_pic_cnt；
                                                        // 当该标识位为 0 时，slice header 中没有相应的信息。

        rbsp_trailing_bits(),                       //
    }
*/


/*
    片层句法(不分区)

    slice_layer_without_partitioning_rbsp () {
        slice_header(),
        slice_data(),                   // all categories of slice_data( ) syntax
        rbsp_slice_trailing_bits(),
    }
*/

/*
    片层 A 分区句法

    slice_data_partition_A_layer_rbsp () {
        slice_header(),
        slice_id,                       //ue(v)
        slice_data(),                   // only category 2 parts of slice_data( ) syntax
        rbsp_slice_trailing_bits(), 
    }
*/

/*
    片层 B 分区句法

    slice_data_partition_B_layer_rbsp () {
        slice_id,                       //ue(v)

        if ( redundant_pic_cnt_present_flag ) {
            redundant_pic_cnt,          //ue(v)
        }

        slice_data(),                   //  only category 3 parts of slice_data( ) syntax
        rbsp_slice_trailing_bits(),
    }
*/

/*
    片层 C 分区句法

    slice_data_partition_C_layer_rbsp () {
        slice_id,                       //ue(v)

        if ( redundant_pic_cnt_present_flag ) {
            redundant_pic_cnt,          //ue(v)
        }

        slice_data(),                   // only category 4 parts of slice_data( ) syntax
        rbsp_slice_trailing_bits(),
    }
*/

/*
    拖尾（trailing bits）句法

    rbsp_trailing_bits () {
        rbsp_stop_one_bit,              //f(1) equal to 1

        while ( !byte_aligned() ) {
            rbsp_alignment_zero_bit,    //f(1) equal to 0
        }
    }
*/

/*
    slice_header () {
        first_mb_in_slice,                          // ue(v)
        slice_type,                                 // ue(v)
        pic_parameter_set_id,                       // ue(v)
        frame_num,                                  // u(v)

        if ( !frame_mbs_only_flag ) {
            field_pic_flag,                         // u(1)

            if ( field_pic_flag ) {
                bottom_field_flag,                  // u(1)
            }
        }

        if ( nal_unit_type == 5 ) {
            idr_pic_id,                             // ue(v)
        }

        if ( pic_order_cnt_type == 0 ) {
            pic_order_cnt_lsb,                      //u(v)

            if ( pic_order_present_flag && !field_pic_flag ) {
                delta_pic_order_cnt_bottom,         //se(v)
            }
        }

        if ( pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag ) {
            delta_pic_order_cnt[ 0 ],               //se(v)

            if ( pic_order_present_flag && !field_pic_flag ) {
                delta_pic_order_cnt[ 1 ],           //se(v)
            }
        }

        if ( redundant_pic_cnt_present_flag ) {
            redundant_pic_cnt,                      //ue(v)
        }

        if ( slice_type == B ) {
            direct_spatial_mv_pred_flag,            //u(1)
        }

        if ( slice_type == P || slice_type == SP || slice_type == B ) {
            num_ref_idx_active_override_flag,       //u(1)

            if ( num_ref_idx_active_override_flag ) {
                num_ref_idx_l0_active_minus1,       //ue(v)

                if ( slice_type == B ) {
                    num_ref_idx_l1_active_minus1,   //ue(v)
                }
            }
        }

        ref_pic_list_reordering () {
            if ( (weighted_pred_flag && ( slice_type == P || slice_type == SP )) || 
                 ( weighted_bipred_idc == 1 && slice_type == B ) ) {
                pred_weight_table(),
            }

            if ( nal_ref_idc != 0 ) {
                dec_ref_pic_marking(),
            }

            if ( entropy_coding_mode_flag && slice_type != I && slice_type != SI ) {
                cabac_init_idc,                     //ue(v)
            }

            slice_qp_delta,                         //se(v)

            if ( slice_type == SP || slice_type == SI ) {
                if ( slice_type == SP ) {
                    sp_for_switch_flag,             //u(1)
                }

                slice_qs_delta,                     //se(v)
            }

            if ( deblocking_filter_control_present_flag ) {
                disable_deblocking_filter_idc,      //ue(v)

                if ( disable_deblocking_filter_idc != 1 ) {
                    slice_alpha_c0_offset_div2,     //se(v)
                    slice_beta_offset_div2,         //se(v)
                }
            }

            if ( num_slice_groups_minus1 > 0 && slice_group_map_type >= 3 && slice_group_map_type <= 5 ) {
                slice_group_change_cycle,           //u(v)
            }
        }
    }
*/

/*
        H264判断一帧的结束

	第一种情况 没有下一个nal则表示当前的nal是当前帧的最后一部分数据。

	第二种情况  nal_unit_type ！=next_nal_unit_type  则表示上一帧已经结束。

	第三种情况 nal_unit_type == next_nal_unit_type的时候有可能是同一帧的数据，也有可能是不同帧的数据。（如：两个PP帧在一块就有可能出现这种情况）

	出现第三种情况的时候我们需要分析slice_header里面的frame_num这一个值。如果frame_num != next_frame_num 则表示当前nal已经是当前帧的最后一部分数据。

	slice_header( ) {

            //片中的第一个宏块地址, 片通过这个句法元素来标定它自己的地址. 
            //在帧场自适应模式下, 宏块都是成对出现, 
            //这时本句法元素表示的是第几个宏块对, 对应的第一个宏块的真实地址应该是: 2*first_mb_in_slice


            // ---------------------------------------------------------------------------------------------------------------------------------------------------------
            //mb_adaptive_frame_field_flag == 0, first_mb_in_slice 就是该条带中第一个宏块的地址，并且first_mb_in_slice 的值应在0到 PicSizeInMbs– 1 的范围内（包括边界值）
            //
            //mb_adaptive_frame_field_flag != 0, first_mb_in_slice * 2  就是该条带中的第一个宏块地址，该宏块是该条带中第一个宏块对中的顶宏块，
            //                                                      并且first_mb_in_slice 的值应该在 0 到 PicSizeInMbs/ 2 – 1 的范围内（包括边界值）
            //
            // PicSizeInMbs 表示图像的大小（以宏块为单位），由序列参数集中的 pic_width_in_mbs_minus1、pic_height_in_map_units_minus1以及其他一些元素指定
            //
            // ---------------------------------------------------------------------------------------------------------------------------------------------------------
            //当前slice中包含的第一个宏块在整帧中的位置 坐标 序号 slice 中宏块有序排列
		first_mb_in_slice, 							// 2 ue(v)

		//当前slice的类型
		slice_type, 								// 2 ue(v) 片的类型

		//当前slice所依赖的pps的id
		pic_parameter_set_id, 							// 2 ue(v) 引用的图像集索引

		if(separate_colour_plane_flag == 1){

		    //当标识位separate_colour_plane_flag为true时，
		    //colour_plane_id表示当前的颜色分量, 0、1、2分别表示Y、U、V分量
                colour_plane_id,							// 2  u(2)
		}


            // ---------------------------------------------------------------------------------------------------------------------------------------------------------
		//每个参考帧都有一个连续的 frame_num 作为它们的标识, 它指明了各图像的解码顺序. 非参考帧也有, 但没有意义
		//
		//每个参考图像都有frame_num, 但编码器要指定当前图像的参考图像时, 使用是ref_id, frame_num->PicNum->ref_id
            //
            //表示当前帧序号的一种计量方式
            //
            //对于非参考帧来说,它的frame_num 值在解码过程中是没有意义的,因为frame_num 值是参考帧特有的,它的主要作用是在该图像被其他图像引用作运动补偿的参考时提供一个标识但
            //H.264 并没有在非参考帧图像中取消这一句法元素,原因是在 POC 的第二种和第三种解码方法中可以通过非参考帧的frame_num 值计算出他们的POC 值
            //
            //frame_num 是对帧编号的,也就是说如果在场模式下,同属一个场对的顶场和底场两个图像的 frame_num 的值是相同的
            //
            //frame_num 是参考帧的标识，但是在解码器中，并不是直接引用的 frame_num 值
            //而是由frame_num  进一步计算出来的变量 PicNum。MaxPicNum表征PicNum的最大值
            // 在场模式下MaxPicNum=2*MaxFrameNum，否则MaxPicNum =MaxFrameNum
            // 其中，MaxFrameNum 由序列参数集中的log2_max_frame_num_minus4 确定
            // PicNum 和frame_num 一样，也是嵌在循环中，当达到这个最大值时，PicNum将从0 开始重新计数
            // CurrPicNum是当前图像的PicNum 值，在计算PicNum的过程中，当前图像的 PicNum 值是由frame_num 直接算出
            // -  如果field_pic_flag= 0 ，  CurrPicNum = frame_num.
            // -  否则, CurrPicNum= 2 * frame_num + 1.
            //
            // 序列参数集中的gaps_in_frame_num_value_allowed_flag等于0时，参考帧的frame_num都是连续的
            // 如果等于1，这时若网络阻塞，编码器可以将编码后的若干图像丢弃，而不用另行通知解码器
            // 在这种情况下，解码器必须有机制将缺失的frame_num 及所对应的图像填补，否则后续图像若将运动矢量指向缺失的图像将会产生解码错误
            // ---------------------------------------------------------------------------------------------------------------------------------------------------------
		frame_num, 									// 2 u(v)
		
		if( !frame_mbs_only_flag ) {

                //场编码标识位.
                //当该标识位为 1 时表示当前slice按照 场 进行编码
                //当该标识位为 0 时表示当前slice按照 帧 进行编码
                field_pic_flag, 							// 2 u(1) 片层中标识图像编码模式的唯一一个元素

                if( field_pic_flag ){

                    //底场标识位
                    //该标志位为1表示当前slice是某一帧的底场
                    //该标志位为0表示当前slice为某一帧的顶场.
                    bottom_field_flag, 					// 2 u(1)  1:底场, 0:顶场 
                }
		}

		if(IdrPicFlag){ //nal_unit_type == 5

		    // IDR图像标识, 不同的IDR图像有不同的IDR值. 场模式下, IDR帧的两个场有相同的 idr_pic_id 值, [0..65535] 
		    //表示 IDR帧 的序号
		    //某一个 IDR帧 所属的所有slice, 其 idr_pic_id 应保持一致. 该值的取值范围为[0,65535]. 超出此范围时，以循环的方式重新开始计数
                idr_pic_id,                                             //2 ue(v)
		}
					
		if( pic_order_cnt_type == 0 ) {

		    // 在 poc 的第一种算法中, 显示传递 poc 的值, u(v)中, v == log2_max_pic_order_cnt_lsb_minus4 + 4 
		    // 表示当前帧序号的另一种计量方式
                pic_order_cnt_lsb,                                  // 2 u(v)

                if( bottom_filed_pic_order_in_frame_present_flag && !field_pic_flag ){

                    //如果是在场模式下, 场对中的两个场都各自被构造为一个图像, 它们有各自的poc的计算方法来分别计算两场的poc, 也就是一个场对拥有一对poc值; 
                    //而在帧模式或帧场自适应模式下, 一个图像只能根据片头中的元素计算出一个 poc. 
                    //在 frame_mbs_only_flag 不为1 时, 每个帧或场自适应的图像在解码时, 帧或帧场自适应中包含的两个场也必须有各自的poc 值, 
                    //通过本元素, 可以在已经解码的帧或帧场自适应图像的 poc 基础上新映射一个 poc, 并把它赋给底场.

                    //表示顶场与底场POC差值的计算方法，不存在则默认为0
                    delta_pic_order_cnt_bottom,                     // 2 se(v)
                }
		}

		if(pic_order_cnt_type==1 && !delta_pic_order_always_zero_flag){

                //poc的第二和第三种算法是从 frame_num 中映射得来, 本元素用于帧编码下的底场和场编码方式的场
		    delta_pci_order_cnt[0],                                 //2 se(v)

		    if(bottom_filed_pic_order_in_frame_present_flag && !field_pic_flag){
                    delta_pic_order_cnt[1],                             //2 se(v)	用于帧编码下的顶场
		    }
		}

		if(redundant_pic_cnt_present_flag){
                redundant_pic_cnt,                                      //2 ue(v)  冗余片的id号
		}

		if(slice_type == B){

		    //B 图像在直接预测模式下, 1:空间预测，0:时间预测 
                direct_spatial_mv_pred_flag,                            //2 u(1)
		}

		if(slice_type == P || slice_type == SP || slice_type == B){

		    //重载PPS中的参考帧队列中实际可用的参考帧的数目
                num_ref_idx_active_override_flag,                       //2 u(1)

                if(num_ref_idx_active_override_flag){
                    num_ref_idx_10_active_minus1,                       //2 ue(v)   重载值 

                    if(slice_type == B){
                        num_ref_idx_11_active_minus1,                   //2 ue(v)   重载值 
                    }
                }
		}

		ref_pic_list_reordering(){                                  //2
                if(slice_type != I && slice_type != SI){
                    ref_pic_list_recordering_flag_l0,                   //2 u(1)	指明List0是否进行重排序

                    if(ref_pic_list_recordering_flag_l0){
                        do{

                            recordering_of_pic_nums_idc,                    //2 ue(v)   执行哪种重排序操作

                            if(recordering_of_pic_nums_idc ==0 || recordering_of_pic_nums_idc == 1){

                                //从当前图像的 PicNum 加上(abs_diff_pic_num_minus1+1)后指明需要重排序的图像
                                abs_diff_pic_num_minus1,                    //2 ue(v)   对短期参考帧重排序时指明重排序图像与当前的差
                            } else if(recordering_of_pic_nums_idc == 2) {
                                long_term_pic_num,                          //2 ue(v)  对长期参考帧得排序时指明重排序图像
                            }

                        }while(recordering_of_pic_nums_idc != 3);
                    }
                }

                if(slice_type == B){
                    ref_pic_list_recordering_flag_l1,                   //2 u(1)	指明List1是否进行重排序

                    if(ref_pic_list_recordering_flag_l1){
                        do{

                            recordering_of_pic_nums_idc,                    //2 ue(v)   执行哪种重排序操作

                            if(recordering_of_pic_nums_idc ==0 || recordering_of_pic_nums_idc == 1){

                                //从当前图像的 PicNum 加上(abs_diff_pic_num_minus1+1)后指明需要重排序的图像
                                abs_diff_pic_num_minus1,                    //2 ue(v)   对短期参考帧重排序时指明重排序图像与当前的差
                            } else if(recordering_of_pic_nums_idc == 2) {
                                long_term_pic_num,                          //2 ue(v)  对长期参考帧得排序时指明重排序图像
                            }
                        
                        }while(recordering_of_pic_nums_idc != 3);
                    }
                }
		}

		if(nal_unit_type == 20){
                ref_pic_list_mvc_modification(),                        //2
		} else {
                ref_pic_list_modification(),                            //2
		}

		if(weighted_pred_flag && (slice_type == P || slice_type == SP) || weighted_bipred_idc == 1 && slice_type == B){
                pred_weight_table(){                                    //2
                    luma_log2_weight_denom,                             //2 ue (v)     给出参考帧列表中参考图像所有亮度的加权系数，[0..7] 
                    chroma_log2_weight_denom,                           //2 ue (v)     给出参考帧列表中参考图像所有色度的加权系数，[0..7]

                    for(i=0; i<=num_ref_idx_l0_active_minus; ++i){
                        luma_weight_l0_flag,                            //2 u(1)	    1:在参考帧序列0中的亮度的加权系数存在

                        if(luma_weight_l0_flag){
                            luma_weight_l0[i],                          //2 se(v)       用参考序列0预测亮度值时, 所用的加权系数. 如果luma_weight_l0_flag=0, luma_weight_l0[i]=2^luma_log2_weight_denom 
                            luma_offset_l0[i],                          //2 se(v)       用参考序列0预测亮度值时, 所用的加权系数的偏移, [-128-..127], 如果luma_weight_l0_flag=0, 该值0
                        }

                        chroma_weight_l0_flag,                          //2 u(1)	    同Luma相似, 但用于色度

                        if(chroma_weight_l0_flag){
                            for(j=0; j<2; ++j){
                                chroma_weight_l0[i][j],                 //2 se(v)
                                chroma_offset_l0[i][j],                 //2 se(v)
                            }
                        }
                    }

                    if(slice_type == B){
                        for(i=0; i<num_ref_idx_l1_active_minus1; ++i){
                            luma_weight_l1_flag,                        //2 u(1)

                            if(luma_weight_l1_flag){
                                luma_weight_l1[i],                      //2 se(v)
                                luma_offset_l1[i],                      //2 se(v)
                            }

                            chroma_weight_l1_flag,                      //2 u(1)

                            if(chroma_weight_l1_flag){
                                for(j=0; j<2; ++j){
                                    chroma_weight_l1[i][j],             //2 se(v)
                                    chroma_offset_l1[i][j],             //2 se(v)
                                }
                            }
                        }
                    }
                }
		}

		if(nal_ref_idc != 0){
		
		    //前文介绍的重排序操作是对参考帧队列重新排序，而标记操作负责将参考图像移入或移出参考帧队列
                dec_ref_pic_marking(){                                  //2
                    if(nal_unit_type == 5){
                        no_output_of_prior_pics_flag,                   //2-5 u(1)	IDR时，1:将前面已经解码的图像全部输出 

                        //IDR时，1:使用长期参考，并且每个IDR图像解码后自动成为长期参考帧，0:IDR图像解码后自动成为短期参考帧
                        long_term_reference_flag,                       //2-5 u(1)  
                    } else {
                    
                        //0:FIFO，使用滑动窗的机制，先入先出，在这种模式下，无法对长期参考帧进行操作,
                        //1:自适应标记，后续码流中会有一系列句法元素显式指明操作的步骤
                        adaptive_ref_pic_marking_mode_flag,             //u(1)

                        if(adaptive_ref_pic_marking_mode_flag){
                            do{
                                memory_management_control_operation,    //2-5 ue(v)

                                if(memory_management_control_operation == 1 || memory_management_control_operation == 3){

                                    //通过该元素可以计算出需要操作的图像在短期参考队列中的序号
                                    difference_of_pic_nums_minus1,      //2-5 ue(v)
                                }

                                if(memory_management_control_operation == 2){
                                    long_term_pic_num,                  //2-5 ue(v)	    得到所要操作的长期参考图像的序号
                                }

                                if(memory_management_control_operation == 3 || memory_management_control_operation == 6){
                                    long_term_frame_idx,                //2-5 ue(v)	    分配一个长期参考帧的序号给一个图像
                                }

                                if(memory_management_control_operation == 4){
                                    max_long_tewrm_frame_idx_plus1,     //2-5 ue(v)   此元素减1，指明长期参考队列的最大数目，[0..num_ref_frames]
                                }
                            }while(memory_management_control_operation  != 0);
                        }
                    }
                }
		}

		if(entropy_coding_mode_flag && slice_type != I && slice_type != SI){
                cabac_init_idc,                                         //2 ue(v)	给出cabac初始化时表格的选择

                //用于计算当前slice内所使用的初始qp值
                slice_qp_delta,                                         //2 se(v)	指出用于当前片的所有宏块的量化参数的初始值. SliceQPY == 26+pic_init_qp_minus26 + slice_qp_delta, [0..51]

                if(slice_type == SP || slice_type == SI){
                    sp_for_switch_flag,                                 //2 u(1)	指出SP帧中的p宏块的解码方式是否是switching模式
                    slice_qs_delta,                                     //2 se(v)   与slice_qp_delta的语义相似，用于SP和SI，QSY=26+pic_init_qs_minus26+slice_qs_delta, [0..51]
                }
		}

		if(deblocking_filter_control_present_flag){

		    //H.264 指定了一套算法可以在解码器端独立地计算图像中各边界的滤波强度进行滤波.
		    // 除了解码器独立计算之外，编码器也可以传递句法元素来干涉滤波强度，该元素指定了在块的是否使用滤波, 同时批明那个块的边界不用滤波
		    disable_deblocking_filter_idc,                          //2 ue(v)

                if(disable_deblocking_filter_idc != 1){
                
                    //增强 alpha 时的偏移值, FilterOffsetA=slice_alpha_c0_offset_div2 << 1
                    slice_alpha_c0_offset_div2,                         //2 se(v)

                    //增强 beta 的偏移值, FilterOffsetB=slice_beta_offset_div2<<1
                    slice_beta_offset_div2,                             //2 se(v)
                }
		}

		if(num_slice_groups_minus1 > 0 && slice_group_map_type >= 3 && slice_group_map_type <= 5){

		    //片组类型是3.4.5时, 由该元素可以获取片组中映射单元的数目
                slice_group_change_cycle,                               //2 u(v)
		}
	}
*/

/*
    slice_type

    +------------+---+---+---+---+---+---+---+---+---+---+
    | slice_type | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
    +------------+---+---+---+---+---+---+---+---+---+---+
    | slice_type | P | B | I |SP |SI | P | B | I |SP |SI |
    +------------+---+---+---+---+---+---+---+---+---+---+

    1) slice_type 的值在 5 到 9 的范围内表示, 除了当前条带的编码类型, 所有当前编码图像的其他条带的 slice_type 的值应与当前条带的 slice_type 的值一样, 
        或者等于当前条带的 slice_type 的值减 5.

    2) 对于IDR图像, slice_type的值应为2、4、7或者9

    3) 如果num_ref_frames的值为0, slice_type的值应为2、4、7或者9. 
        其中, num_ref_frames 是 SPS 的语法元素, 规定了可能在视频序列中任何图像帧间预测的解码过程中用到的短期参考帧和长期参考帧、互补参考场对以及不成对的参考场的最大数量
*/

/*
    +---------------------+------------------------------+--------------------------+-------------------
    | frame_mbs_only_flag | mb_adaptive_frame_field_flag |      field_pic_flag      | 模式
    +---------------------+------------------------------+--------------------------+-------------------
    |   1                 |     不存在于码流中           |      不存在于码流中      |	  帧 编码
    +---------------------+------------------------------+--------------------------+-------------------
    |   0                 |     0                        |      0                   |	  帧 编码
    +---------------------+------------------------------+--------------------------+-------------------
    |   0                 |     0                        |      1                   |	  场 编码
    +---------------------+------------------------------+--------------------------+-------------------
    |   0                 |     1                        |      0                   |    帧场自适应（仅在此情形下，MbaffFrameFlag=1，其他几种情况下MbaffFrameFlag都为0）
    +---------------------+------------------------------+--------------------------+-------------------
    |   0                 |     1                        |      1                   |	  场 编码
    +---------------------+------------------------------+--------------------------+-------------------
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
	6. OptionFileds 						任选字段				有填充时候，填充任选字段后面
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
------------------------------------------------------------------------------------2byte           ================> 6byte
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
	------------------------------------------------------------------------------1byte         ================> 9byte
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
