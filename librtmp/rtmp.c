/*
*      Copyright (C) 2005-2008 Team XBMC
*      http://www.xbmc.org
*      Copyright (C) 2008-2009 Andrej Stepanchuk
*      Copyright (C) 2009-2010 Howard Chu
*
*  This file is part of librtmp.
*
*  librtmp is free software; you can redistribute it and/or modify
*  it under the terms of the GNU Lesser General Public License as
*  published by the Free Software Foundation; either version 2.1,
*  or (at your option) any later version.
*
*  librtmp is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public License
*  along with librtmp see the file COPYING.  If not, write to
*  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*  Boston, MA  02110-1301, USA.
*  http://www.gnu.org/copyleft/lgpl.html
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "rtmp_sys.h"
#include "log.h"

#ifdef CRYPTO
#ifdef USE_POLARSSL
#include <polarssl/havege.h>
#include <polarssl/md5.h>
#include <polarssl/base64.h>
#define MD5_DIGEST_LENGTH 16

static const char *my_dhm_P =
	"E4004C1F94182000103D883A448B3F80" \
	"2CE4B44A83301270002C20D0321CFD00" \
	"11CCEF784C26A400F43DFB901BCA7538" \
	"F2C6B176001CF5A0FD16D2C48B1D0C1C" \
	"F6AC8E1DA6BCC3B4E1F96B0564965300" \
	"FFA1D0B601EB2800F489AA512C4B248C" \
	"01F76949A60BB7F00A40B1EAB64BDD48" \
	"E8A700D60B7F1200FA8E77B0A979DABF";

static const char *my_dhm_G = "4";

#elif defined(USE_GNUTLS)
#include <gnutls/gnutls.h>
#define MD5_DIGEST_LENGTH 16
#include <nettle/base64.h>
#include <nettle/md5.h>
#else	/* USE_OPENSSL */
#include <openssl/ssl.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#endif
TLS_CTX RTMP_TLS_ctx;
#endif

#define RTMP_SIG_SIZE 1536
#define RTMP_LARGE_HEADER_SIZE 12

static const int packetSize[] = { 12, 8, 4, 1 };

static const int CRC_32_Table[] = {
    0x00000000, 0xB71DC104, 0x6E3B8209, 0xD926430D, 0xDC760413, 0x6B6BC517,
    0xB24D861A, 0x0550471E, 0xB8ED0826, 0x0FF0C922, 0xD6D68A2F, 0x61CB4B2B,
    0x649B0C35, 0xD386CD31, 0x0AA08E3C, 0xBDBD4F38, 0x70DB114C, 0xC7C6D048,
    0x1EE09345, 0xA9FD5241, 0xACAD155F, 0x1BB0D45B, 0xC2969756, 0x758B5652,
    0xC836196A, 0x7F2BD86E, 0xA60D9B63, 0x11105A67, 0x14401D79, 0xA35DDC7D,
    0x7A7B9F70, 0xCD665E74, 0xE0B62398, 0x57ABE29C, 0x8E8DA191, 0x39906095,
    0x3CC0278B, 0x8BDDE68F, 0x52FBA582, 0xE5E66486, 0x585B2BBE, 0xEF46EABA,
    0x3660A9B7, 0x817D68B3, 0x842D2FAD, 0x3330EEA9, 0xEA16ADA4, 0x5D0B6CA0,
    0x906D32D4, 0x2770F3D0, 0xFE56B0DD, 0x494B71D9, 0x4C1B36C7, 0xFB06F7C3,
    0x2220B4CE, 0x953D75CA, 0x28803AF2, 0x9F9DFBF6, 0x46BBB8FB, 0xF1A679FF,
    0xF4F63EE1, 0x43EBFFE5, 0x9ACDBCE8, 0x2DD07DEC, 0x77708634, 0xC06D4730,
    0x194B043D, 0xAE56C539, 0xAB068227, 0x1C1B4323, 0xC53D002E, 0x7220C12A,
    0xCF9D8E12, 0x78804F16, 0xA1A60C1B, 0x16BBCD1F, 0x13EB8A01, 0xA4F64B05,
    0x7DD00808, 0xCACDC90C, 0x07AB9778, 0xB0B6567C, 0x69901571, 0xDE8DD475,
    0xDBDD936B, 0x6CC0526F, 0xB5E61162, 0x02FBD066, 0xBF469F5E, 0x085B5E5A,
    0xD17D1D57, 0x6660DC53, 0x63309B4D, 0xD42D5A49, 0x0D0B1944, 0xBA16D840,
    0x97C6A5AC, 0x20DB64A8, 0xF9FD27A5, 0x4EE0E6A1, 0x4BB0A1BF, 0xFCAD60BB,
    0x258B23B6, 0x9296E2B2, 0x2F2BAD8A, 0x98366C8E, 0x41102F83, 0xF60DEE87,
    0xF35DA999, 0x4440689D, 0x9D662B90, 0x2A7BEA94, 0xE71DB4E0, 0x500075E4,
    0x892636E9, 0x3E3BF7ED, 0x3B6BB0F3, 0x8C7671F7, 0x555032FA, 0xE24DF3FE,
    0x5FF0BCC6, 0xE8ED7DC2, 0x31CB3ECF, 0x86D6FFCB, 0x8386B8D5, 0x349B79D1,
    0xEDBD3ADC, 0x5AA0FBD8, 0xEEE00C69, 0x59FDCD6D, 0x80DB8E60, 0x37C64F64,
    0x3296087A, 0x858BC97E, 0x5CAD8A73, 0xEBB04B77, 0x560D044F, 0xE110C54B,
    0x38368646, 0x8F2B4742, 0x8A7B005C, 0x3D66C158, 0xE4408255, 0x535D4351,
    0x9E3B1D25, 0x2926DC21, 0xF0009F2C, 0x471D5E28, 0x424D1936, 0xF550D832,
    0x2C769B3F, 0x9B6B5A3B, 0x26D61503, 0x91CBD407, 0x48ED970A, 0xFFF0560E,
    0xFAA01110, 0x4DBDD014, 0x949B9319, 0x2386521D, 0x0E562FF1, 0xB94BEEF5,
    0x606DADF8, 0xD7706CFC, 0xD2202BE2, 0x653DEAE6, 0xBC1BA9EB, 0x0B0668EF,
    0xB6BB27D7, 0x01A6E6D3, 0xD880A5DE, 0x6F9D64DA, 0x6ACD23C4, 0xDDD0E2C0,
    0x04F6A1CD, 0xB3EB60C9, 0x7E8D3EBD, 0xC990FFB9, 0x10B6BCB4, 0xA7AB7DB0,
    0xA2FB3AAE, 0x15E6FBAA, 0xCCC0B8A7, 0x7BDD79A3, 0xC660369B, 0x717DF79F,
    0xA85BB492, 0x1F467596, 0x1A163288, 0xAD0BF38C, 0x742DB081, 0xC3307185,
    0x99908A5D, 0x2E8D4B59, 0xF7AB0854, 0x40B6C950, 0x45E68E4E, 0xF2FB4F4A,
    0x2BDD0C47, 0x9CC0CD43, 0x217D827B, 0x9660437F, 0x4F460072, 0xF85BC176,
    0xFD0B8668, 0x4A16476C, 0x93300461, 0x242DC565, 0xE94B9B11, 0x5E565A15,
    0x87701918, 0x306DD81C, 0x353D9F02, 0x82205E06, 0x5B061D0B, 0xEC1BDC0F,
    0x51A69337, 0xE6BB5233, 0x3F9D113E, 0x8880D03A, 0x8DD09724, 0x3ACD5620,
    0xE3EB152D, 0x54F6D429, 0x7926A9C5, 0xCE3B68C1, 0x171D2BCC, 0xA000EAC8,
    0xA550ADD6, 0x124D6CD2, 0xCB6B2FDF, 0x7C76EEDB, 0xC1CBA1E3, 0x76D660E7,
    0xAFF023EA, 0x18EDE2EE, 0x1DBDA5F0, 0xAAA064F4, 0x738627F9, 0xC49BE6FD,
    0x09FDB889, 0xBEE0798D, 0x67C63A80, 0xD0DBFB84, 0xD58BBC9A, 0x62967D9E,
    0xBBB03E93, 0x0CADFF97, 0xB110B0AF, 0x060D71AB, 0xDF2B32A6, 0x6836F3A2,
    0x6D66B4BC, 0xDA7B75B8, 0x035D36B5, 0xB440F7B1, 0x00000001
};

int RTMP_ctrlC;

const char RTMPProtocolStrings[][7] = {
	"RTMP",
	"RTMPT",
	"RTMPE",
	"RTMPTE",
	"RTMPS",
	"RTMPTS",
	"",
	"",
	"RTMFP"
};

const char RTMPProtocolStringsLower[][7] = {
	"rtmp",
	"rtmpt",
	"rtmpe",
	"rtmpte",
	"rtmps",
	"rtmpts",
	"",
	"",
	"rtmfp"
};

static const char *RTMPT_cmds[] = {
	"open",
	"send",
	"idle",
	"close"
};

typedef enum {
	RTMPT_OPEN=0, RTMPT_SEND, RTMPT_IDLE, RTMPT_CLOSE
} RTMPTCmd;

static int DumpMetaData(AMFObject *obj);
static int HandShake(RTMP *r, int FP9HandShake);
static int SocksNegotiate(RTMP *r);

static int SendConnectPacket(RTMP *r, RTMPPacket *cp);
static int SendCheckBW(RTMP *r);
static int SendCheckBWResult(RTMP *r, double txn);
static int SendDeleteStream(RTMP *r, double dStreamId);
static int SendFCSubscribe(RTMP *r, AVal *subscribepath);
static int SendPlay(RTMP *r);
static int SendBytesReceived(RTMP *r);
static int SendUsherToken(RTMP *r, AVal *usherToken);

#if 0				/* unused */
static int SendBGHasStream(RTMP *r, double dId, AVal *playpath);
#endif

static int HandleInvoke(RTMP *r, const char *body, unsigned int nBodySize);
static int HandleMetadata(RTMP *r, char *body, unsigned int len);
static void HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet);
static void HandleAudio(RTMP *r, const RTMPPacket *packet);
static void HandleVideo(RTMP *r, const RTMPPacket *packet);
static void HandleCtrl(RTMP *r, const RTMPPacket *packet);
static void HandleServerBW(RTMP *r, const RTMPPacket *packet);
static void HandleClientBW(RTMP *r, const RTMPPacket *packet);

static int ReadN(RTMP *r, char *buffer, int n);
static int WriteN(RTMP *r, const char *buffer, int n);

static void DecodeTEA(AVal *key, AVal *text);

static int HTTP_Post(RTMP *r, RTMPTCmd cmd, const char *buf, int len);
static int HTTP_read(RTMP *r, int fill);

static void CloseInternal(RTMP *r, int reconnect);

#ifndef _WIN32
static int clk_tck;
#endif

#ifdef CRYPTO
#include "handshake.h"
#endif

uint32_t
	RTMP_GetTime()
{
#ifdef _DEBUG
	return 0;
#elif defined(_WIN32)
	return timeGetTime();
#else
	struct tms t;
	if (!clk_tck) clk_tck = sysconf(_SC_CLK_TCK);
	return times(&t) * 1000 / clk_tck;
#endif
}

#define ngx_movemem(dst, src, n)   (((u_char *) memmove(dst, src, n)) + (n))
#define ngx_memset(buf, c, n)     (void) memset(buf, c, n)
#define ngx_memcpy(dst, src, n)   (void) memcpy(dst, src, n)


void
	RTMP_UserInterrupt()
{
	RTMP_ctrlC = TRUE;
}

uint32_t crc32(int *crctable, uint32_t crc, const char *buf, int len)
{
	const uint8_t *end = buf + len;

	while (buf < end)
        crc = crctable[((uint8_t) crc) ^ *buf++] ^ (crc >> 8);

    return crc;
}

static int write_pcr_bits(uint8_t *p, int64_t pcr)
{
    int64_t pcr_low = pcr % 300, pcr_high = pcr / 300;

    *p++ = pcr_high >> 25;
    *p++ = pcr_high >> 17;
    *p++ = pcr_high >>  9;
    *p++ = pcr_high >>  1;
    *p++ = pcr_high <<  7 | pcr_low >> 8 | 0x7e;
    *p++ = pcr_low;

    return 6;
}

static char* mpegts_write_pcr_bits(uint8_t *p, int64_t pcr)
{
    int64_t pcr_low = pcr % 300, pcr_high = pcr / 300;

    *p++ = pcr_high >> 25;
    *p++ = pcr_high >> 17;
    *p++ = pcr_high >>  9;
    *p++ = pcr_high >>  1;
    *p++ = pcr_high <<  7 | pcr_low >> 8 | 0x7e;
    *p++ = pcr_low;

    return p;
}


static char* write_pts(char *q, int fourbits, int64_t pts)
{
    int val;

	//11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS

    val  = fourbits << 4 | (((pts >> 30) & 0x07) << 1) | 1;
    *q++ = (char) val;
    val  = (((pts >> 15) & 0x7fff) << 1) | 1;
    *q++ = (char) (val >> 8);
    *q++ = (char) val;
    val  = (((pts) & 0x7fff) << 1) | 1;
    *q++ = (char) (val >> 8);
    *q++ = (char) val;

	return q;
}

static void * rtmp_rmemcpy(void *dst, const void* src, int n)
{
    u_char     *d, *s;

    d = dst;
    s = (u_char*)src + n - 1;

    while(s >= (u_char*)src) {
        *d++ = *s--;
    }

    return dst;
}

static int rtmp_hls_append_aud(char *buf, int len)
{
    static u_char aud_nal[] = { 0x00, 0x00, 0x00, 0x01, 0x09, 0xf0 };

	char *p = buf;
    if (sizeof(aud_nal) > len) {
        return -1;
    }

	memcpy(p, aud_nal, sizeof(aud_nal));
	p += sizeof(aud_nal);
	
    return p - buf;
}

/*
	typedef struct adts_fixed_header
*/
static int adts_write_frame_header(char *buf, int profile, int samplefreidx, int chancfg, int size){
	char *p = buf;

	*p++ = 0xff;
    *p++ = 0xf1;
    *p++ = (u_char) (((profile - 1) << 6) | (samplefreidx << 2) | ((chancfg & 0x04) >> 2));
    *p++ = (u_char) (((chancfg & 0x03) << 6) | ((size >> 11) & 0x03));
    *p++ = (u_char) (size >> 3);
    *p++ = (u_char) ((size << 5) | 0x1f);
    *p++ = 0xfc;

	return p - buf;
}

#if 0
static int adts_write_frame_header(char *buf, int profile, int samplefreidx, int chancfg, int size){
	char *p = buf;

	*p++ = (char) 0xFF;
    *p++ = (char) 0xF9;
    *p++ = (char) (((profile - 1) << 6) + (samplefreidx << 2) + (chancfg >> 2));
    *p++ = (char) (((chancfg & 0x03) << 6) + (size >> 11));
    *p++ = (char) ((size & 0x7FF) >> 3);
    *p++ = (char) (((size & 0x07) << 5) + 0x1F);
    *p++ = (char) 0xFC;

	return p - buf;
}
#endif


static int rtmp_hls_append_sps_pps(char *buf, int len, char *sps, int nsps, char *pps, int npps)
{
	char *p = buf;
	//sps
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x01;
	memcpy(p, sps, nsps);
	p += nsps;

	//pps
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x01;
	memcpy(p, pps, npps);
	p += npps;

	return p - buf;
}


int rtmp_mpegts_write_frame(RTMPPacket *pkt, int* cc, Tag_Video_AvcC* avc, AudioSpecificConfig *aac)
{
    int  pes_size, header_size, body_size, in_size, stuff_size, flags, blen, ret, size;
    char  packet[188] = {0}, *p, *base, *pos, *last, buf[1024*1024] = {0}, *bp;
    int   first, rc, pid, sid, key = 0;

	int pmtpid = 0xf0 << 8 | 0x00;
	int videopid = 0x01 << 8 | 0x00;
	int audiopid = 0x01 << 8 | 0x01;

	int64_t pts=0, dts=0;
	uint32_t cts=0, rlen, len;
	uint8_t src_nal_type, nal_type;

	char *src;
	int slen;
	int aud_sent, sps_pps_sent;

	bp = buf;
	blen = sizeof(buf);

	static FILE *pfile = NULL;
	
	if(pkt->m_packetType == RTMP_PACKET_TYPE_VIDEO){

		if((pkt->m_body[0] & 0x0f) == 0x07){
			src = pkt->m_body + 5;
			slen = pkt->m_nBodySize - 5;
		} else {
			src = pkt->m_body + 1;
			slen = pkt->m_nBodySize - 1;
		}

		//video tag  type|codeid  1byte
		key = ((pkt->m_body[0] & 0xf0) >> 4) == 1 ? 1 : 0;

		//pts dts   Composition time offset   3byte
		dts = (int64_t)pkt->m_nTimeStamp * 90;
		memcpy(&cts, pkt->m_body+2, 3);
		cts = ((cts & 0x00FF0000) >> 16) | ((cts & 0x000000FF) << 16) |
	          (cts & 0x0000FF00);

		pts = dts + cts * 90;

		aud_sent = 0;
		sps_pps_sent = 0;
		
		while(slen){

			//data len  4byte
			if(slen < 4)
				return 0;
			memcpy(&rlen, src, 4);
			src += 4;
			slen -= 4;

			len = 0;
	        rtmp_rmemcpy(&len, &rlen, 4);
	        if (len == 0) {
	            continue;
	        }

			//nalu type  1byte
			if(slen < 1)
				return 0;
			memcpy(&src_nal_type, src, 1);
			src++;
			slen--;

			nal_type = src_nal_type & 0x1f;

			if (nal_type >= 7 && nal_type <= 9) {
	            if (slen < len -1) {
					return -1;
	            }
				src += len - 1;
				slen -= len - 1;
	            continue;
	        }

			if (!aud_sent) {
	            switch (nal_type) {
	                case 1:
	                case 5:
	                case 6:
	                    ret = rtmp_hls_append_aud(bp, blen);
						bp += ret;
						blen -= ret;
	                case 9:
	                    aud_sent = 1;
	                    break;
	            }
	        }

			switch (nal_type) {
	            case 1:
	                sps_pps_sent = 0;
	                break;
	            case 5:
	                if (sps_pps_sent) {
	                    break;
	                }

					ret = rtmp_hls_append_sps_pps(bp, blen, avc->sequenceParameterSetNALUnit, 
						avc->sequenceParameterSetLength, 
						avc->pictureParameterSetNALUnit, 
						avc->pictureParameterSetLength);
					bp += ret;
					blen -= ret;
					
	                sps_pps_sent = 1;
	                break;
	        }

			/* AnnexB prefix */

	        if (blen < 5) {
				printf("---hls: not enough buffer for AnnexB prefix\n");
	            return 0;
	        }
			
			/* first AnnexB prefix is long (4 bytes) */

	        if (blen == sizeof(buf)) {
	            *bp++ = 0;
	        }

	        *bp++ = 0;
	        *bp++ = 0;
	        *bp++ = 1;
	        *bp++ = src_nal_type;

			/* NAL body */

	        if (blen < len) {
	            printf("---hls: not enough buffer for NAL\n");
	            return 0;
	        }

			memcpy(bp, src, len - 1);
			bp += len - 1;
			blen -= len - 1;

			src += len - 1;
			slen -= len - 1;

		}

	}else{

		if(((pkt->m_body[0] >> 4) & 0x0f) == 0x0a){
			src = pkt->m_body + 2;
			slen = pkt->m_nBodySize - 2;
		} else {
			src = pkt->m_body + 1;
			slen = pkt->m_nBodySize - 1;
		}
		size = slen + 7;
    	dts = pts = (uint64_t) pkt->m_nTimeStamp * 90;

		ret = adts_write_frame_header(bp, aac->aac_profile, aac->sampling_frequency_index, 
			aac->channel_configuration, size);

		bp += ret;
		blen -= ret;

		if(blen < slen)
			printf("---hls: not enough buffer for AAC");
		memcpy(bp, src, slen);
		bp += slen;
		blen -= slen;
	}
		
	pos = buf;
	last = bp;

	if(!pfile){
		pfile = fopen("./test.ts", "a+");

		memset(packet, 0, sizeof(packet));
		Ts_Packet_Pat(0x00, pmtpid, packet);
		fwrite(packet, 188, 1, pfile);

		memset(packet, 0, sizeof(packet));
		Ts_Packet_Pmt(pmtpid, videopid, audiopid, packet);
		fwrite(packet, 188, 1, pfile);
	}

	if(pkt->m_packetType == RTMP_PACKET_TYPE_VIDEO){
		pid = videopid;
		sid = 0xe0;
	} else {
		pid = audiopid;
		sid = 0xc0;
	}


	first = 1;
    while (pos < last) {
        p = packet;

        *p++ = 0x47;
        *p++ = (u_char) (pid >> 8);

        if (first) {
            p[-1] |= 0x40;
        }

        *p++ = (u_char) pid;
        *p++ = 0x10 | ((*cc)++ & 0x0f); /* payload */

        if (first) {

            if (key) {
                packet[3] |= 0x20; /* adaptation */

                *p++ = 7;    /* size */
                *p++ = 0x50; /* random access + PCR */

                /*
                            在 TS 的传输过程中，一般 DTS 和 PCR 差值会在一个合适的范围，这个差值就是要设置的视音频 Buffer 的大小. 
                            1) 视频 DTS 和 PCR 的差值在 700ms -- 1200ms 之间.
                            2) 音频 DTS 和 PCR 的差值在 200ms -- 700ms 之间。
                        */
                p = mpegts_write_pcr_bits(p, dts - 63000); /* 700 ms PCR delay */
            }

            /* PES header */

            *p++ = 0x00;
            *p++ = 0x00;
            *p++ = 0x01;
            *p++ = (u_char) sid;

            header_size = 5;
            flags = 0x80; /* PTS */

            if (dts != pts) {
                header_size += 5;
                flags |= 0x40; /* DTS */
            }

            pes_size = (last - pos) + header_size + 3;
            if (pes_size > 0xffff) {
                pes_size = 0;
            }

            *p++ = (u_char) (pes_size >> 8);
            *p++ = (u_char) pes_size;
            *p++ = 0x80; /* H222 */
            *p++ = (u_char) flags;
            *p++ = (u_char) header_size;

            p = write_pts(p, flags >> 6, pts + 63000);

            if (dts != pts) {
                p = write_pts(p, 1, dts + 63000);
            }

            first = 0;
        }

        body_size = (int) (packet + sizeof(packet) - p);
        in_size = (int) (last - pos);

        if (body_size <= in_size) {
            ngx_memcpy(p, pos, body_size);
            pos += body_size;

        } else {
            stuff_size = (body_size - in_size);

            if (packet[3] & 0x20) {

                /* has adaptation */

                base = &packet[5] + packet[4];
                p = ngx_movemem(base + stuff_size, base, p - base);
                ngx_memset(base, 0xff, stuff_size);
                packet[4] += (char) stuff_size;

            } else {

                /* no adaptation */

                packet[3] |= 0x20;
                p = ngx_movemem(&packet[4] + stuff_size, &packet[4], p - &packet[4]);

                packet[4] = (char) (stuff_size - 1);
                if (stuff_size >= 2) {
                    packet[5] = 0;
                    ngx_memset(&packet[6], 0xff, stuff_size - 2);
                }
            }

            ngx_memcpy(p, pos, in_size);
            pos = last;
        }

		rc = fwrite(packet, 188, 1, pfile);
        if (rc < 0) {
			printf("---hls: fwrite failed ret = %d\n", rc);
            return rc;
        }
    }

    return 0;
}



int Ts_Packet_AV(RTMPPacket *packet, char *sps, int nsps, char *pps, int npps, int *cont, 
	AudioSpecificConfig *aac)
{
	int key = 0, pktlen=0, pid, ret;
	static FILE *pfile = NULL;
	char buf[1024*1024*5] = {0}, tsdata[188] = {0};
	char *p;
	int64_t pcr = 0;

	int pmtpid = 0xf0 << 8 | 0x00;
	int videopid = 0x01 << 8 | 0x00;
	int audiopid = 0x01 << 8 | 0x01;
	
	if(packet->m_packetType == RTMP_PACKET_TYPE_VIDEO && packet->m_body[0] == 0x17){
		key = 1;
	}

	if(!pfile){
		pfile = fopen("./test.ts", "a+");

		memset(tsdata, 0, sizeof(tsdata));
		Ts_Packet_Pat(0x00, pmtpid, tsdata);
		fwrite(tsdata, 188, 1, pfile);

		memset(tsdata, 0, sizeof(tsdata));
		Ts_Packet_Pmt(pmtpid, videopid, audiopid, tsdata);
		fwrite(tsdata, 188, 1, pfile);
	}

	memset(buf, 0, sizeof(buf));
	pktlen = Pes_Packet_AV(packet, sps, nsps, pps, npps, buf, key, aac);
	p = buf;

	if(packet->m_packetType == RTMP_PACKET_TYPE_VIDEO){
		pid = videopid;
		pcr = packet->m_nTimeStamp * 90 - 63000;
	} else {
		pid = audiopid;
		pcr = 0;
	}

	memset(tsdata, 0, sizeof(tsdata));
	ret = Ts_Header(tsdata, 1, pid, 3, pcr, 0, (*cont)++);
	//printf("--headerlen=%d, pktlen=%d--\n", ret, pktlen);
	memcpy(tsdata+ret, p, 188-ret);
	pktlen -= 188-ret;
	p += 188-ret;
	fwrite(tsdata, 188, 1, pfile);
	

	while(pktlen > 0){
		if(pktlen >= 188 - 4){
			memset(tsdata, 0, sizeof(tsdata));
			ret = Ts_Header(tsdata, 0, pid, 1, pcr, 0, (*cont)++);

			//printf("--headerlen=%d, pktlen=%d--\n", ret, pktlen);
			
			memcpy(tsdata+ret, p, 188-ret);
			pktlen -= 188-ret;
			p += 188-ret;
			fwrite(tsdata, 188, 1, pfile);
		}

		if(pktlen == 183){
			memset(tsdata, 0, sizeof(tsdata));
			ret = Ts_Header(tsdata, 0, pid, 1, pcr, 0, (*cont)++);			
			memcpy(tsdata+ret, p, pktlen);
			tsdata[187] = 0xff;
			break;
		}
		
		if(pktlen <= 188 - 6){
			memset(tsdata, 0, sizeof(tsdata));
			ret = 188 - 6 - pktlen;
			ret = Ts_Header(tsdata, 0, pid, 3, pcr, ret, (*cont)++);

			//printf("--headerlen=%d, pktlen=%d--\n", ret, pktlen);
			if(pktlen < 188 - 6)
				memset(tsdata+ret, 0xff, 188-pktlen-ret);
			memcpy(tsdata+188-pktlen, p, pktlen);

			fwrite(tsdata, 188, 1, pfile);
			break;
		}
	}

	return 0;
}

int Ts_Header(char *buf, int startflag, int pid, int adaptflag, int64_t pcr, int stuflen, int cont)
{
	int adflag = 0;
	char *p = buf;
	*p++ = 0x47;
	*p++ = (pid >> 8) & 0x1f;
	if(startflag)
		p[-1] |= 0x40;
	*p++ = pid & 0xff;

	switch(adaptflag){
		case 1: adflag |= 0x01 << 4; break;
		case 2: adflag |= 0x02 << 4; break;
		case 3: adflag |= 0x03 << 4; break;
		default : break;
	}
	//adflag |= cont & 0x0f;

	adflag = (adflag & 0xf0) | (cont & 0x0f);

	*p++ = adflag;

	if(adaptflag == 1 && startflag){
		*p++ = 0x00;
	}

	if(adaptflag > 1 && startflag){
		if(pcr){
			*p++ = 0x07;	//len
			*p++ = 0x50;	//flag
			p += write_pcr_bits(p, pcr);
		} else {
			*p++ = 0x01;
			*p++ = 0x40;
		}
	}

	if(adaptflag > 1 && !startflag){
		*p++ = stuflen;
		*p++ = 0x00;
	}

	return p - buf;
}

void Ts_Packet_Pmt(int pmt_pid, int video_pid, int audio_pid, char *buf)
{
	unsigned int crc;
	char *p = buf;

	p += Ts_Header(p, 1, pmt_pid, 1, 0, 0, 1);

	*p++ = 0x02;
	*p++ = 0xb0;
	*p++ = 0x17;
	*p++ = 0x00;
	*p++ = 0x01;
	*p++ = 0xc1;
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0xe1;
	*p++ = 0x00;
	*p++ = 0xf0;
	*p++ = 0x00;

	*p++ = 0x1b;	//h264
	*p++ = 0xe0 | ((video_pid >> 8) & 0x1f);
	*p++ = video_pid & 0xff;
	*p++ = 0xf0;
	*p++ = 0x00;

	*p++ = 0x0f;	//aac
	*p++ = 0xe0 | ((audio_pid >> 8) & 0x1f);
	*p++ = audio_pid & 0xff;
	*p++ = 0xf0;
	*p++ = 0x00;

	//crc32
	#if 0
	crc = crc32(CRC_32_Table, -1, buf, p - buf);

	*p++ = (crc >> 24) & 0xff;
	*p++ = (crc >> 16) & 0xff;
	*p++ = (crc >> 8) & 0xff;
	*p++ = crc & 0xff;
	#endif

	*p++ = 0x2f;
	*p++ = 0x44;
	*p++ = 0xb9;
	*p++ = 0x9b;
	
	if(p-buf < 188)
		memset(p, 0xff, 188 - (p - buf));

	return;
}

void Ts_Packet_Pat(int pat_pid, int pmt_pid, char *buf)
{
	unsigned int crc;
	char *p = buf;

	p += Ts_Header(p, 1, pat_pid, 1, 0, 0, 1);

	*p++ = 0x00;	//pat table id
	*p++ = 0xb0;
	*p++ = 0x0d;	//lenght
	*p++ = 0x00;
	*p++ = 0x01;	//transport_stream_id
	*p++ = 0xc1;
	*p++ = 0x00;
	*p++ = 0x00;

	*p++ = 0x00;
	*p++ = 0x01;	//program_number

	//*p++ = 0xf0;	//pmt_id
	//*p++ = 0x00;

	*p++ = 0xe0 | ((pmt_pid >> 8) & 0x1f);
	*p++ = pmt_pid & 0xff;

	//crc32
	#if 0
	crc = crc32(CRC_32_Table, -1, buf, p - buf);

	*p++ = (crc >> 24) & 0xff;
	*p++ = (crc >> 16) & 0xff;
	*p++ = (crc >> 8) & 0xff;
	*p++ = crc & 0xff;
	#endif

	*p++ = 0x2a;
	*p++ = 0xb1;
	*p++ = 0x04;
	*p++ = 0xb2;

	if(p-buf < 188)
		memset(p, 0xff, 188 - (p - buf));

	return;
}

/*int avpriv_mpeg4audio_sample_rates[] = { 
   96000, 88200, 64000, 48000, 44100, 32000, 
   24000, 22050, 16000, 12000, 11025, 8000, 7350 
}; 
channel_configuration: 表示声道数chanCfg 
0: Defined in AOT Specifc Config 
1: 1 channel: front-center 
2: 2 channels: front-left, front-right 
3: 3 channels: front-center, front-left, front-right 
4: 4 channels: front-center, front-left, front-right, back-center 
5: 5 channels: front-center, front-left, front-right, back-left, back-right 
6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel 
7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel 
8-15: Reserved 
*/  



int Pes_Packet_AV(RTMPPacket *packet, char *sps, int nsps, char *pps, int npps, char *buf, int key,
	AudioSpecificConfig *aac)
{
	char *p = buf;
	char *src;
	int slen=0;
	int64_t pts=0, dts=0;
	uint32_t cts=0;

	switch (packet->m_packetType)
	{
		case RTMP_PACKET_TYPE_AUDIO: {
			if(((packet->m_body[0] >> 4) & 0x0f) == 0x0a){
				src = packet->m_body + 2;
				slen = packet->m_nBodySize - 2;
			} else {
				src = packet->m_body + 1;
				slen = packet->m_nBodySize - 1;
			}
		}break;
		case RTMP_PACKET_TYPE_VIDEO: {
			if((packet->m_body[0] & 0x0f) == 0x07){
				src = packet->m_body + 4 + 5;
				slen = packet->m_nBodySize - 4 - 5;
			} else {
				src = packet->m_body + 1 + 5;
				slen = packet->m_nBodySize - 1 - 5;
			}
		}break;
		
		default : break;
	}

	//pes start_code
	*p++ = 0x00;
	*p++ = 0x00;
	*p++ = 0x01;

	
	//pes pts_dts_flag
	//if(pts != 0 && dts != 0 && dts != pts){
	if(packet->m_packetType == RTMP_PACKET_TYPE_VIDEO){

		//pes stream_id
		*p++ = 0xe0;

		//pes packet_en
		if(packet->m_nBodySize+3+10 > 0xffff){
			*p++ = 0x00;
			*p++ = 0x00;
		} else {
			*p++ = (packet->m_nBodySize+3+10) >> 8;
			*p++ = packet->m_nBodySize+3+10;
		}

		//pes flag
		*p++ = 0x80;

		//pts dts
		dts = (int64_t)packet->m_nTimeStamp * 90;
		memcpy(&cts, packet->m_body+2, 3);
		cts = ((cts & 0x00FF0000) >> 16) | ((cts & 0x000000FF) << 16) |
	          (cts & 0x0000FF00);

		pts = dts + cts * 90;
		
		*p++ = 0xc0;
		//pes pts_dts_len
		*p++ = 0x0a;

		write_pts(p, 0x0c >> 6, pts);
        p += 5;

		write_pts(p, 1, dts);
        p += 5;

		if((packet->m_body[9] & 0x1f) == 0x06){
			//NALU AUD
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x01;
			*p++ = 0x09;
			*p++ = 0xf0;

		} else if((packet->m_body[9] & 0x1f) == 0x05) {
			//NALU AUD
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x01;
			*p++ = 0x09;
			*p++ = 0xf0;

			//sps
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x01;
			memcpy(p, sps, nsps);
			p += nsps;

			//pps
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x00;
			*p++ = 0x01;
			memcpy(p, pps, npps);
			p += npps;
		}

		//Nalu start_code
		*p++ = 0x00;
		*p++ = 0x00;
		*p++ = 0x00;
		*p++ = 0x01;
	}else{
		
		//pes stream_id
		*p++ = 0xc0;

		pts = (int64_t)packet->m_nTimeStamp * 90;
		
		//pes packet_en
		if(slen+3+5+7 > 0xffff){
			*p++ = 0x00;
			*p++ = 0x00;
		} else {
			*p++ = (slen+3+5+7) >> 8;
			*p++ = (slen+3+5+7);
		}

		//pes flag
		*p++ = 0x80;

		*p++ = 0x80;
		//pes pts_len
		*p++ = 0x05;
		
		write_pts(p, 0x80 >> 6, pts);
        p += 5;

		p += adts_write_frame_header(p, aac->aac_profile, aac->sampling_frequency_index, 
			aac->channel_configuration, slen);
	}

	memcpy(p, src, slen);
	p += slen;

	return p-buf;
}


/*
	AudioSpecificConfig :
	AAC Profile 5bits | 采样率 4bits | 声道数 4bits | 其他 3bits |
*/

void Parse_AacConfigration(AudioSpecificConfig *aacc, RTMPPacket *packet)
{
	aacc->aac_profile = ((packet->m_body[2] & 0xF8) >> 3) & 0x1f;
	aacc->sampling_frequency_index = (((packet->m_body[2] & 0x07) << 1) | ((packet->m_body[3] >> 7) & 0x01)) & 0x0f;
	aacc->channel_configuration = (packet->m_body[3] >> 3) & 0x0F;
	aacc->framelength_flag = (packet->m_body[3] >> 2) & 0x01;
	aacc->depends_on_core_coder = (packet->m_body[3] >> 1) & 0x01;
	aacc->extension_flag = packet->m_body[3] & 0x01;

	printf("profile = %d, sfreidx = %d, channelcfg = %d, framelength = %d, docc = %d, extern = %d\n",
		aacc->aac_profile, aacc->sampling_frequency_index, 
		aacc->channel_configuration, aacc->framelength_flag, 
		aacc->depends_on_core_coder, aacc->extension_flag);

	return;
}

void Parse_AvcConfigration(Tag_Video_AvcC *avcc, RTMPPacket *packet)
{
	int i;
	char hc, lc;
	unsigned short num;
	if(packet->m_body[0] != 0x17 || packet->m_body[1] != 0x00)
		return;

	i = 5;
	avcc->configurationVersion = packet->m_body[i++];						//0x01
	avcc->AVCProfileIndication = packet->m_body[i++];						//0x64
	avcc->profile_compatibility = packet->m_body[i++];						//0x00
	avcc->AVCLevelIndication = packet->m_body[i++];							//0x1e
	//avcc->reserved_1 = p++;									
	avcc->lengthSizeMinusOne = packet->m_body[i++];							//0xff
	//avcc->reserved_2 = p++;									
	avcc->numOfSequenceParameterSets = packet->m_body[i++];					//0xe1

	//SPS
	hc = packet->m_body[i++];												//0x00
	lc = packet->m_body[i++];												//0x18
	num = (hc << 8) | lc;
	avcc->sequenceParameterSetLength = num;
	avcc->sequenceParameterSetNALUnit = calloc(num, sizeof(char));
	memcpy(avcc->sequenceParameterSetNALUnit, packet->m_body+i, num);		//
	i += num;


	//PPS
	avcc->numOfPictureParameterSets = packet->m_body[i++];					//0x01
	hc = packet->m_body[i++];												//0x00
	lc = packet->m_body[i++];												//0x06
	num = (hc << 8) | lc;
	avcc->pictureParameterSetLength = num;
	avcc->pictureParameterSetNALUnit = calloc(num, sizeof(char));
	memcpy(avcc->pictureParameterSetNALUnit, packet->m_body+i, num);

	return;
}


void RTMPPacket_Copy1(RTMPPacket *dst, RTMPPacket *src)
{
	dst->m_headerType = src->m_headerType;				
    dst->m_packetType = src->m_packetType;				
    dst->m_hasAbsTimestamp = src->m_hasAbsTimestamp;			
    dst->m_nChannel = src->m_nChannel;						
    dst->m_nTimeStamp = src->m_nTimeStamp;				
    dst->m_nInfoField2 = src->m_nInfoField2;
    dst->m_nBodySize = src->m_nBodySize;
    dst->m_nBytesRead = src->m_nBytesRead;

	// RTMP HEADER
	memcpy(dst->m_body-RTMP_MAX_HEADER_SIZE, 
	src->m_body-RTMP_MAX_HEADER_SIZE, dst->m_nBodySize+RTMP_MAX_HEADER_SIZE);

	#if 0
	//chunk
	if(src->m_chunk){
		dst->m_chunk = malloc(sizeof(RTMPChunk));
		dst->m_chunk->c_headerSize = src->m_chunk->c_headerSize;
		if(src->m_chunk->c_headerSize)
			memcpy(dst->m_chunk->c_header, src->m_chunk->c_header, src->m_chunk->c_headerSize);
		if(src->m_chunk->c_chunkSize){
			dst->m_chunk->c_chunkSize = src->m_chunk->c_chunkSize;
			dst->m_chunk->c_chunk = malloc(src->m_chunk->c_chunkSize);
			memcpy(dst->m_chunk->c_chunk, src->m_chunk->c_chunk, src->m_chunk->c_chunkSize);
		}
	}
	#endif
}


void RTMPPacket_Copy(RTMPPacket *dst, RTMPPacket *src)
{
	int datasize;
	unsigned ts, basets;
	char *ptr;
	dst->m_headerType = src->m_headerType;				
    dst->m_packetType = src->m_packetType;				
    dst->m_hasAbsTimestamp = src->m_hasAbsTimestamp;			
    dst->m_nChannel = src->m_nChannel;						
    dst->m_nTimeStamp = src->m_nTimeStamp;				
    dst->m_nInfoField2 = src->m_nInfoField2;
    dst->m_nBodySize = src->m_nBodySize;
    dst->m_nBytesRead = src->m_nBytesRead;

	ptr = dst->m_body;

	// RTMP HEADER
	memcpy(dst->m_body-RTMP_MAX_HEADER_SIZE, src->m_body-RTMP_MAX_HEADER_SIZE, RTMP_MAX_HEADER_SIZE);

	//1.  Reserved(2b)|Filter(1b)|TagType(5b)
	switch (src->m_packetType)
	{
		case RTMP_PACKET_TYPE_AUDIO: 
			*ptr++ = 0x08;
			break;

		case RTMP_PACKET_TYPE_VIDEO: 
			*ptr++ = 0x09;
			break;

		case RTMP_PACKET_TYPE_INFO: 
			*ptr++ = 0x12;
			break;

		default:
			break;
	}

	dst->m_nBodySize+=RTMP_FLV_TAG_HEADER_SIZE+RTMP_FLV_PREVIOUS_TAG_SIZE;

	//2. DataSize(3byte)
	datasize = src->m_nBodySize;
	*ptr++ = datasize >> 16;
	*ptr++ = (uint8_t)(datasize>>8);
	*ptr++ = (uint8_t)datasize;

	//3. Timestamp(3byte)
	ts = src->m_nTimeStamp;
	basets = ts & 0xFFFFFF;
	
	*ptr++ = basets >> 16;
	*ptr++ = (uint8_t)(basets>>8);
	*ptr++ = (uint8_t)basets;
	
	//4. TimestampExtended(1byte)
	*ptr++ = (ts >> 24) & 0x7F;

	//5. StreamID(3byte)
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;

	//6. Data
	if(src->m_nBodySize){
		memcpy(ptr, src->m_body, src->m_nBodySize);
	}
	ptr += src->m_nBodySize;
	
	//7. PreviousTagSize
	*ptr++ = 			datasize >> 24;
	*ptr++ = (uint8_t)(datasize >> 16);
	*ptr++ = (uint8_t)(datasize >> 8);
	*ptr++ = (uint8_t) datasize;

	
	//chunk
	if(src->m_chunk){
		dst->m_chunk = malloc(sizeof(RTMPChunk));
		dst->m_chunk->c_headerSize = src->m_chunk->c_headerSize;
		if(src->m_chunk->c_headerSize)
			memcpy(dst->m_chunk->c_header, src->m_chunk->c_header, src->m_chunk->c_headerSize);
		if(src->m_chunk->c_chunkSize){
			dst->m_chunk->c_chunkSize = src->m_chunk->c_chunkSize;
			dst->m_chunk->c_chunk = malloc(src->m_chunk->c_chunkSize);
			memcpy(dst->m_chunk->c_chunk, src->m_chunk->c_chunk, src->m_chunk->c_chunkSize);
		}
	}
		
}

void RTMPPacket_Copy_Free(RTMPPacket *p)
{
	if(p->m_chunk){
		if(p->m_chunk->c_chunkSize){
			free(p->m_chunk->c_chunk);
			p->m_chunk->c_chunk = NULL;
		}
			
		free(p->m_chunk);
		p->m_chunk = NULL;
	}
	RTMPPacket_Free(p);

	//free(p);
	//p = NULL;
}


void
	RTMPPacket_Reset(RTMPPacket *p)
{
	p->m_headerType = 0;
	p->m_packetType = 0;
	p->m_nChannel = 0;
	p->m_nTimeStamp = 0;
	p->m_nInfoField2 = 0;
	p->m_hasAbsTimestamp = FALSE;
	p->m_nBodySize = 0;
	p->m_nBytesRead = 0;
}

int
	RTMPPacket_Alloc(RTMPPacket *p, uint32_t nSize)
{
	char *ptr;
	if (nSize > SIZE_MAX - RTMP_MAX_HEADER_SIZE)
		return FALSE;
	ptr = calloc(1, nSize + RTMP_MAX_HEADER_SIZE);
	if (!ptr)
		return FALSE;
	p->m_body = ptr + RTMP_MAX_HEADER_SIZE;
	p->m_nBytesRead = 0;
	return TRUE;
}

void
	RTMPPacket_Free(RTMPPacket *p)
{
	if (p->m_body)
	{
		free(p->m_body - RTMP_MAX_HEADER_SIZE);
		p->m_body = NULL;
	}
}

void
	RTMPPacket_Dump(RTMPPacket *p)
{
	RTMP_Log(RTMP_LOGDEBUG,
		"RTMP PACKET: packet type: 0x%02x. channel: 0x%02x. info 1: %d info 2: %d. Body size: %u. body: 0x%02x",
		p->m_packetType, p->m_nChannel, p->m_nTimeStamp, p->m_nInfoField2,
		p->m_nBodySize, p->m_body ? (unsigned char)p->m_body[0] : 0);
}

int
	RTMP_LibVersion()
{
	return RTMP_LIB_VERSION;
}

void
	RTMP_TLS_Init()
{
#ifdef CRYPTO
#ifdef USE_POLARSSL
	/* Do this regardless of NO_SSL, we use havege for rtmpe too */
	RTMP_TLS_ctx = calloc(1,sizeof(struct tls_ctx));
	havege_init(&RTMP_TLS_ctx->hs);
#elif defined(USE_GNUTLS) && !defined(NO_SSL)
	/* Technically we need to initialize libgcrypt ourselves if
	* we're not going to call gnutls_global_init(). Ignoring this
	* for now.
	*/
	gnutls_global_init();
	RTMP_TLS_ctx = malloc(sizeof(struct tls_ctx));
	gnutls_certificate_allocate_credentials(&RTMP_TLS_ctx->cred);
	gnutls_priority_init(&RTMP_TLS_ctx->prios, "NORMAL", NULL);
	gnutls_certificate_set_x509_trust_file(RTMP_TLS_ctx->cred,
		"ca.pem", GNUTLS_X509_FMT_PEM);
#elif !defined(NO_SSL) /* USE_OPENSSL */
	/* libcrypto doesn't need anything special */
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_digests();
	RTMP_TLS_ctx = SSL_CTX_new(SSLv23_method());
	SSL_CTX_set_options(RTMP_TLS_ctx, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(RTMP_TLS_ctx);
#endif
#endif
}

void *
	RTMP_TLS_AllocServerContext(const char* cert, const char* key)
{
	void *ctx = NULL;
#ifdef CRYPTO
	if (!RTMP_TLS_ctx)
		RTMP_TLS_Init();
#ifdef USE_POLARSSL
	tls_server_ctx *tc = ctx = calloc(1, sizeof(struct tls_server_ctx));
	tc->dhm_P = my_dhm_P;
	tc->dhm_G = my_dhm_G;
	tc->hs = &RTMP_TLS_ctx->hs;
	if (x509parse_crtfile(&tc->cert, cert)) {
		free(tc);
		return NULL;
	}
	if (x509parse_keyfile(&tc->key, key, NULL)) {
		x509_free(&tc->cert);
		free(tc);
		return NULL;
	}
#elif defined(USE_GNUTLS) && !defined(NO_SSL)
	gnutls_certificate_allocate_credentials((gnutls_certificate_credentials*) &ctx);
	if (gnutls_certificate_set_x509_key_file(ctx, cert, key, GNUTLS_X509_FMT_PEM) != 0) {
		gnutls_certificate_free_credentials(ctx);
		return NULL;
	}
#elif !defined(NO_SSL) /* USE_OPENSSL */
	ctx = SSL_CTX_new(SSLv23_server_method());
	if (!SSL_CTX_use_certificate_chain_file(ctx, cert)) {
		SSL_CTX_free(ctx);
		return NULL;
	}
	if (!SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM)) {
		SSL_CTX_free(ctx);
		return NULL;
	}
#endif
#endif
	return ctx;
}

void
	RTMP_TLS_FreeServerContext(void *ctx)
{
#ifdef CRYPTO
#ifdef USE_POLARSSL
	x509_free(&((tls_server_ctx*)ctx)->cert);
	rsa_free(&((tls_server_ctx*)ctx)->key);
	free(ctx);
#elif defined(USE_GNUTLS) && !defined(NO_SSL)
	gnutls_certificate_free_credentials(ctx);
#elif !defined(NO_SSL) /* USE_OPENSSL */
	SSL_CTX_free(ctx);
#endif
#endif
}

RTMP *
	RTMP_Alloc()
{
	return calloc(1, sizeof(RTMP));
}

void
	RTMP_Free(RTMP *r)
{
	free(r);
}

void
	RTMP_Init(RTMP *r)
{
#ifdef CRYPTO
	if (!RTMP_TLS_ctx)
		RTMP_TLS_Init();
#endif

	memset(r, 0, sizeof(RTMP));
	r->m_sb.sb_socket = -1;
	r->m_inChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	r->m_outChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	r->m_nBufferMS = 30000;
	r->m_nClientBW = 2500000;
	r->m_nClientBW2 = 2;
	r->m_nServerBW = 2500000;
	r->m_fAudioCodecs = 3191.0;
	r->m_fVideoCodecs = 252.0;
	r->Link.timeout = 30;
	r->Link.swfAge = 30;
}

void
	RTMP_EnableWrite(RTMP *r)
{
	r->Link.protocol |= RTMP_FEATURE_WRITE;
}

double
	RTMP_GetDuration(RTMP *r)
{
	return r->m_fDuration;
}

int
	RTMP_IsConnected(RTMP *r)
{
	return r->m_sb.sb_socket != -1;
}

int
	RTMP_Socket(RTMP *r)
{
	return r->m_sb.sb_socket;
}

int
	RTMP_IsTimedout(RTMP *r)
{
	return r->m_sb.sb_timedout;
}

void
	RTMP_SetBufferMS(RTMP *r, int size)
{
	r->m_nBufferMS = size;
}

void
	RTMP_UpdateBufferMS(RTMP *r)
{
	RTMP_SendCtrl(r, 3, r->m_stream_id, r->m_nBufferMS);
}

#undef OSS
#ifdef _WIN32
#define OSS	"WIN"
#elif defined(__sun__)
#define OSS	"SOL"
#elif defined(__APPLE__)
#define OSS	"MAC"
#elif defined(__linux__)
#define OSS	"LNX"
#else
#define OSS	"GNU"
#endif
#define DEF_VERSTR	OSS " 10,0,32,18"
static const char DEFAULT_FLASH_VER[] = DEF_VERSTR;
const AVal RTMP_DefaultFlashVer =
{ (char *)DEFAULT_FLASH_VER, sizeof(DEFAULT_FLASH_VER) - 1 };

static void
	SocksSetup(RTMP *r, AVal *sockshost)
{
	if (sockshost->av_len)
	{
		const char *socksport = strchr(sockshost->av_val, ':');
		char *hostname = strdup(sockshost->av_val);

		if (socksport)
			hostname[socksport - sockshost->av_val] = '\0';
		r->Link.sockshost.av_val = hostname;
		r->Link.sockshost.av_len = strlen(hostname);

		r->Link.socksport = socksport ? atoi(socksport + 1) : 1080;
		RTMP_Log(RTMP_LOGDEBUG, "Connecting via SOCKS proxy: %s:%d", r->Link.sockshost.av_val,
			r->Link.socksport);
	}
	else
	{
		r->Link.sockshost.av_val = NULL;
		r->Link.sockshost.av_len = 0;
		r->Link.socksport = 0;
	}
}

void
	RTMP_SetupStream(RTMP *r,
	int protocol,
	AVal *host,
	unsigned int port,
	AVal *sockshost,
	AVal *playpath,
	AVal *tcUrl,
	AVal *swfUrl,
	AVal *pageUrl,
	AVal *app,
	AVal *auth,
	AVal *swfSHA256Hash,
	uint32_t swfSize,
	AVal *flashVer,
	AVal *subscribepath,
	AVal *usherToken,
	int dStart,
	int dStop, int bLiveStream, long int timeout)
{
	RTMP_Log(RTMP_LOGDEBUG, "Protocol : %s", RTMPProtocolStrings[protocol&7]);
	RTMP_Log(RTMP_LOGDEBUG, "Hostname : %.*s", host->av_len, host->av_val);
	RTMP_Log(RTMP_LOGDEBUG, "Port     : %d", port);
	RTMP_Log(RTMP_LOGDEBUG, "Playpath : %s", playpath->av_val);

	if (tcUrl && tcUrl->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "tcUrl    : %s", tcUrl->av_val);
	if (swfUrl && swfUrl->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "swfUrl   : %s", swfUrl->av_val);
	if (pageUrl && pageUrl->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "pageUrl  : %s", pageUrl->av_val);
	if (app && app->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "app      : %.*s", app->av_len, app->av_val);
	if (auth && auth->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "auth     : %s", auth->av_val);
	if (subscribepath && subscribepath->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "subscribepath : %s", subscribepath->av_val);
	if (usherToken && usherToken->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "NetStream.Authenticate.UsherToken : %s", usherToken->av_val);
	if (flashVer && flashVer->av_val)
		RTMP_Log(RTMP_LOGDEBUG, "flashVer : %s", flashVer->av_val);
	if (dStart > 0)
		RTMP_Log(RTMP_LOGDEBUG, "StartTime     : %d msec", dStart);
	if (dStop > 0)
		RTMP_Log(RTMP_LOGDEBUG, "StopTime      : %d msec", dStop);

	RTMP_Log(RTMP_LOGDEBUG, "live     : %s", bLiveStream ? "yes" : "no");
	RTMP_Log(RTMP_LOGDEBUG, "timeout  : %ld sec", timeout);

#ifdef CRYPTO
	if (swfSHA256Hash != NULL && swfSize > 0)
	{
		memcpy(r->Link.SWFHash, swfSHA256Hash->av_val, sizeof(r->Link.SWFHash));
		r->Link.SWFSize = swfSize;
		RTMP_Log(RTMP_LOGDEBUG, "SWFSHA256:");
		RTMP_LogHex(RTMP_LOGDEBUG, r->Link.SWFHash, sizeof(r->Link.SWFHash));
		RTMP_Log(RTMP_LOGDEBUG, "SWFSize  : %u", r->Link.SWFSize);
	}
	else
	{
		r->Link.SWFSize = 0;
	}
#endif

	SocksSetup(r, sockshost);

	if (tcUrl && tcUrl->av_len)
		r->Link.tcUrl = *tcUrl;
	if (swfUrl && swfUrl->av_len)
		r->Link.swfUrl = *swfUrl;
	if (pageUrl && pageUrl->av_len)
		r->Link.pageUrl = *pageUrl;
	if (app && app->av_len)
		r->Link.app = *app;
	if (auth && auth->av_len)
	{
		r->Link.auth = *auth;
		r->Link.lFlags |= RTMP_LF_AUTH;
	}
	if (flashVer && flashVer->av_len)
		r->Link.flashVer = *flashVer;
	else
		r->Link.flashVer = RTMP_DefaultFlashVer;
	if (subscribepath && subscribepath->av_len)
		r->Link.subscribepath = *subscribepath;
	if (usherToken && usherToken->av_len)
		r->Link.usherToken = *usherToken;
	r->Link.seekTime = dStart;
	r->Link.stopTime = dStop;
	if (bLiveStream)
		r->Link.lFlags |= RTMP_LF_LIVE;
	r->Link.timeout = timeout;

	r->Link.protocol = protocol;
	r->Link.hostname = *host;
	r->Link.port = port;
	r->Link.playpath = *playpath;

	if (r->Link.port == 0)
	{
		if (protocol & RTMP_FEATURE_SSL)
			r->Link.port = 443;
		else if (protocol & RTMP_FEATURE_HTTP)
			r->Link.port = 80;
		else
			r->Link.port = 1935;
	}
}

enum { OPT_STR=0, OPT_INT, OPT_BOOL, OPT_CONN };
static const char *optinfo[] = {
	"string", "integer", "boolean", "AMF" };

#define OFF(x)	offsetof(struct RTMP,x)

static struct urlopt {
	AVal name;
	off_t off;
	int otype;
	int omisc;
	char *use;
} options[] = {
	{ AVC("socks"),     OFF(Link.sockshost),     OPT_STR, 0,
	"Use the specified SOCKS proxy" },
	{ AVC("app"),       OFF(Link.app),           OPT_STR, 0,
	"Name of target app on server" },
	{ AVC("tcUrl"),     OFF(Link.tcUrl),         OPT_STR, 0,
	"URL to played stream" },
	{ AVC("pageUrl"),   OFF(Link.pageUrl),       OPT_STR, 0,
	"URL of played media's web page" },
	{ AVC("swfUrl"),    OFF(Link.swfUrl),        OPT_STR, 0,
	"URL to player SWF file" },
	{ AVC("flashver"),  OFF(Link.flashVer),      OPT_STR, 0,
	"Flash version string (default " DEF_VERSTR ")" },
	{ AVC("conn"),      OFF(Link.extras),        OPT_CONN, 0,
	"Append arbitrary AMF data to Connect message" },
	{ AVC("playpath"),  OFF(Link.playpath),      OPT_STR, 0,
	"Path to target media on server" },
	{ AVC("playlist"),  OFF(Link.lFlags),        OPT_BOOL, RTMP_LF_PLST,
	"Set playlist before play command" },
	{ AVC("live"),      OFF(Link.lFlags),        OPT_BOOL, RTMP_LF_LIVE,
	"Stream is live, no seeking possible" },
	{ AVC("subscribe"), OFF(Link.subscribepath), OPT_STR, 0,
	"Stream to subscribe to" },
	{ AVC("jtv"), OFF(Link.usherToken),          OPT_STR, 0,
	"Justin.tv authentication token" },
	{ AVC("token"),     OFF(Link.token),	       OPT_STR, 0,
	"Key for SecureToken response" },
	{ AVC("swfVfy"),    OFF(Link.lFlags),        OPT_BOOL, RTMP_LF_SWFV,
	"Perform SWF Verification" },
	{ AVC("swfAge"),    OFF(Link.swfAge),        OPT_INT, 0,
	"Number of days to use cached SWF hash" },
	{ AVC("start"),     OFF(Link.seekTime),      OPT_INT, 0,
	"Stream start position in milliseconds" },
	{ AVC("stop"),      OFF(Link.stopTime),      OPT_INT, 0,
	"Stream stop position in milliseconds" },
	{ AVC("buffer"),    OFF(m_nBufferMS),        OPT_INT, 0,
	"Buffer time in milliseconds" },
	{ AVC("timeout"),   OFF(Link.timeout),       OPT_INT, 0,
	"Session timeout in seconds" },
	{ AVC("pubUser"),   OFF(Link.pubUser),       OPT_STR, 0,
	"Publisher username" },
	{ AVC("pubPasswd"), OFF(Link.pubPasswd),     OPT_STR, 0,
	"Publisher password" },
	{ {NULL,0}, 0, 0}
};

static const AVal truth[] = {
	AVC("1"),
	AVC("on"),
	AVC("yes"),
	AVC("true"),
	{0,0}
};

static void RTMP_OptUsage()
{
	int i;

	RTMP_Log(RTMP_LOGERROR, "Valid RTMP options are:\n");
	for (i=0; options[i].name.av_len; i++) {
		RTMP_Log(RTMP_LOGERROR, "%10s %-7s  %s\n", options[i].name.av_val,
			optinfo[options[i].otype], options[i].use);
	}
}

static int
	parseAMF(AMFObject *obj, AVal *av, int *depth)
{
	AMFObjectProperty prop = {{0,0}};
	int i;
	char *p, *arg = av->av_val;

	if (arg[1] == ':')
	{
		p = (char *)arg+2;
		switch(arg[0])
		{
		case 'B':
			prop.p_type = AMF_BOOLEAN;
			prop.p_vu.p_number = atoi(p);
			break;
		case 'S':
			prop.p_type = AMF_STRING;
			prop.p_vu.p_aval.av_val = p;
			prop.p_vu.p_aval.av_len = av->av_len - (p-arg);
			break;
		case 'N':
			prop.p_type = AMF_NUMBER;
			prop.p_vu.p_number = strtod(p, NULL);
			break;
		case 'Z':
			prop.p_type = AMF_NULL;
			break;
		case 'O':
			i = atoi(p);
			if (i)
			{
				prop.p_type = AMF_OBJECT;
			}
			else
			{
				(*depth)--;
				return 0;
			}
			break;
		default:
			return -1;
		}
	}
	else if (arg[2] == ':' && arg[0] == 'N')
	{
		p = strchr(arg+3, ':');
		if (!p || !*depth)
			return -1;
		prop.p_name.av_val = (char *)arg+3;
		prop.p_name.av_len = p - (arg+3);

		p++;
		switch(arg[1])
		{
		case 'B':
			prop.p_type = AMF_BOOLEAN;
			prop.p_vu.p_number = atoi(p);
			break;
		case 'S':
			prop.p_type = AMF_STRING;
			prop.p_vu.p_aval.av_val = p;
			prop.p_vu.p_aval.av_len = av->av_len - (p-arg);
			break;
		case 'N':
			prop.p_type = AMF_NUMBER;
			prop.p_vu.p_number = strtod(p, NULL);
			break;
		case 'O':
			prop.p_type = AMF_OBJECT;
			break;
		default:
			return -1;
		}
	}
	else
		return -1;

	if (*depth)
	{
		AMFObject *o2;
		for (i=0; i<*depth; i++)
		{
			o2 = &obj->o_props[obj->o_num-1].p_vu.p_object;
			obj = o2;
		}
	}
	AMF_AddProp(obj, &prop);
	if (prop.p_type == AMF_OBJECT)
		(*depth)++;
	return 0;
}

int RTMP_SetOpt(RTMP *r, const AVal *opt, AVal *arg)
{
	int i;
	void *v;

	for (i=0; options[i].name.av_len; i++) {
		if (opt->av_len != options[i].name.av_len) continue;
		if (strcasecmp(opt->av_val, options[i].name.av_val)) continue;
		v = (char *)r + options[i].off;
		switch(options[i].otype) {
		case OPT_STR: {
			AVal *aptr = v;
			*aptr = *arg; }
					  break;
		case OPT_INT: {
			long l = strtol(arg->av_val, NULL, 0);
			*(int *)v = l; }
					  break;
		case OPT_BOOL: {
			int j, fl;
			fl = *(int *)v;
			for (j=0; truth[j].av_len; j++) {
				if (arg->av_len != truth[j].av_len) continue;
				if (strcasecmp(arg->av_val, truth[j].av_val)) continue;
				fl |= options[i].omisc; break; }
			*(int *)v = fl;
					   }
					   break;
		case OPT_CONN:
			if (parseAMF(&r->Link.extras, arg, &r->Link.edepth))
				return FALSE;
			break;
		}
		break;
	}
	if (!options[i].name.av_len) {
		RTMP_Log(RTMP_LOGERROR, "Unknown option %s", opt->av_val);
		RTMP_OptUsage();
		return FALSE;
	}

	return TRUE;
}

int RTMP_SetupURL(RTMP *r, char *url)
{
	AVal opt, arg;
	char *p1, *p2, *ptr = strchr(url, ' ');
	int ret, len;
	unsigned int port = 0;

	if (ptr)
		*ptr = '\0';

	len = strlen(url);
	ret = RTMP_ParseURL(url, &r->Link.protocol, &r->Link.hostname,
		&port, &r->Link.playpath0, &r->Link.app);
	if (!ret)
		return ret;
	r->Link.port = port;
	r->Link.playpath = r->Link.playpath0;

	while (ptr) {
		*ptr++ = '\0';
		p1 = ptr;
		p2 = strchr(p1, '=');
		if (!p2)
			break;
		opt.av_val = p1;
		opt.av_len = p2 - p1;
		*p2++ = '\0';
		arg.av_val = p2;
		ptr = strchr(p2, ' ');
		if (ptr) {
			*ptr = '\0';
			arg.av_len = ptr - p2;
			/* skip repeated spaces */
			while(ptr[1] == ' ')
				*ptr++ = '\0';
		} else {
			arg.av_len = strlen(p2);
		}

		/* unescape */
		port = arg.av_len;
		for (p1=p2; port >0;) {
			if (*p1 == '\\') {
				unsigned int c;
				if (port < 3)
					return FALSE;
				sscanf(p1+1, "%02x", &c);
				*p2++ = c;
				port -= 3;
				p1 += 3;
			} else {
				*p2++ = *p1++;
				port--;
			}
		}
		arg.av_len = p2 - arg.av_val;

		ret = RTMP_SetOpt(r, &opt, &arg);
		if (!ret)
			return ret;
	}

	if (!r->Link.tcUrl.av_len)
	{
		r->Link.tcUrl.av_val = url;
		if (r->Link.app.av_len)
		{
			if (r->Link.app.av_val < url + len)
			{
				/* if app is part of original url, just use it */
				r->Link.tcUrl.av_len = r->Link.app.av_len + (r->Link.app.av_val - url);
			}
			else
			{
				len = r->Link.hostname.av_len + r->Link.app.av_len +
					sizeof("rtmpte://:65535/");
				r->Link.tcUrl.av_val = malloc(len);
				r->Link.tcUrl.av_len = snprintf(r->Link.tcUrl.av_val, len,
					"%s://%.*s:%d/%.*s",
					RTMPProtocolStringsLower[r->Link.protocol],
					r->Link.hostname.av_len, r->Link.hostname.av_val,
					r->Link.port,
					r->Link.app.av_len, r->Link.app.av_val);
				r->Link.lFlags |= RTMP_LF_FTCU;
			}
		}
		else
		{
			r->Link.tcUrl.av_len = strlen(url);
		}
	}

#ifdef CRYPTO
	if ((r->Link.lFlags & RTMP_LF_SWFV) && r->Link.swfUrl.av_len)
		RTMP_HashSWF(r->Link.swfUrl.av_val, &r->Link.SWFSize,
		(unsigned char *)r->Link.SWFHash, r->Link.swfAge);
#endif

	SocksSetup(r, &r->Link.sockshost);

	if (r->Link.port == 0)
	{
		if (r->Link.protocol & RTMP_FEATURE_SSL)
			r->Link.port = 443;
		else if (r->Link.protocol & RTMP_FEATURE_HTTP)
			r->Link.port = 80;
		else
			r->Link.port = 1935;
	}
	return TRUE;
}

static int
	add_addr_info(struct sockaddr_in *service, AVal *host, int port)
{
	char *hostname;
	int ret = TRUE;
	if (host->av_val[host->av_len])
	{
		hostname = malloc(host->av_len+1);
		memcpy(hostname, host->av_val, host->av_len);
		hostname[host->av_len] = '\0';
	}
	else
	{
		hostname = host->av_val;
	}

	service->sin_addr.s_addr = inet_addr(hostname);
	if (service->sin_addr.s_addr == INADDR_NONE)
	{
		struct hostent *host = gethostbyname(hostname);
		if (host == NULL || host->h_addr == NULL)
		{
			RTMP_Log(RTMP_LOGERROR, "Problem accessing the DNS. (addr: %s)", hostname);
			ret = FALSE;
			goto finish;
		}
		service->sin_addr = *(struct in_addr *)host->h_addr;
	}

	service->sin_port = htons(port);
finish:
	if (hostname != host->av_val)
		free(hostname);
	return ret;
}

int
	RTMP_Connect0(RTMP *r, struct sockaddr * service)
{
	int on = 1;
	r->m_sb.sb_timedout = FALSE;
	r->m_pausing = 0;
	r->m_fDuration = 0.0;

	r->m_sb.sb_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (r->m_sb.sb_socket != -1)
	{
		if (connect(r->m_sb.sb_socket, service, sizeof(struct sockaddr)) < 0)
		{
			int err = GetSockError();
			RTMP_Log(RTMP_LOGERROR, "%s, failed to connect socket. %d (%s)",
				__FUNCTION__, err, strerror(err));
			RTMP_Close(r);
			return FALSE;
		}

		if (r->Link.socksport)
		{
			RTMP_Log(RTMP_LOGDEBUG, "%s ... SOCKS negotiation", __FUNCTION__);
			if (!SocksNegotiate(r))
			{
				RTMP_Log(RTMP_LOGERROR, "%s, SOCKS negotiation failed.", __FUNCTION__);
				RTMP_Close(r);
				return FALSE;
			}
		}
	}
	else
	{
		RTMP_Log(RTMP_LOGERROR, "%s, failed to create socket. Error: %d", __FUNCTION__,
			GetSockError());
		return FALSE;
	}

	/* set timeout */
	{
		SET_RCVTIMEO(tv, r->Link.timeout);
		if (setsockopt
			(r->m_sb.sb_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)))
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Setting socket timeout to %ds failed!",
				__FUNCTION__, r->Link.timeout);
		}
	}

	setsockopt(r->m_sb.sb_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

	return TRUE;
}

int
	RTMP_TLS_Accept(RTMP *r, void *ctx)
{
#if defined(CRYPTO) && !defined(NO_SSL)
	TLS_server(ctx, r->m_sb.sb_ssl);
	TLS_setfd(r->m_sb.sb_ssl, r->m_sb.sb_socket);
	if (TLS_accept(r->m_sb.sb_ssl) < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, TLS_Connect failed", __FUNCTION__);
		return FALSE;
	}
	return TRUE;
#else
	return FALSE;
#endif
}

int
	RTMP_Connect1(RTMP *r, RTMPPacket *cp)
{
	if (r->Link.protocol & RTMP_FEATURE_SSL)
	{
#if defined(CRYPTO) && !defined(NO_SSL)
		TLS_client(RTMP_TLS_ctx, r->m_sb.sb_ssl);
		TLS_setfd(r->m_sb.sb_ssl, r->m_sb.sb_socket);
		if (TLS_connect(r->m_sb.sb_ssl) < 0)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, TLS_Connect failed", __FUNCTION__);
			RTMP_Close(r);
			return FALSE;
		}
#else
		RTMP_Log(RTMP_LOGERROR, "%s, no SSL/TLS support", __FUNCTION__);
		RTMP_Close(r);
		return FALSE;

#endif
	}
	if (r->Link.protocol & RTMP_FEATURE_HTTP)
	{
		r->m_msgCounter = 1;
		r->m_clientID.av_val = NULL;
		r->m_clientID.av_len = 0;
		HTTP_Post(r, RTMPT_OPEN, "", 1);
		if (HTTP_read(r, 1) != 0)
		{
			r->m_msgCounter = 0;
			RTMP_Log(RTMP_LOGDEBUG, "%s, Could not connect for handshake", __FUNCTION__);
			RTMP_Close(r);
			return 0;
		}
		r->m_msgCounter = 0;
	}
	RTMP_Log(RTMP_LOGDEBUG, "%s, ... connected, handshaking", __FUNCTION__);
	if (!HandShake(r, TRUE))
	{
		RTMP_Log(RTMP_LOGERROR, "%s, handshake failed.", __FUNCTION__);
		RTMP_Close(r);
		return FALSE;
	}
	RTMP_Log(RTMP_LOGDEBUG, "%s, handshaked", __FUNCTION__);

	if (!SendConnectPacket(r, cp))
	{
		RTMP_Log(RTMP_LOGERROR, "%s, RTMP connect failed.", __FUNCTION__);
		RTMP_Close(r);
		return FALSE;
	}
	return TRUE;
}

int
	RTMP_Connect(RTMP *r, RTMPPacket *cp)
{
	struct sockaddr_in service;
	if (!r->Link.hostname.av_len)
		return FALSE;

	memset(&service, 0, sizeof(struct sockaddr_in));
	service.sin_family = AF_INET;

	if (r->Link.socksport)
	{
		/* Connect via SOCKS */
		if (!add_addr_info(&service, &r->Link.sockshost, r->Link.socksport))
			return FALSE;
	}
	else
	{
		/* Connect directly */
		if (!add_addr_info(&service, &r->Link.hostname, r->Link.port))
			return FALSE;
	}

		//////////////////////  2   /////////////////////////
		
		//connect 1935
	if (!RTMP_Connect0(r, (struct sockaddr *)&service))
		return FALSE;

	r->m_bSendCounter = TRUE;


		//////////////////////  3   /////////////////////////
		
		//handshake connect
	return RTMP_Connect1(r, cp);
}

static int
	SocksNegotiate(RTMP *r)
{
	unsigned long addr;
	struct sockaddr_in service;
	memset(&service, 0, sizeof(struct sockaddr_in));

	add_addr_info(&service, &r->Link.hostname, r->Link.port);
	addr = htonl(service.sin_addr.s_addr);

	{
		char packet[] = {
			4, 1,			/* SOCKS 4, connect */
			(r->Link.port >> 8) & 0xFF,
			(r->Link.port) & 0xFF,
			(char)(addr >> 24) & 0xFF, (char)(addr >> 16) & 0xFF,
			(char)(addr >> 8) & 0xFF, (char)addr & 0xFF,
			0
		};				/* NULL terminate */

		WriteN(r, packet, sizeof packet);

		if (ReadN(r, packet, 8) != 8)
			return FALSE;

		if (packet[0] == 0 && packet[1] == 90)
		{
			return TRUE;
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR, "%s, SOCKS returned error code %d", __FUNCTION__, packet[1]);
			return FALSE;
		}
	}
}

int
	RTMP_ConnectStream(RTMP *r, int seekTime)
{
	RTMPPacket packet = { 0 };

	/* seekTime was already set by SetupStream / SetupURL.
	* This is only needed by ReconnectStream.
	*/
	if (seekTime > 0)
		r->Link.seekTime = seekTime;

	r->m_mediaChannel = 0;

	while (!r->m_bPlaying && RTMP_IsConnected(r) && RTMP_ReadPacket(r, &packet))
	{
		if (RTMPPacket_IsReady(&packet))
		{
			if (!packet.m_nBodySize)
				continue;
			if ((packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) ||
				(packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) ||
				(packet.m_packetType == RTMP_PACKET_TYPE_INFO))
			{
				RTMP_Log(RTMP_LOGWARNING, "Received FLV packet before play()! Ignoring.");
				RTMPPacket_Free(&packet);
				continue;
			}

			RTMP_ClientPacket(r, &packet);
			RTMPPacket_Free(&packet);
		}
	}

	return r->m_bPlaying;
}

int
	RTMP_ReconnectStream(RTMP *r, int seekTime)
{
	RTMP_DeleteStream(r);

	RTMP_SendCreateStream(r);

	return RTMP_ConnectStream(r, seekTime);
}

int
	RTMP_ToggleStream(RTMP *r)
{
	int res;

	if (!r->m_pausing)
	{
		if (RTMP_IsTimedout(r) && r->m_read.status == RTMP_READ_EOF)
			r->m_read.status = 0;

		res = RTMP_SendPause(r, TRUE, r->m_pauseStamp);
		if (!res)
			return res;

		r->m_pausing = 1;
		sleep(1);
	}
	res = RTMP_SendPause(r, FALSE, r->m_pauseStamp);
	r->m_pausing = 3;
	return res;
}

void
	RTMP_DeleteStream(RTMP *r)
{
	if (r->m_stream_id < 0)
		return;

	r->m_bPlaying = FALSE;

	SendDeleteStream(r, r->m_stream_id);
	r->m_stream_id = -1;
}

int
	RTMP_GetNextMediaPacket(RTMP *r, RTMPPacket *packet)
{
	int bHasMediaPacket = 0;

	while (!bHasMediaPacket && RTMP_IsConnected(r)
		&& RTMP_ReadPacket(r, packet))
	{
		if (!RTMPPacket_IsReady(packet) || !packet->m_nBodySize)
		{
			continue;
		}

		bHasMediaPacket = RTMP_ClientPacket(r, packet);

		if (!bHasMediaPacket)
		{
			RTMPPacket_Free(packet);
		}
		else if (r->m_pausing == 3)
		{
			if (packet->m_nTimeStamp <= r->m_mediaStamp)
			{
				bHasMediaPacket = 0;
#ifdef _DEBUG
				RTMP_Log(RTMP_LOGDEBUG,
					"Skipped type: %02X, size: %d, TS: %d ms, abs TS: %d, pause: %d ms",
					packet->m_packetType, packet->m_nBodySize,
					packet->m_nTimeStamp, packet->m_hasAbsTimestamp,
					r->m_mediaStamp);
#endif
				RTMPPacket_Free(packet);
				continue;
			}
			r->m_pausing = 0;
		}
	}

	if (bHasMediaPacket)
		r->m_bPlaying = TRUE;
	else if (r->m_sb.sb_timedout && !r->m_pausing)
		r->m_pauseStamp = r->m_mediaChannel < r->m_channelsAllocatedIn ?
		r->m_channelTimestamp[r->m_mediaChannel] : 0;

	return bHasMediaPacket;
}

int
	RTMP_ClientPacket(RTMP *r, RTMPPacket *packet)
{
	int bHasMediaPacket = 0;
	switch (packet->m_packetType)
	{
	case RTMP_PACKET_TYPE_CHUNK_SIZE:
		/* chunk size */
		HandleChangeChunkSize(r, packet);
		break;

	case RTMP_PACKET_TYPE_ACKNOWLEDGEMENT:
		/* bytes read report */
		RTMP_Log(RTMP_LOGDEBUG, "%s, received: bytes read report", __FUNCTION__);
		break;

	case RTMP_PACKET_TYPE_CONTROL:
		/* ctrl */
		HandleCtrl(r, packet);
		break;

	case RTMP_PACKET_TYPE_SET_WINDOW_ACK_SIZE:
		/* server bw */
		HandleServerBW(r, packet);
		break;

	case RTMP_PACKET_TYPE_SET_PEER_BW:
		/* client bw */
		HandleClientBW(r, packet);
		break;

	case RTMP_PACKET_TYPE_AUDIO:
		/* audio data */
		/*RTMP_Log(RTMP_LOGDEBUG, "%s, received: audio %lu bytes", __FUNCTION__, packet.m_nBodySize); */
		HandleAudio(r, packet);
		bHasMediaPacket = 1;
		if (!r->m_mediaChannel)
			r->m_mediaChannel = packet->m_nChannel;
		if (!r->m_pausing)
			r->m_mediaStamp = packet->m_nTimeStamp;
		break;

	case RTMP_PACKET_TYPE_VIDEO:
		/* video data */
		/*RTMP_Log(RTMP_LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet.m_nBodySize); */
		HandleVideo(r, packet);
		bHasMediaPacket = 1;
		if (!r->m_mediaChannel)
			r->m_mediaChannel = packet->m_nChannel;
		if (!r->m_pausing)
			r->m_mediaStamp = packet->m_nTimeStamp;
		break;

	case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
		/* flex stream send */
		RTMP_Log(RTMP_LOGDEBUG,
			"%s, flex stream send, size %u bytes, not supported, ignoring",
			__FUNCTION__, packet->m_nBodySize);
		break;

	case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
		/* flex shared object */
		RTMP_Log(RTMP_LOGDEBUG,
			"%s, flex shared object, size %u bytes, not supported, ignoring",
			__FUNCTION__, packet->m_nBodySize);
		break;

	case RTMP_PACKET_TYPE_FLEX_MESSAGE:
		/* flex message */
		{
			RTMP_Log(RTMP_LOGDEBUG,
				"%s, flex message, size %u bytes, not fully supported",
				__FUNCTION__, packet->m_nBodySize);
			/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

			/* some DEBUG code */
#if 0
			RTMP_LIB_AMFObject obj;
			int nRes = obj.Decode(packet.m_body+1, packet.m_nBodySize-1);
			if(nRes < 0) {
				RTMP_Log(RTMP_LOGERROR, "%s, error decoding AMF3 packet", __FUNCTION__);
				/*return; */
			}

			obj.Dump();
#endif

			if (HandleInvoke(r, packet->m_body + 1, packet->m_nBodySize - 1) == 1)
				bHasMediaPacket = 2;
			break;
		}
	case RTMP_PACKET_TYPE_INFO:
		/* metadata (notify) */
		RTMP_Log(RTMP_LOGDEBUG, "%s, received: notify %u bytes", __FUNCTION__,
			packet->m_nBodySize);
		if (HandleMetadata(r, packet->m_body, packet->m_nBodySize))
			bHasMediaPacket = 1;
		break;

	case RTMP_PACKET_TYPE_SHARED_OBJECT:
		RTMP_Log(RTMP_LOGDEBUG, "%s, shared object, not supported, ignoring",
			__FUNCTION__);
		break;

	case RTMP_PACKET_TYPE_INVOKE:
		/* invoke */
		RTMP_Log(RTMP_LOGDEBUG, "%s, received: invoke %u bytes", __FUNCTION__,
			packet->m_nBodySize);
		/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

		if (HandleInvoke(r, packet->m_body, packet->m_nBodySize) == 1)
			bHasMediaPacket = 2;
		break;

	case RTMP_PACKET_TYPE_FLASH_VIDEO:
		{
			/* go through FLV packets and handle metadata packets */
			unsigned int pos = 0;
			uint32_t nTimeStamp = packet->m_nTimeStamp;

			while (pos + 11 < packet->m_nBodySize)
			{
				uint32_t dataSize = AMF_DecodeInt24(packet->m_body + pos + 1);	/* size without header (11) and prevTagSize (4) */

				if (pos + 11 + dataSize + 4 > packet->m_nBodySize)
				{
					RTMP_Log(RTMP_LOGWARNING, "Stream corrupt?!");
					break;
				}
				if (packet->m_body[pos] == 0x12)
				{
					HandleMetadata(r, packet->m_body + pos + 11, dataSize);
				}
				else if (packet->m_body[pos] == 8 || packet->m_body[pos] == 9)
				{
					nTimeStamp = AMF_DecodeInt24(packet->m_body + pos + 4);
					nTimeStamp |= (packet->m_body[pos + 7] << 24);
				}
				pos += (11 + dataSize + 4);
			}
			if (!r->m_pausing)
				r->m_mediaStamp = nTimeStamp;

			/* FLV tag(s) */
			/*RTMP_Log(RTMP_LOGDEBUG, "%s, received: FLV tag(s) %lu bytes", __FUNCTION__, packet.m_nBodySize); */
			bHasMediaPacket = 1;
			break;
		}
	default:
		RTMP_Log(RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
			packet->m_packetType);
#ifdef _DEBUG
		RTMP_LogHex(RTMP_LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
	}

	return bHasMediaPacket;
}

#ifdef _DEBUG
extern FILE *netstackdump;
extern FILE *netstackdump_read;
#endif

static int
	ReadN(RTMP *r, char *buffer, int n)
{
	int nOriginalSize = n;
	int avail;
	char *ptr;

	r->m_sb.sb_timedout = FALSE;

#ifdef _DEBUG
	memset(buffer, 0, n);
#endif

	ptr = buffer;
	while (n > 0)
	{
		int nBytes = 0, nRead;
		if (r->Link.protocol & RTMP_FEATURE_HTTP)
		{
			int refill = 0;
			while (!r->m_resplen)
			{
				int ret;
				if (r->m_sb.sb_size < 13 || refill)
				{
					if (!r->m_unackd)
						HTTP_Post(r, RTMPT_IDLE, "", 1);
					if (RTMPSockBuf_Fill(&r->m_sb) < 1)
					{
						if (!r->m_sb.sb_timedout)
							RTMP_Close(r);
						return 0;
					}
				}
				if ((ret = HTTP_read(r, 0)) == -1)
				{
					RTMP_Log(RTMP_LOGDEBUG, "%s, No valid HTTP response found", __FUNCTION__);
					RTMP_Close(r);
					return 0;
				}
				else if (ret == -2)
				{
					refill = 1;
				}
				else
				{
					refill = 0;
				}
			}
			if (r->m_resplen && !r->m_sb.sb_size)
				RTMPSockBuf_Fill(&r->m_sb);
			avail = r->m_sb.sb_size;
			if (avail > r->m_resplen)
				avail = r->m_resplen;
		}
		else
		{
			avail = r->m_sb.sb_size;
			if (avail == 0)
			{
				if (RTMPSockBuf_Fill(&r->m_sb) < 1)
				{
					if (!r->m_sb.sb_timedout)
						RTMP_Close(r);
					return 0;
				}
				avail = r->m_sb.sb_size;
			}
		}
		nRead = ((n < avail) ? n : avail);
		if (nRead > 0)
		{
			memcpy(ptr, r->m_sb.sb_start, nRead);
			r->m_sb.sb_start += nRead;
			r->m_sb.sb_size -= nRead;
			nBytes = nRead;
			r->m_nBytesIn += nRead;
			if (r->m_bSendCounter && r->m_nBytesIn > ( r->m_nBytesInSent + r->m_nClientBW / 10))
				if (!SendBytesReceived(r))
					return FALSE;
		}
		/*RTMP_Log(RTMP_LOGDEBUG, "%s: %d bytes\n", __FUNCTION__, nBytes); */
#ifdef _DEBUG
		fwrite(ptr, 1, nBytes, netstackdump_read);
#endif

		if (nBytes == 0)
		{
			RTMP_Log(RTMP_LOGDEBUG, "%s, RTMP socket closed by peer", __FUNCTION__);
			/*goto again; */
			RTMP_Close(r);
			break;
		}

		if (r->Link.protocol & RTMP_FEATURE_HTTP)
			r->m_resplen -= nBytes;

#ifdef CRYPTO
		if (r->Link.rc4keyIn)
		{
			RC4_encrypt(r->Link.rc4keyIn, nBytes, ptr);
		}
#endif

		n -= nBytes;
		ptr += nBytes;
	}

	return nOriginalSize - n;
}

static int
	WriteN(RTMP *r, const char *buffer, int n)
{
	const char *ptr = buffer;
#ifdef CRYPTO
	char *encrypted = 0;
	char buf[RTMP_BUFFER_CACHE_SIZE];

	if (r->Link.rc4keyOut)
	{
		if (n > sizeof(buf))
			encrypted = (char *)malloc(n);
		else
			encrypted = (char *)buf;
		ptr = encrypted;
		RC4_encrypt2(r->Link.rc4keyOut, n, buffer, ptr);
	}
#endif

	while (n > 0)
	{
		int nBytes;

		if (r->Link.protocol & RTMP_FEATURE_HTTP)
			nBytes = HTTP_Post(r, RTMPT_SEND, ptr, n);
		else
			nBytes = RTMPSockBuf_Send(&r->m_sb, ptr, n);
		/*RTMP_Log(RTMP_LOGDEBUG, "%s: %d\n", __FUNCTION__, nBytes); */

		if (nBytes < 0)
		{
			int sockerr = GetSockError();
			RTMP_Log(RTMP_LOGERROR, "%s, RTMP send error %d (%d bytes)", __FUNCTION__,
				sockerr, n);

			if (sockerr == EINTR && !RTMP_ctrlC)
				continue;

			RTMP_Close(r);
			n = 1;
			break;
		}

		if (nBytes == 0)
			break;

		n -= nBytes;
		ptr += nBytes;
	}

#ifdef CRYPTO
	if (encrypted && encrypted != buf)
		free(encrypted);
#endif

	return n == 0;
}

#define SAVC(x)	static const AVal av_##x = AVC(#x)

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(secureToken);
SAVC(secureTokenResponse);
SAVC(type);
SAVC(nonprivate);

static int
	SendConnectPacket(RTMP *r, RTMPPacket *cp)
{
	RTMPPacket packet;
	char pbuf[4096], *pend = pbuf + sizeof(pbuf);
	char *enc;

	if (cp)
		return RTMP_SendPacket(r, cp, TRUE);

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_connect);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_OBJECT;

	enc = AMF_EncodeNamedString(enc, pend, &av_app, &r->Link.app);	//rtmp app : live
	if (!enc)
		return FALSE;
	if (r->Link.protocol & RTMP_FEATURE_WRITE)
	{
		enc = AMF_EncodeNamedString(enc, pend, &av_type, &av_nonprivate);
		if (!enc)
			return FALSE;
	}
	if (r->Link.flashVer.av_len)
	{
		enc = AMF_EncodeNamedString(enc, pend, &av_flashVer, &r->Link.flashVer);
		if (!enc)
			return FALSE;
	}
	if (r->Link.swfUrl.av_len)
	{
		enc = AMF_EncodeNamedString(enc, pend, &av_swfUrl, &r->Link.swfUrl);
		if (!enc)
			return FALSE;
	}
	if (r->Link.tcUrl.av_len)
	{
		enc = AMF_EncodeNamedString(enc, pend, &av_tcUrl, &r->Link.tcUrl);
		if (!enc)
			return FALSE;
	}
	if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
	{
		enc = AMF_EncodeNamedBoolean(enc, pend, &av_fpad, FALSE);
		if (!enc)
			return FALSE;
		enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 15.0);
		if (!enc)
			return FALSE;
		enc = AMF_EncodeNamedNumber(enc, pend, &av_audioCodecs, r->m_fAudioCodecs);
		if (!enc)
			return FALSE;
		enc = AMF_EncodeNamedNumber(enc, pend, &av_videoCodecs, r->m_fVideoCodecs);
		if (!enc)
			return FALSE;
		enc = AMF_EncodeNamedNumber(enc, pend, &av_videoFunction, 1.0);
		if (!enc)
			return FALSE;
		if (r->Link.pageUrl.av_len)
		{
			enc = AMF_EncodeNamedString(enc, pend, &av_pageUrl, &r->Link.pageUrl);
			if (!enc)
				return FALSE;
		}
	}
	if (r->m_fEncoding != 0.0 || r->m_bSendEncoding)
	{	/* AMF0, AMF3 not fully supported yet */
		enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
		if (!enc)
			return FALSE;
	}
	if (enc + 3 >= pend)
		return FALSE;
	*enc++ = 0;
	*enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
	*enc++ = AMF_OBJECT_END;

	/* add auth string */
	if (r->Link.auth.av_len)
	{
		enc = AMF_EncodeBoolean(enc, pend, r->Link.lFlags & RTMP_LF_AUTH);
		if (!enc)
			return FALSE;
		enc = AMF_EncodeString(enc, pend, &r->Link.auth);
		if (!enc)
			return FALSE;
	}
	if (r->Link.extras.o_num)
	{
		int i;
		for (i = 0; i < r->Link.extras.o_num; i++)
		{
			enc = AMFProp_Encode(&r->Link.extras.o_props[i], enc, pend);
			if (!enc)
				return FALSE;
		}
	}
	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

#if 0				/* unused */
SAVC(bgHasStream);

static int
	SendBGHasStream(RTMP *r, double dId, AVal *playpath)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_bgHasStream);
	enc = AMF_EncodeNumber(enc, pend, dId);
	*enc++ = AMF_NULL;

	enc = AMF_EncodeString(enc, pend, playpath);
	if (enc == NULL)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}
#endif

SAVC(createStream);

int
	RTMP_SendCreateStream(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_createStream);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;		/* NULL */

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

SAVC(FCSubscribe);

static int
	SendFCSubscribe(RTMP *r, AVal *subscribepath)
{
	RTMPPacket packet;
	char pbuf[512], *pend = pbuf + sizeof(pbuf);
	char *enc;
	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	RTMP_Log(RTMP_LOGDEBUG, "FCSubscribe: %s", subscribepath->av_val);
	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_FCSubscribe);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, subscribepath);

	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

/* Justin.tv specific authentication */
static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");

static int
	SendUsherToken(RTMP *r, AVal *usherToken)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;
	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	RTMP_Log(RTMP_LOGDEBUG, "UsherToken: %s", usherToken->av_val);
	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_NetStream_Authenticate_UsherToken);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, usherToken);

	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}
/******************************************/

SAVC(releaseStream);

static int
	SendReleaseStream(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_releaseStream);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(FCPublish);

static int
	SendFCPublish(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_FCPublish);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(FCUnpublish);

static int
	SendFCUnpublish(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_FCUnpublish);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, &r->Link.playpath);
	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(publish);
SAVC(live);
SAVC(record);

static int
	SendPublish(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x04;	/* source channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = r->m_stream_id;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_publish);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, &r->Link.playpath);	//stream name : tmp
	if (!enc)
		return FALSE;

	/* FIXME: should we choose live based on Link.lFlags & RTMP_LF_LIVE? */
	enc = AMF_EncodeString(enc, pend, &av_live);
	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

SAVC(deleteStream);

static int
	SendDeleteStream(RTMP *r, double dStreamId)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_deleteStream);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeNumber(enc, pend, dStreamId);

	packet.m_nBodySize = enc - packet.m_body;

	/* no response expected */
	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(pause);

int
	RTMP_SendPause(RTMP *r, int DoPause, int iTime)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x08;	/* video channel */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_pause);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeBoolean(enc, pend, DoPause);
	enc = AMF_EncodeNumber(enc, pend, (double)iTime);

	packet.m_nBodySize = enc - packet.m_body;

	RTMP_Log(RTMP_LOGDEBUG, "%s, %d, pauseTime=%d", __FUNCTION__, DoPause, iTime);
	return RTMP_SendPacket(r, &packet, TRUE);
}

int RTMP_Pause(RTMP *r, int DoPause)
{
	if (DoPause)
		r->m_pauseStamp = r->m_mediaChannel < r->m_channelsAllocatedIn ?
		r->m_channelTimestamp[r->m_mediaChannel] : 0;
	return RTMP_SendPause(r, DoPause, r->m_pauseStamp);
}

SAVC(seek);

int
	RTMP_SendSeek(RTMP *r, int iTime)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x08;	/* video channel */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_seek);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeNumber(enc, pend, (double)iTime);

	packet.m_nBodySize = enc - packet.m_body;

	r->m_read.flags |= RTMP_READ_SEEKING;
	r->m_read.nResumeTS = 0;

	return RTMP_SendPacket(r, &packet, TRUE);
}

int
	RTMP_SendServerBW(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);

	packet.m_nChannel = 0x02;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_SET_WINDOW_ACK_SIZE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 4;

	AMF_EncodeInt32(packet.m_body, pend, r->m_nServerBW);
	return RTMP_SendPacket(r, &packet, FALSE);
}

int
	RTMP_SendClientBW(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);

	packet.m_nChannel = 0x02;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_SET_PEER_BW;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 5;

	AMF_EncodeInt32(packet.m_body, pend, r->m_nClientBW);
	packet.m_body[4] = r->m_nClientBW2;
	return RTMP_SendPacket(r, &packet, FALSE);
}


int RTMP_Send_Set_ChunkSize(RTMP *r){
    RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);

	packet.m_nChannel = 0x02;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 4;

	AMF_EncodeInt32(packet.m_body, pend, r->m_outChunkSize);
	return RTMP_SendPacket(r, &packet, FALSE);
}

static int
	SendBytesReceived(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);

	packet.m_nChannel = 0x02;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_ACKNOWLEDGEMENT;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	packet.m_nBodySize = 4;

	AMF_EncodeInt32(packet.m_body, pend, r->m_nBytesIn);	/* hard coded for now */
	r->m_nBytesInSent = r->m_nBytesIn;

	/*RTMP_Log(RTMP_LOGDEBUG, "Send bytes report. 0x%x (%d bytes)", (unsigned int)m_nBytesIn, m_nBytesIn); */
	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(_checkbw);

static int
	SendCheckBW(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__checkbw);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;

	packet.m_nBodySize = enc - packet.m_body;

	/* triggers _onbwcheck and eventually results in _onbwdone */
	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(_result);

static int
	SendCheckBWResult(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0x16 * r->m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeNumber(enc, pend, (double)r->m_nBWCheckCounter++);

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(ping);
SAVC(pong);

static int
	SendPong(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0x16 * r->m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_pong);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

SAVC(play);

static int
	SendPlay(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x08;	/* we make 8 our stream channel */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = r->m_stream_id;	/*0x01000000; */
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_play);
	enc = AMF_EncodeNumber(enc, pend, ++r->m_numInvokes);
	*enc++ = AMF_NULL;

	RTMP_Log(RTMP_LOGDEBUG, "%s, seekTime=%d, stopTime=%d, sending play: %s",
		__FUNCTION__, r->Link.seekTime, r->Link.stopTime,
		r->Link.playpath.av_val);
	enc = AMF_EncodeString(enc, pend, &r->Link.playpath);	//stream name : tmp
	if (!enc)
		return FALSE;

	/* Optional parameters start and len.
	*
	* start: -2, -1, 0, positive number
	*  -2: looks for a live stream, then a recorded stream,
	*      if not found any open a live stream
	*  -1: plays a live stream
	* >=0: plays a recorded streams from 'start' milliseconds
	*/
	if (r->Link.lFlags & RTMP_LF_LIVE)
		enc = AMF_EncodeNumber(enc, pend, -1000.0);
	else
	{
		if (r->Link.seekTime > 0.0)
			enc = AMF_EncodeNumber(enc, pend, r->Link.seekTime);	/* resume from here */
		else
			enc = AMF_EncodeNumber(enc, pend, 0.0);	/*-2000.0);*/ /* recorded as default, -2000.0 is not reliable since that freezes the player if the stream is not found */
	}
	if (!enc)
		return FALSE;

	/* len: -1, 0, positive number
	*  -1: plays live or recorded stream to the end (default)
	*   0: plays a frame 'start' ms away from the beginning
	*  >0: plays a live or recoded stream for 'len' milliseconds
	*/
	/*enc += EncodeNumber(enc, -1.0); */ /* len */
	if (r->Link.stopTime)
	{
		enc = AMF_EncodeNumber(enc, pend, r->Link.stopTime - r->Link.seekTime);
		if (!enc)
			return FALSE;
	}

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

SAVC(set_playlist);
SAVC(0);

static int
	SendPlaylist(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x08;	/* we make 8 our stream channel */
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = r->m_stream_id;	/*0x01000000; */
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_set_playlist);
	enc = AMF_EncodeNumber(enc, pend, 0);
	*enc++ = AMF_NULL;
	*enc++ = AMF_ECMA_ARRAY;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT;
	enc = AMF_EncodeNamedString(enc, pend, &av_0, &r->Link.playpath);
	if (!enc)
		return FALSE;
	if (enc + 3 >= pend)
		return FALSE;
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, TRUE);
}

static int
	SendSecureTokenResponse(RTMP *r, AVal *resp)
{
	RTMPPacket packet;
	char pbuf[1024], *pend = pbuf + sizeof(pbuf);
	char *enc;

	packet.m_nChannel = 0x03;	/* control channel (invoke) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_secureTokenResponse);
	enc = AMF_EncodeNumber(enc, pend, 0.0);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeString(enc, pend, resp);
	if (!enc)
		return FALSE;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

/*
from http://jira.red5.org/confluence/display/docs/Ping:

Ping is the most mysterious message in RTMP and till now we haven't fully interpreted it yet. In summary, Ping message is used as a special command that are exchanged between client and server. This page aims to document all known Ping messages. Expect the list to grow.

The type of Ping packet is 0x4 and contains two mandatory parameters and two optional parameters. The first parameter is the type of Ping and in short integer. The second parameter is the target of the ping. As Ping is always sent in Channel 2 (control channel) and the target object in RTMP header is always 0 which means the Connection object, it's necessary to put an extra parameter to indicate the exact target object the Ping is sent to. The second parameter takes this responsibility. The value has the same meaning as the target object field in RTMP header. (The second value could also be used as other purposes, like RTT Ping/Pong. It is used as the timestamp.) The third and fourth parameters are optional and could be looked upon as the parameter of the Ping packet. Below is an unexhausted list of Ping messages.

* type 0: Clear the stream. No third and fourth parameters. The second parameter could be 0. After the connection is established, a Ping 0,0 will be sent from server to client. The message will also be sent to client on the start of Play and in response of a Seek or Pause/Resume request. This Ping tells client to re-calibrate the clock with the timestamp of the next packet server sends.
* type 1: Tell the stream to clear the playing buffer.
* type 3: Buffer time of the client. The third parameter is the buffer time in millisecond.
* type 4: Reset a stream. Used together with type 0 in the case of VOD. Often sent before type 0.
* type 6: Ping the client from server. The second parameter is the current time.
* type 7: Pong reply from client. The second parameter is the time the server sent with his ping request.
* type 26: SWFVerification request
* type 27: SWFVerification response
*/
int
	RTMP_SendCtrl(RTMP *r, short nType, unsigned int nObject, unsigned int nTime)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf + sizeof(pbuf);
	int nSize;
	char *buf;

	RTMP_Log(RTMP_LOGDEBUG, "sending ctrl. type: 0x%04x", (unsigned short)nType);

	packet.m_nChannel = 0x02;	/* control channel (ping) */
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet.m_packetType = RTMP_PACKET_TYPE_CONTROL;
	packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	switch(nType) {
	case 0x03: nSize = 10; break;	/* buffer time */
	case 0x1A: nSize = 3; break;	/* SWF verify request */
	case 0x1B: nSize = 44; break;	/* SWF verify response */
	default: nSize = 6; break;
	}

	packet.m_nBodySize = nSize;

	buf = packet.m_body;
	buf = AMF_EncodeInt16(buf, pend, nType);	//AMF3 type

	if (nType == 0x1B)
	{
#ifdef CRYPTO
		memcpy(buf, r->Link.SWFVerificationResponse, 42);
		RTMP_Log(RTMP_LOGDEBUG, "Sending SWFVerification response: ");
		RTMP_LogHex(RTMP_LOGDEBUG, (uint8_t *)packet.m_body, packet.m_nBodySize);
#endif
	}
	else if (nType == 0x1A)
	{
		*buf = nObject & 0xff;
	}
	else
	{
		if (nSize > 2)
			buf = AMF_EncodeInt32(buf, pend, nObject);	//AMF3 value

		if (nSize > 6)
			buf = AMF_EncodeInt32(buf, pend, nTime);
	}

	return RTMP_SendPacket(r, &packet, FALSE);
}

static void
	AV_erase(RTMP_METHOD *vals, int *num, int i, int freeit)
{
	if (freeit)
		free(vals[i].name.av_val);
	(*num)--;
	for (; i < *num; i++)
	{
		vals[i] = vals[i + 1];
	}
	vals[i].name.av_val = NULL;
	vals[i].name.av_len = 0;
	vals[i].num = 0;
}

void
	RTMP_DropRequest(RTMP *r, int i, int freeit)
{
	AV_erase(r->m_methodCalls, &r->m_numCalls, i, freeit);
}

static void
	AV_queue(RTMP_METHOD **vals, int *num, AVal *av, int txn)
{
	char *tmp;
	if (!(*num & 0x0f))
		*vals = realloc(*vals, (*num + 16) * sizeof(RTMP_METHOD));
	tmp = malloc(av->av_len + 1);
	memcpy(tmp, av->av_val, av->av_len);
	tmp[av->av_len] = '\0';
	(*vals)[*num].num = txn;
	(*vals)[*num].name.av_len = av->av_len;
	(*vals)[(*num)++].name.av_val = tmp;
}

static void
	AV_clear(RTMP_METHOD *vals, int num)
{
	int i;
	for (i = 0; i < num; i++)
		free(vals[i].name.av_val);
	free(vals);
}


#ifdef CRYPTO
static int
	b64enc(const unsigned char *input, int length, char *output, int maxsize)
{
#ifdef USE_POLARSSL
	size_t buf_size = maxsize;
	if(base64_encode((unsigned char *) output, &buf_size, input, length) == 0)
	{
		output[buf_size] = '\0';
		return 1;
	}
	else
	{
		RTMP_Log(RTMP_LOGDEBUG, "%s, error", __FUNCTION__);
		return 0;
	}
#elif defined(USE_GNUTLS)
	if (BASE64_ENCODE_RAW_LENGTH(length) <= maxsize)
		base64_encode_raw((uint8_t*) output, length, input);
	else
	{
		RTMP_Log(RTMP_LOGDEBUG, "%s, error", __FUNCTION__);
		return 0;
	}
#else   /* USE_OPENSSL */
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	if (BIO_flush(b64) == 1)
	{
		BIO_get_mem_ptr(b64, &bptr);
		memcpy(output, bptr->data, bptr->length-1);
		output[bptr->length-1] = '\0';
	}
	else
	{
		RTMP_Log(RTMP_LOGDEBUG, "%s, error", __FUNCTION__);
		return 0;
	}
	BIO_free_all(b64);
#endif
	return 1;
}

#ifdef USE_POLARSSL
#define MD5_CTX	md5_context
#define MD5_Init(ctx)	md5_starts(ctx)
#define MD5_Update(ctx,data,len)	md5_update(ctx,(unsigned char *)data,len)
#define MD5_Final(dig,ctx)	md5_finish(ctx,dig)
#elif defined(USE_GNUTLS)
typedef struct md5_ctx	MD5_CTX;
#define MD5_Init(ctx)	md5_init(ctx)
#define MD5_Update(ctx,data,len)	md5_update(ctx,len,data)
#define MD5_Final(dig,ctx)	md5_digest(ctx,MD5_DIGEST_LENGTH,dig)
#else
#endif

static const AVal av_authmod_adobe = AVC("authmod=adobe");
static const AVal av_authmod_llnw  = AVC("authmod=llnw");

static void hexenc(unsigned char *inbuf, int len, char *dst)
{
	char *ptr = dst;
	while(len--) {
		sprintf(ptr, "%02x", *inbuf++);
		ptr += 2;
	}
	*ptr = '\0';
}

static char *
	AValChr(AVal *av, char c)
{
	int i;
	for (i = 0; i < av->av_len; i++)
		if (av->av_val[i] == c)
			return &av->av_val[i];
	return NULL;
}

static int
	PublisherAuth(RTMP *r, AVal *description)
{
	char *token_in = NULL;
	char *ptr;
	unsigned char md5sum_val[MD5_DIGEST_LENGTH+1];
	MD5_CTX md5ctx;
	int challenge2_data;
#define RESPONSE_LEN 32
#define CHALLENGE2_LEN 16
#define SALTED2_LEN (32+8+8+8)
#define B64DIGEST_LEN	24	/* 16 byte digest => 22 b64 chars + 2 chars padding */
#define B64INT_LEN	8	/* 4 byte int => 6 b64 chars + 2 chars padding */
#define HEXHASH_LEN	(2*MD5_DIGEST_LENGTH)
	char response[RESPONSE_LEN];
	char challenge2[CHALLENGE2_LEN];
	char salted2[SALTED2_LEN];
	AVal pubToken;

	if (strstr(description->av_val, av_authmod_adobe.av_val) != NULL)
	{
		if(strstr(description->av_val, "code=403 need auth") != NULL)
		{
			if (strstr(r->Link.app.av_val, av_authmod_adobe.av_val) != NULL) {
				RTMP_Log(RTMP_LOGERROR, "%s, wrong pubUser & pubPasswd for publisher auth", __FUNCTION__);
				return 0;
			} else if(r->Link.pubUser.av_len && r->Link.pubPasswd.av_len) {
				pubToken.av_val = malloc(r->Link.pubUser.av_len + av_authmod_adobe.av_len + 8);
				pubToken.av_len = sprintf(pubToken.av_val, "?%s&user=%s",
					av_authmod_adobe.av_val,
					r->Link.pubUser.av_val);
				RTMP_Log(RTMP_LOGDEBUG, "%s, pubToken1: %s", __FUNCTION__, pubToken.av_val);
			} else {
				RTMP_Log(RTMP_LOGERROR, "%s, need to set pubUser & pubPasswd for publisher auth", __FUNCTION__);
				return 0;
			}
		}
		else if((token_in = strstr(description->av_val, "?reason=needauth")) != NULL)
		{
			char *par, *val = NULL, *orig_ptr;
			AVal user, salt, opaque, challenge, *aptr = NULL;
			opaque.av_len = 0;
			challenge.av_len = 0;

			ptr = orig_ptr = strdup(token_in);
			while (ptr)
			{
				par = ptr;
				ptr = strchr(par, '&');
				if(ptr)
					*ptr++ = '\0';

				val =  strchr(par, '=');
				if(val)
					*val++ = '\0';

				if (aptr) {
					aptr->av_len = par - aptr->av_val - 1;
					aptr = NULL;
				}
				if (strcmp(par, "user") == 0){
					user.av_val = val;
					aptr = &user;
				} else if (strcmp(par, "salt") == 0){
					salt.av_val = val;
					aptr = &salt;
				} else if (strcmp(par, "opaque") == 0){
					opaque.av_val = val;
					aptr = &opaque;
				} else if (strcmp(par, "challenge") == 0){
					challenge.av_val = val;
					aptr = &challenge;
				}

				RTMP_Log(RTMP_LOGDEBUG, "%s, par:\"%s\" = val:\"%s\"", __FUNCTION__, par, val);
			}
			if (aptr)
				aptr->av_len = strlen(aptr->av_val);

			/* hash1 = base64enc(md5(user + _aodbeAuthSalt + password)) */
			MD5_Init(&md5ctx);
			MD5_Update(&md5ctx, user.av_val, user.av_len);
			MD5_Update(&md5ctx, salt.av_val, salt.av_len);
			MD5_Update(&md5ctx, r->Link.pubPasswd.av_val, r->Link.pubPasswd.av_len);
			MD5_Final(md5sum_val, &md5ctx);
			RTMP_Log(RTMP_LOGDEBUG, "%s, md5(%s%s%s) =>", __FUNCTION__,
				user.av_val, salt.av_val, r->Link.pubPasswd.av_val);
			RTMP_LogHexString(RTMP_LOGDEBUG, md5sum_val, MD5_DIGEST_LENGTH);

			b64enc(md5sum_val, MD5_DIGEST_LENGTH, salted2, SALTED2_LEN);
			RTMP_Log(RTMP_LOGDEBUG, "%s, b64(md5_1) = %s", __FUNCTION__, salted2);

			challenge2_data = rand();

			b64enc((unsigned char *) &challenge2_data, sizeof(int), challenge2, CHALLENGE2_LEN);
			RTMP_Log(RTMP_LOGDEBUG, "%s, b64(%d) = %s", __FUNCTION__, challenge2_data, challenge2);

			MD5_Init(&md5ctx);
			MD5_Update(&md5ctx, salted2, B64DIGEST_LEN);
			/* response = base64enc(md5(hash1 + opaque + challenge2)) */
			if (opaque.av_len)
				MD5_Update(&md5ctx, opaque.av_val, opaque.av_len);
			else if (challenge.av_len)
				MD5_Update(&md5ctx, challenge.av_val, challenge.av_len);
			MD5_Update(&md5ctx, challenge2, B64INT_LEN);
			MD5_Final(md5sum_val, &md5ctx);

			RTMP_Log(RTMP_LOGDEBUG, "%s, md5(%s%s%s) =>", __FUNCTION__,
				salted2, opaque.av_len ? opaque.av_val : "", challenge2);
			RTMP_LogHexString(RTMP_LOGDEBUG, md5sum_val, MD5_DIGEST_LENGTH);

			b64enc(md5sum_val, MD5_DIGEST_LENGTH, response, RESPONSE_LEN);
			RTMP_Log(RTMP_LOGDEBUG, "%s, b64(md5_2) = %s", __FUNCTION__, response);

			/* have all hashes, create auth token for the end of app */
			pubToken.av_val = malloc(32 + B64INT_LEN + B64DIGEST_LEN + opaque.av_len);
			pubToken.av_len = sprintf(pubToken.av_val,
				"&challenge=%s&response=%s&opaque=%s",
				challenge2,
				response,
				opaque.av_len ? opaque.av_val : "");
			RTMP_Log(RTMP_LOGDEBUG, "%s, pubToken2: %s", __FUNCTION__, pubToken.av_val);
			free(orig_ptr);
		}
		else if(strstr(description->av_val, "?reason=authfailed") != NULL)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed: wrong password", __FUNCTION__);
			return 0;
		}
		else if(strstr(description->av_val, "?reason=nosuchuser") != NULL)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed: no such user", __FUNCTION__);
			return 0;
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed: unknown auth mode: %s",
				__FUNCTION__, description->av_val);
			return 0;
		}

		ptr = malloc(r->Link.app.av_len + pubToken.av_len);
		strncpy(ptr, r->Link.app.av_val, r->Link.app.av_len);
		strncpy(ptr + r->Link.app.av_len, pubToken.av_val, pubToken.av_len);
		r->Link.app.av_len += pubToken.av_len;
		if(r->Link.lFlags & RTMP_LF_FAPU)
			free(r->Link.app.av_val);
		r->Link.app.av_val = ptr;

		ptr = malloc(r->Link.tcUrl.av_len + pubToken.av_len);
		strncpy(ptr, r->Link.tcUrl.av_val, r->Link.tcUrl.av_len);
		strncpy(ptr + r->Link.tcUrl.av_len, pubToken.av_val, pubToken.av_len);
		r->Link.tcUrl.av_len += pubToken.av_len;
		if(r->Link.lFlags & RTMP_LF_FTCU)
			free(r->Link.tcUrl.av_val);
		r->Link.tcUrl.av_val = ptr;

		free(pubToken.av_val);
		r->Link.lFlags |= RTMP_LF_FTCU | RTMP_LF_FAPU;

		RTMP_Log(RTMP_LOGDEBUG, "%s, new app: %.*s tcUrl: %.*s playpath: %s", __FUNCTION__,
			r->Link.app.av_len, r->Link.app.av_val,
			r->Link.tcUrl.av_len, r->Link.tcUrl.av_val,
			r->Link.playpath.av_val);
	}
	else if (strstr(description->av_val, av_authmod_llnw.av_val) != NULL)
	{
		if(strstr(description->av_val, "code=403 need auth") != NULL)
		{
			/* This part seems to be the same for llnw and adobe */

			if (strstr(r->Link.app.av_val, av_authmod_llnw.av_val) != NULL) {
				RTMP_Log(RTMP_LOGERROR, "%s, wrong pubUser & pubPasswd for publisher auth", __FUNCTION__);
				return 0;
			} else if(r->Link.pubUser.av_len && r->Link.pubPasswd.av_len) {
				pubToken.av_val = malloc(r->Link.pubUser.av_len + av_authmod_llnw.av_len + 8);
				pubToken.av_len = sprintf(pubToken.av_val, "?%s&user=%s",
					av_authmod_llnw.av_val,
					r->Link.pubUser.av_val);
				RTMP_Log(RTMP_LOGDEBUG, "%s, pubToken1: %s", __FUNCTION__, pubToken.av_val);
			} else {
				RTMP_Log(RTMP_LOGERROR, "%s, need to set pubUser & pubPasswd for publisher auth", __FUNCTION__);
				return 0;
			}
		}
		else if((token_in = strstr(description->av_val, "?reason=needauth")) != NULL)
		{
			char *orig_ptr;
			char *par, *val = NULL;
			char hash1[HEXHASH_LEN+1], hash2[HEXHASH_LEN+1], hash3[HEXHASH_LEN+1];
			AVal user, nonce, *aptr = NULL;
			AVal apptmp;

			/* llnw auth method
			* Seems to be closely based on HTTP Digest Auth:
			*    http://tools.ietf.org/html/rfc2617
			*    http://en.wikipedia.org/wiki/Digest_access_authentication
			*/

			const char authmod[] = "llnw";
			const char realm[] = "live";
			const char method[] = "publish";
			const char qop[] = "auth";
			/* nc = 1..connection count (or rather, number of times cnonce has been reused) */
			int nc = 1;
			/* nchex = hexenc(nc) (8 hex digits according to RFC 2617) */
			char nchex[9];
			/* cnonce = hexenc(4 random bytes) (initialized on first connection) */
			char cnonce[9];

			ptr = orig_ptr = strdup(token_in);
			/* Extract parameters (we need user and nonce) */
			while (ptr)
			{
				par = ptr;
				ptr = strchr(par, '&');
				if(ptr)
					*ptr++ = '\0';

				val =  strchr(par, '=');
				if(val)
					*val++ = '\0';

				if (aptr) {
					aptr->av_len = par - aptr->av_val - 1;
					aptr = NULL;
				}
				if (strcmp(par, "user") == 0){
					user.av_val = val;
					aptr = &user;
				} else if (strcmp(par, "nonce") == 0){
					nonce.av_val = val;
					aptr = &nonce;
				}

				RTMP_Log(RTMP_LOGDEBUG, "%s, par:\"%s\" = val:\"%s\"", __FUNCTION__, par, val);
			}
			if (aptr)
				aptr->av_len = strlen(aptr->av_val);

			/* FIXME: handle case where user==NULL or nonce==NULL */

			sprintf(nchex, "%08x", nc);
			sprintf(cnonce, "%08x", rand());

			/* hash1 = hexenc(md5(user + ":" + realm + ":" + password)) */
			MD5_Init(&md5ctx);
			MD5_Update(&md5ctx, user.av_val, user.av_len);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, realm, sizeof(realm)-1);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, r->Link.pubPasswd.av_val, r->Link.pubPasswd.av_len);
			MD5_Final(md5sum_val, &md5ctx);
			RTMP_Log(RTMP_LOGDEBUG, "%s, md5(%s:%s:%s) =>", __FUNCTION__,
				user.av_val, realm, r->Link.pubPasswd.av_val);
			RTMP_LogHexString(RTMP_LOGDEBUG, md5sum_val, MD5_DIGEST_LENGTH);
			hexenc(md5sum_val, MD5_DIGEST_LENGTH, hash1);

			/* hash2 = hexenc(md5(method + ":/" + app + "/" + appInstance)) */
			/* Extract appname + appinstance without query parameters */
			apptmp = r->Link.app;
			ptr = AValChr(&apptmp, '?');
			if (ptr)
				apptmp.av_len = ptr - apptmp.av_val;

			MD5_Init(&md5ctx);
			MD5_Update(&md5ctx, method, sizeof(method)-1);
			MD5_Update(&md5ctx, ":/", 2);
			MD5_Update(&md5ctx, apptmp.av_val, apptmp.av_len);
			if (!AValChr(&apptmp, '/'))
				MD5_Update(&md5ctx, "/_definst_", sizeof("/_definst_") - 1);
			MD5_Final(md5sum_val, &md5ctx);
			RTMP_Log(RTMP_LOGDEBUG, "%s, md5(%s:/%.*s) =>", __FUNCTION__,
				method, apptmp.av_len, apptmp.av_val);
			RTMP_LogHexString(RTMP_LOGDEBUG, md5sum_val, MD5_DIGEST_LENGTH);
			hexenc(md5sum_val, MD5_DIGEST_LENGTH, hash2);

			/* hash3 = hexenc(md5(hash1 + ":" + nonce + ":" + nchex + ":" + cnonce + ":" + qop + ":" + hash2)) */
			MD5_Init(&md5ctx);
			MD5_Update(&md5ctx, hash1, HEXHASH_LEN);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, nonce.av_val, nonce.av_len);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, nchex, sizeof(nchex)-1);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, cnonce, sizeof(cnonce)-1);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, qop, sizeof(qop)-1);
			MD5_Update(&md5ctx, ":", 1);
			MD5_Update(&md5ctx, hash2, HEXHASH_LEN);
			MD5_Final(md5sum_val, &md5ctx);
			RTMP_Log(RTMP_LOGDEBUG, "%s, md5(%s:%s:%s:%s:%s:%s) =>", __FUNCTION__,
				hash1, nonce.av_val, nchex, cnonce, qop, hash2);
			RTMP_LogHexString(RTMP_LOGDEBUG, md5sum_val, MD5_DIGEST_LENGTH);
			hexenc(md5sum_val, MD5_DIGEST_LENGTH, hash3);

			/* pubToken = &authmod=<authmod>&user=<username>&nonce=<nonce>&cnonce=<cnonce>&nc=<nchex>&response=<hash3> */
			/* Append nonces and response to query string which already contains
			* user + authmod */
			pubToken.av_val = malloc(64 + sizeof(authmod)-1 + user.av_len + nonce.av_len + sizeof(cnonce)-1 + sizeof(nchex)-1 + HEXHASH_LEN);
			sprintf(pubToken.av_val,
				"&nonce=%s&cnonce=%s&nc=%s&response=%s",
				nonce.av_val, cnonce, nchex, hash3);
			pubToken.av_len = strlen(pubToken.av_val);
			RTMP_Log(RTMP_LOGDEBUG, "%s, pubToken2: %s", __FUNCTION__, pubToken.av_val);

			free(orig_ptr);
		}
		else if(strstr(description->av_val, "?reason=authfail") != NULL)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed", __FUNCTION__);
			return 0;
		}
		else if(strstr(description->av_val, "?reason=nosuchuser") != NULL)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed: no such user", __FUNCTION__);
			return 0;
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR, "%s, Authentication failed: unknown auth mode: %s",
				__FUNCTION__, description->av_val);
			return 0;
		}

		ptr = malloc(r->Link.app.av_len + pubToken.av_len);
		strncpy(ptr, r->Link.app.av_val, r->Link.app.av_len);
		strncpy(ptr + r->Link.app.av_len, pubToken.av_val, pubToken.av_len);
		r->Link.app.av_len += pubToken.av_len;
		if(r->Link.lFlags & RTMP_LF_FAPU)
			free(r->Link.app.av_val);
		r->Link.app.av_val = ptr;

		ptr = malloc(r->Link.tcUrl.av_len + pubToken.av_len);
		strncpy(ptr, r->Link.tcUrl.av_val, r->Link.tcUrl.av_len);
		strncpy(ptr + r->Link.tcUrl.av_len, pubToken.av_val, pubToken.av_len);
		r->Link.tcUrl.av_len += pubToken.av_len;
		if(r->Link.lFlags & RTMP_LF_FTCU)
			free(r->Link.tcUrl.av_val);
		r->Link.tcUrl.av_val = ptr;

		free(pubToken.av_val);
		r->Link.lFlags |= RTMP_LF_FTCU | RTMP_LF_FAPU;

		RTMP_Log(RTMP_LOGDEBUG, "%s, new app: %.*s tcUrl: %.*s playpath: %s", __FUNCTION__,
			r->Link.app.av_len, r->Link.app.av_val,
			r->Link.tcUrl.av_len, r->Link.tcUrl.av_val,
			r->Link.playpath.av_val);
	}
	else
	{
		return 0;
	}
	return 1;
}
#endif


SAVC(onBWDone);
SAVC(onFCSubscribe);
SAVC(onFCUnsubscribe);
SAVC(_onbwcheck);
SAVC(_onbwdone);
SAVC(_error);
SAVC(close);
SAVC(code);
SAVC(level);
SAVC(description);
SAVC(onStatus);
SAVC(playlist_ready);
static const AVal av_NetStream_Failed = AVC("NetStream.Failed");
static const AVal av_NetStream_Play_Failed = AVC("NetStream.Play.Failed");
static const AVal av_NetStream_Play_StreamNotFound =
	AVC("NetStream.Play.StreamNotFound");
static const AVal av_NetConnection_Connect_InvalidApp =
	AVC("NetConnection.Connect.InvalidApp");
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_NetStream_Play_Complete = AVC("NetStream.Play.Complete");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_NetStream_Seek_Notify = AVC("NetStream.Seek.Notify");
static const AVal av_NetStream_Pause_Notify = AVC("NetStream.Pause.Notify");
static const AVal av_NetStream_Play_PublishNotify =
	AVC("NetStream.Play.PublishNotify");
static const AVal av_NetStream_Play_UnpublishNotify =
	AVC("NetStream.Play.UnpublishNotify");
static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_NetConnection_Connect_Rejected =
	AVC("NetConnection.Connect.Rejected");

/* Returns 0 for OK/Failed/error, 1 for 'Stop or Complete' */
static int
	HandleInvoke(RTMP *r, const char *body, unsigned int nBodySize)
{
	AMFObject obj;
	AVal method;
	double txn;
	int ret = 0, nRes;
	if (body[0] != 0x02)		/* make sure it is a string method name we start with */
	{
		RTMP_Log(RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
			__FUNCTION__);
		return 0;
	}

	nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
		return 0;
	}

	AMF_Dump(&obj);
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
	txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
	RTMP_Log(RTMP_LOGDEBUG, "%s, server invoking <%s>", __FUNCTION__, method.av_val);

	if (AVMATCH(&method, &av__result))
	{
		AVal methodInvoked = {0};
		int i;

		for (i=0; i<r->m_numCalls; i++) {
			if (r->m_methodCalls[i].num == (int)txn) {
				methodInvoked = r->m_methodCalls[i].name;
				AV_erase(r->m_methodCalls, &r->m_numCalls, i, FALSE);
				break;
			}
		}
		if (!methodInvoked.av_val) {
			RTMP_Log(RTMP_LOGDEBUG, "%s, received result id %f without matching request",
				__FUNCTION__, txn);
			goto leave;
		}

		RTMP_Log(RTMP_LOGDEBUG, "%s, received result for method call <%s>", __FUNCTION__,
			methodInvoked.av_val);

		if (AVMATCH(&methodInvoked, &av_connect))
		{
			if (r->Link.token.av_len)
			{
				AMFObjectProperty p;
				if (RTMP_FindFirstMatchingProperty(&obj, &av_secureToken, &p))
				{
					DecodeTEA(&r->Link.token, &p.p_vu.p_aval);
					SendSecureTokenResponse(r, &p.p_vu.p_aval);
				}
			}
			if (r->Link.protocol & RTMP_FEATURE_WRITE)
			{
				SendReleaseStream(r);
				SendFCPublish(r);
			}
			else
			{
				RTMP_SendServerBW(r);
				RTMP_SendCtrl(r, 3, 0, 300);
			}


			//////////////////////  5   /////////////////////////
			
			RTMP_SendCreateStream(r);

			if (!(r->Link.protocol & RTMP_FEATURE_WRITE))
			{
				/* Authenticate on Justin.tv legacy servers before sending FCSubscribe */
				if (r->Link.usherToken.av_len)
					SendUsherToken(r, &r->Link.usherToken);
				/* Send the FCSubscribe if live stream or if subscribepath is set */
				if (r->Link.subscribepath.av_len)
					SendFCSubscribe(r, &r->Link.subscribepath);
				else if (r->Link.lFlags & RTMP_LF_LIVE)
					SendFCSubscribe(r, &r->Link.playpath);
			}
		}
		else if (AVMATCH(&methodInvoked, &av_createStream))
		{
			r->m_stream_id = (int)AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));

			if (r->Link.protocol & RTMP_FEATURE_WRITE)
			{

				//////////////////////  5   /////////////////////////
				
				SendPublish(r);
			}
			else
			{
				if (r->Link.lFlags & RTMP_LF_PLST)
					SendPlaylist(r);

				//////////////////////  5   /////////////////////////
				
				SendPlay(r);
				RTMP_SendCtrl(r, 3, r->m_stream_id, r->m_nBufferMS);
			}
		}
		else if (AVMATCH(&methodInvoked, &av_play) ||
			AVMATCH(&methodInvoked, &av_publish))
		{
			r->m_bPlaying = TRUE;
		}
		free(methodInvoked.av_val);
	}
	else if (AVMATCH(&method, &av_onBWDone))
	{
		if (!r->m_nBWCheckCounter)
			SendCheckBW(r);
	}
	else if (AVMATCH(&method, &av_onFCSubscribe))
	{
		/* SendOnFCSubscribe(); */
	}
	else if (AVMATCH(&method, &av_onFCUnsubscribe))
	{
		RTMP_Close(r);
		ret = 1;
	}
	else if (AVMATCH(&method, &av_ping))
	{
		SendPong(r, txn);
	}
	else if (AVMATCH(&method, &av__onbwcheck))
	{
		SendCheckBWResult(r, txn);
	}
	else if (AVMATCH(&method, &av__onbwdone))
	{
		int i;
		for (i = 0; i < r->m_numCalls; i++)
			if (AVMATCH(&r->m_methodCalls[i].name, &av__checkbw))
			{
				AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
				break;
			}
	}
	else if (AVMATCH(&method, &av__error))
	{
#ifdef CRYPTO
		AVal methodInvoked = {0};
		int i;

		if (r->Link.protocol & RTMP_FEATURE_WRITE)
		{
			for (i=0; i<r->m_numCalls; i++)
			{
				if (r->m_methodCalls[i].num == txn)
				{
					methodInvoked = r->m_methodCalls[i].name;
					AV_erase(r->m_methodCalls, &r->m_numCalls, i, FALSE);
					break;
				}
			}
			if (!methodInvoked.av_val)
			{
				RTMP_Log(RTMP_LOGDEBUG, "%s, received result id %f without matching request",
					__FUNCTION__, txn);
				goto leave;
			}

			RTMP_Log(RTMP_LOGDEBUG, "%s, received error for method call <%s>", __FUNCTION__,
				methodInvoked.av_val);

			if (AVMATCH(&methodInvoked, &av_connect))
			{
				AMFObject obj2;
				AVal code, level, description;
				AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &obj2);
				AMFProp_GetString(AMF_GetProp(&obj2, &av_code, -1), &code);
				AMFProp_GetString(AMF_GetProp(&obj2, &av_level, -1), &level);
				AMFProp_GetString(AMF_GetProp(&obj2, &av_description, -1), &description);
				RTMP_Log(RTMP_LOGDEBUG, "%s, error description: %s", __FUNCTION__, description.av_val);
				/* if PublisherAuth returns 1, then reconnect */
				if (PublisherAuth(r, &description) == 1)
				{
					CloseInternal(r, 1);
					if (!RTMP_Connect(r, NULL) || !RTMP_ConnectStream(r, 0))
						goto leave;
				}
			}
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR, "rtmp server sent error");
		}
		free(methodInvoked.av_val);
#else
		RTMP_Log(RTMP_LOGERROR, "rtmp server sent error");
#endif
	}
	else if (AVMATCH(&method, &av_close))
	{
		RTMP_Log(RTMP_LOGERROR, "rtmp server requested close");
		RTMP_Close(r);
	}
	else if (AVMATCH(&method, &av_onStatus))
	{
		AMFObject obj2;
		AVal code, level;
		AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &obj2);
		AMFProp_GetString(AMF_GetProp(&obj2, &av_code, -1), &code);
		AMFProp_GetString(AMF_GetProp(&obj2, &av_level, -1), &level);

		RTMP_Log(RTMP_LOGDEBUG, "%s, onStatus: %s", __FUNCTION__, code.av_val);
		if (AVMATCH(&code, &av_NetStream_Failed)
			|| AVMATCH(&code, &av_NetStream_Play_Failed)
			|| AVMATCH(&code, &av_NetStream_Play_StreamNotFound)
			|| AVMATCH(&code, &av_NetConnection_Connect_InvalidApp))
		{
			r->m_stream_id = -1;
			RTMP_Close(r);
			RTMP_Log(RTMP_LOGERROR, "Closing connection: %s", code.av_val);
		}

		else if (AVMATCH(&code, &av_NetStream_Play_Start)
			|| AVMATCH(&code, &av_NetStream_Play_PublishNotify))
		{
			int i;
			r->m_bPlaying = TRUE;
			for (i = 0; i < r->m_numCalls; i++)
			{
				if (AVMATCH(&r->m_methodCalls[i].name, &av_play))
				{
					AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
					break;
				}
			}
		}

		else if (AVMATCH(&code, &av_NetStream_Publish_Start))
		{
			int i;
			r->m_bPlaying = TRUE;
			for (i = 0; i < r->m_numCalls; i++)
			{
				if (AVMATCH(&r->m_methodCalls[i].name, &av_publish))
				{
					AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
					break;
				}
			}
		}

		/* Return 1 if this is a Play.Complete or Play.Stop */
		else if (AVMATCH(&code, &av_NetStream_Play_Complete)
			|| AVMATCH(&code, &av_NetStream_Play_Stop)
			|| AVMATCH(&code, &av_NetStream_Play_UnpublishNotify))
		{
			RTMP_Close(r);
			ret = 1;
		}

		else if (AVMATCH(&code, &av_NetStream_Seek_Notify))
		{
			r->m_read.flags &= ~RTMP_READ_SEEKING;
		}

		else if (AVMATCH(&code, &av_NetStream_Pause_Notify))
		{
			if (r->m_pausing == 1 || r->m_pausing == 2)
			{
				RTMP_SendPause(r, FALSE, r->m_pauseStamp);
				r->m_pausing = 3;
			}
		}
	}
	else if (AVMATCH(&method, &av_playlist_ready))
	{
		int i;
		for (i = 0; i < r->m_numCalls; i++)
		{
			if (AVMATCH(&r->m_methodCalls[i].name, &av_set_playlist))
			{
				AV_erase(r->m_methodCalls, &r->m_numCalls, i, TRUE);
				break;
			}
		}
	}
	else
	{

	}
leave:
	AMF_Reset(&obj);
	return ret;
}

int
	RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal *name,
	AMFObjectProperty * p)
{
	int n;
	/* this is a small object search to locate the "duration" property */
	for (n = 0; n < obj->o_num; n++)
	{
		AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);

		if (AVMATCH(&prop->p_name, name))
		{
			memcpy(p, prop, sizeof(*prop));
			return TRUE;
		}

		if (prop->p_type == AMF_OBJECT || prop->p_type == AMF_ECMA_ARRAY)
		{
			if (RTMP_FindFirstMatchingProperty(&prop->p_vu.p_object, name, p))
				return TRUE;
		}
	}
	return FALSE;
}

/* Like above, but only check if name is a prefix of property */
int
	RTMP_FindPrefixProperty(AMFObject *obj, const AVal *name,
	AMFObjectProperty * p)
{
	int n;
	for (n = 0; n < obj->o_num; n++)
	{
		AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);

		if (prop->p_name.av_len > name->av_len &&
			!memcmp(prop->p_name.av_val, name->av_val, name->av_len))
		{
			memcpy(p, prop, sizeof(*prop));
			return TRUE;
		}

		if (prop->p_type == AMF_OBJECT)
		{
			if (RTMP_FindPrefixProperty(&prop->p_vu.p_object, name, p))
				return TRUE;
		}
	}
	return FALSE;
}

static int
	DumpMetaData(AMFObject *obj)
{
	AMFObjectProperty *prop;
	int n, len;
	for (n = 0; n < obj->o_num; n++)
	{
		char str[256] = "";
		prop = AMF_GetProp(obj, NULL, n);
		switch (prop->p_type)
		{
		case AMF_OBJECT:
		case AMF_ECMA_ARRAY:
		case AMF_STRICT_ARRAY:
			if (prop->p_name.av_len)
				RTMP_Log(RTMP_LOGINFO, "%.*s:", prop->p_name.av_len, prop->p_name.av_val);
			DumpMetaData(&prop->p_vu.p_object);
			break;
		case AMF_NUMBER:
			snprintf(str, 255, "%.2f", prop->p_vu.p_number);
			break;
		case AMF_BOOLEAN:
			snprintf(str, 255, "%s",
				prop->p_vu.p_number != 0. ? "TRUE" : "FALSE");
			break;
		case AMF_STRING:
			len = snprintf(str, 255, "%.*s", prop->p_vu.p_aval.av_len,
				prop->p_vu.p_aval.av_val);
			if (len >= 1 && str[len-1] == '\n')
				str[len-1] = '\0';
			break;
		case AMF_DATE:
			snprintf(str, 255, "timestamp:%.2f", prop->p_vu.p_number);
			break;
		default:
			snprintf(str, 255, "INVALID TYPE 0x%02x",
				(unsigned char)prop->p_type);
		}
		if (str[0] && prop->p_name.av_len)
		{
			RTMP_Log(RTMP_LOGINFO, "  %-22.*s%s", prop->p_name.av_len,
				prop->p_name.av_val, str);
		}
	}
	return FALSE;
}

SAVC(onMetaData);
SAVC(duration);
SAVC(video);
SAVC(audio);

static int
	HandleMetadata(RTMP *r, char *body, unsigned int len)
{
	/* allright we get some info here, so parse it and print it */
	/* also keep duration or filesize to make a nice progress bar */

	AMFObject obj;
	AVal metastring;
	int ret = FALSE;

	int nRes = AMF_Decode(&obj, body, len, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding meta data packet", __FUNCTION__);
		return FALSE;
	}

	AMF_Dump(&obj);
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &metastring);

	if (AVMATCH(&metastring, &av_onMetaData))
	{
		AMFObjectProperty prop;
		/* Show metadata */
		RTMP_Log(RTMP_LOGINFO, "Metadata:");
		DumpMetaData(&obj);
		if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop))
		{
			r->m_fDuration = prop.p_vu.p_number;
			/*RTMP_Log(RTMP_LOGDEBUG, "Set duration: %.2f", m_fDuration); */
		}
		/* Search for audio or video tags */
		if (RTMP_FindPrefixProperty(&obj, &av_video, &prop))
			r->m_read.dataType |= 1;
		if (RTMP_FindPrefixProperty(&obj, &av_audio, &prop))
			r->m_read.dataType |= 4;
		ret = TRUE;
	}
	AMF_Reset(&obj);
	return ret;
}

static void
	HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet)
{
	if (packet->m_nBodySize >= 4)
	{
		r->m_inChunkSize = AMF_DecodeInt32(packet->m_body);
		RTMP_Log(RTMP_LOGDEBUG, "%s, received: chunk size change to %d", __FUNCTION__,
			r->m_inChunkSize);
	}
}

static void
	HandleAudio(RTMP *r, const RTMPPacket *packet)
{
}

static void
	HandleVideo(RTMP *r, const RTMPPacket *packet)
{
}

static void
	HandleCtrl(RTMP *r, const RTMPPacket *packet)
{
	short nType = -1;
	unsigned int tmp;
	if (packet->m_body && packet->m_nBodySize >= 2)
		nType = AMF_DecodeInt16(packet->m_body);
	
	RTMP_Log(RTMP_LOGDEBUG, "%s, received ctrl. type: %d, len: %d", __FUNCTION__, nType,
		packet->m_nBodySize);
	/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

	if (packet->m_nBodySize >= 6)
	{
		switch (nType)
		{
		case 0:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Begin %d", __FUNCTION__, tmp);
			break;

		case 1:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream EOF %d", __FUNCTION__, tmp);
			if (r->m_pausing == 1)
				r->m_pausing = 2;
			break;

		case 2:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream Dry %d", __FUNCTION__, tmp);
			break;

		case 4:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream IsRecorded %d", __FUNCTION__, tmp);
			break;

		case 6:		/* server ping. reply with pong. */
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Ping %d", __FUNCTION__, tmp);
			RTMP_SendCtrl(r, 0x07, tmp, 0);
			break;

			/* FMS 3.5 servers send the following two controls to let the client
			* know when the server has sent a complete buffer. I.e., when the
			* server has sent an amount of data equal to m_nBufferMS in duration.
			* The server meters its output so that data arrives at the client
			* in realtime and no faster.
			*
			* The rtmpdump program tries to set m_nBufferMS as large as
			* possible, to force the server to send data as fast as possible.
			* In practice, the server appears to cap this at about 1 hour's
			* worth of data. After the server has sent a complete buffer, and
			* sends this BufferEmpty message, it will wait until the play
			* duration of that buffer has passed before sending a new buffer.
			* The BufferReady message will be sent when the new buffer starts.
			* (There is no BufferReady message for the very first buffer;
			* presumably the Stream Begin message is sufficient for that
			* purpose.)
			*
			* If the network speed is much faster than the data bitrate, then
			* there may be long delays between the end of one buffer and the
			* start of the next.
			*
			* Since usually the network allows data to be sent at
			* faster than realtime, and rtmpdump wants to download the data
			* as fast as possible, we use this RTMP_LF_BUFX hack: when we
			* get the BufferEmpty message, we send a Pause followed by an
			* Unpause. This causes the server to send the next buffer immediately
			* instead of waiting for the full duration to elapse. (That's
			* also the purpose of the ToggleStream function, which rtmpdump
			* calls if we get a read timeout.)
			*
			* Media player apps don't need this hack since they are just
			* going to play the data in realtime anyway. It also doesn't work
			* for live streams since they obviously can only be sent in
			* realtime. And it's all moot if the network speed is actually
			* slower than the media bitrate.
			*/
		case 31:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
			if (!(r->Link.lFlags & RTMP_LF_BUFX))
				break;
			if (!r->m_pausing)
			{
				r->m_pauseStamp = r->m_mediaChannel < r->m_channelsAllocatedIn ?
					r->m_channelTimestamp[r->m_mediaChannel] : 0;
				RTMP_SendPause(r, TRUE, r->m_pauseStamp);
				r->m_pausing = 1;
			}
			else if (r->m_pausing == 2)
			{
				RTMP_SendPause(r, FALSE, r->m_pauseStamp);
				r->m_pausing = 3;
			}
			break;

		case 32:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
			break;

		default:
			tmp = AMF_DecodeInt32(packet->m_body + 2);
			RTMP_Log(RTMP_LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
			break;
		}

	}

	if (nType == 0x1A)
	{
		RTMP_Log(RTMP_LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
		if (packet->m_nBodySize > 2 && packet->m_body[2] > 0x01)
		{
			RTMP_Log(RTMP_LOGERROR,
				"%s: SWFVerification Type %d request not supported! Patches welcome...",
				__FUNCTION__, packet->m_body[2]);
		}
#ifdef CRYPTO
		/*RTMP_LogHex(packet.m_body, packet.m_nBodySize); */

		/* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
		else if (r->Link.SWFSize)
		{
			RTMP_SendCtrl(r, 0x1B, 0, 0);
		}
		else
		{
			RTMP_Log(RTMP_LOGERROR,
				"%s: Ignoring SWFVerification request, use --swfVfy!",
				__FUNCTION__);
		}
#else
		RTMP_Log(RTMP_LOGERROR,
			"%s: Ignoring SWFVerification request, no CRYPTO support!",
			__FUNCTION__);
#endif
	}
}

static void
	HandleServerBW(RTMP *r, const RTMPPacket *packet)
{
	r->m_nServerBW = AMF_DecodeInt32(packet->m_body);
	RTMP_Log(RTMP_LOGDEBUG, "%s: server BW = %d", __FUNCTION__, r->m_nServerBW);
}

static void
	HandleClientBW(RTMP *r, const RTMPPacket *packet)
{
	r->m_nClientBW = AMF_DecodeInt32(packet->m_body);
	if (packet->m_nBodySize > 4)
		r->m_nClientBW2 = packet->m_body[4];
	else
		r->m_nClientBW2 = -1;
	RTMP_Log(RTMP_LOGDEBUG, "%s: client BW = %d %d", __FUNCTION__, r->m_nClientBW,
		r->m_nClientBW2);
}

static int
	DecodeInt32LE(const char *data)
{
	unsigned char *c = (unsigned char *)data;
	unsigned int val;

	val = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
	return val;
}

static int
	EncodeInt32LE(char *output, int nVal)
{
	output[0] = nVal;
	nVal >>= 8;
	output[1] = nVal;
	nVal >>= 8;
	output[2] = nVal;
	nVal >>= 8;
	output[3] = nVal;
	return 4;
}

/*
    MpegTS PCR

    一、 标准规定在原始音频和视频流中,PTS的间隔不能超过0.7s，出现在TS包头的PCR间隔不能超过0.1s，PCR最大值为26:30:43
    二、 在TS的传输过程中，一般 DTS 和 PCR 差值会在一个合适的范围，这个差值就是要设置的视音频 Buffer 的大小
        1) 视频 DTS 和 PCR 的差值在 700ms -- 1200ms 之间
        2) 音频 DTS 和 PCR 的差值在 200ms -- 700ms 之间

    三、 PCR分两部分编码：一个以系统时钟频率的 1/300 为单位，称为PCR_base，共33bit；另一个以系统时钟频率为单位，称为PCR_ext，共9bit，共42bit。
        具体规定如下:
        PCR_base(i) = ((系统时钟频率 x t(i)) div 300) % 2^33
        PCR_ext(i) = ((系统时钟频率 x t(i)) div 1) % 300
        PCR(i) = PCR_base(i) x 300 + PCR_ext(i)

        例如:
            时间"03:02:29.012"的PCR计算如下:
            03:02:29.012 = ((3 * 60) + 2) * 60 + 29.012 = 10949.012s

            PCR_base = ((27 000 000 * 10949.012) / 300) % 2^33 = 98 541 080
            PCR_ext  = ((27 000 000 * 10949.012) / 1  ) % 300  = 0 
            PCR = 98 541 080 * 300 + 0 = 295 623 324 000

        逆推一个:

            假设PCR= 1209740011800
            PCRbase = 1209740011800 / 300 = 4032466706
            PCR-ext = 1209740011800 % 300 = 0

            t = PCRbase*300/27000000 = 44805.185622222222222222222222222
            time:  t/3600: ( t-(t/3600))/60: t-( t-(t/3600))/60 = 12:26:45.185622222222222222222222222

    四、 PCR-base的作用:
        1) 与PTS和DTS作比较, 当二者相同时, 相应的单元被显示或者解码
        2) 在解码器切换节目时,提供对解码器 PCR 计数器的初始值, 以让该 PCR 值与 PTS、DTS 最大可能地达到相同的时间起点

    五、 PCR-ext的作用:
        1) 通过解码器端的锁相环路修正解码器的系统时钟, 使其达到和编码器一致的 27MHz.
*/

//#if 0
/*
一、  Chunk Format（块格式）
	|--------------|----------------|--------------------|------------|
	| Basic Header | Message Header | Extended Timestamp | Chunk Data |
	|--------------|----------------|--------------------|------------|
	|<---------------- Chunk Header -------------------->|

	1、 Basic Header(基本的头信息): 
		 0  1  2  3  4  5  6  7
		|--+--+--+--+--+--+--+--+
		| Fmt |  	cs id 	|									csid = [3, 63]
		|-----|-----------------|

		 0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7
		|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--|
		| Fmt |  	   0        |	    CSID		|					csid = [64, 319]
		|-----|-----------------|-----------------------|

		 0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7
		|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--+
		| Fmt |        1        |			    CSID				|	csid = [64, 65599]
		|-----|-----------------|-----------------------------------------------|

		1) CSID ()---- chunk stream ID（流通道Id），用来唯一标识一个特定的流通道,  Basic Header的长度取决于 CSID 的大小
			一般  Basic Header 占用1个字节, 
				CSID占6位，6位最多可以表示64个数，因此这种情况下CSID在［0，63］之间，其中用户可自定义的范围为［3，63］。 
				
			特殊  CSID，0，1，2由协议保留表示特殊信息:
		
			0代表 Basic Header 占用2个字节,
				第一个字节的 CSID 全0, 第二个字节为 CSID, 8位 可以表示 [0，255], + 64 = [64, 319]
				
			1代表 Basic Header 占用3个字节,
				第一个字节的 CSID = 1, 第二三个字节为 CSID, 16位可以表示 [0，65535], + 64 = [64, 65599], 小端存取, CSID[2] << 8 | CSID[1]
				
			2代表 该chunk是控制信息和一些命令信息，后面会有详细的介绍。 
			
		2) fmt  (2bit)---- chunk type（chunk的类型），chunk type决定了后面Message Header的格式

			fmt == 0, (11 byte) Message_Header = Time(3byte) + MessageLen(3byte) + MessageTypeID(1byte) + Message_Stream_ID(4byte), timestamp == 绝对值
			
			fmt == 1, (7 byte) Message_Header = Time(3byte) + MessageLen(3byte) + MessageTypeID(1byt), timestamp == 时间差值 
			
				Message Header占用7个字节，省去了表示msg stream id的4个字节，表示此chunk和上一次发的chunk所在的流相同，
				如果在发送端只和对端有一个流链接的时候可以尽量去采取这种格式。 
				timestamp delta：占用3个字节，
				注意这里和 fmt == 0 时不同，存储的是和上一个 chunk 的时间差。类似上面提到的timestamp，
				当它的值超过3个字节所能表示的最大值时，三个字节都置为1，实际的时间戳差值就会转存到Extended Timestamp字段中，
				接受端在判断timestamp delta字段24个位都为1时就会去Extended timestamp中解析时机的与上次时间戳的差值。 

			fmt == 2, (3 byte) Message_Header = Time(3byte), timestamp == 时间差值 
			
				Message Header占用3个字节，相对于type＝1格式又省去了表示消息长度的3个字节和表示消息类型的1个字节，
				表示此chunk和上一次发送的chunk所在的流、消息的长度和消息的类型都相同。余下的这三个字节表示timestamp delta，使用同type＝1 

			fmt == 3, (0 byte) , timestamp == 时间差值

				它表示这个chunk的Message Header和上一个是完全相同的，自然就不用再传输一遍了。
				当它跟在 fmt == 0 的chunk后面时，表示和前一个chunk的时间戳都是相同的。
				什么时候连时间戳都相同呢？就是一个Message拆分成了多个chunk，这个chunk和上一个chunk同属于一个Message。
				当它跟在 fmt == 1 或者 fmt == 2 的chunk后面时，表示和前一个chunk的时间戳的差是相同的。
				比如第一个chunk的Type＝0，timestamp＝100，第二个chunk的Type＝2，timestamp delta＝20，表示时间戳为100+20=120，
				第三个chunk的Type＝3，表示timestamp delta＝20，时间戳为120+20=140 

	2、 Message Header（消息的头信息)
	
		|0  1  2  3  4  5  6  7 | 0  1  2  3  4  5  6  7| 0  1  2  3  4  5  6  7| 0  1  2  3  4  5  6  7|
		|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--|--+--+--+--+--+--+--+--|
		|					timestamp(3byte)					|     message length	>
		|-----------------------------------------------------------------------|=======================|
		>       		message length(3byte)		| messagetypeid (1byte) |   message stream id	>
		|===============================================|-----------------------|=======================|
		>                           message stream id(4byte)	(小端字节序)	|
		|=======================================================================|

		1、 timestamp（时间戳）：占用3个字节，因此它最多能表示到 16777215 = 0xFFFFFF = 2^24-1, 当它的值超过这个最大值时，这三个字节都置为1，
			这样实际的 timestamp 会转存到 Extended Timestamp 字段中，
			接受端在判断 timestamp 字段 24个位 都为1时就会去 Extended timestamp 中解析实际的时间戳。

			T = timestatmp == 0xFFFFFF ? timestamp : Extended_timestamp
			单位 : 毫秒

		2、 message length（消息数据的长度）：占用3个字节，表示实际发送的消息的数据如音频帧、视频帧等数据的长度，单位是字节。
			注意这里是Message的长度，也就是chunk属于的Message的总数据长度，而不是chunk本身Data的数据的长度。

			VideoData / AudioData Length, 实际音视频数据长度

		3、 message type id(消息的类型id)：占用1个字节，表示实际发送的数据的类型

			0x01 --- Chunk Size
			0x02 --- AbortMessage
			0x03 --- Report(ACK)
			0x04 --- Control
			0x05 --- Server BandWidth(Windows ACK Size)
			0x06 --- Client BandWidth(Set Peer Bandwith)
			0x08 --- AudioData
			0x09 --- VideoData
			0x12 --- Indo Metadata
			0x14 --- Invoke

		4、 msg stream id（消息的流id）：占用4个字节，表示该 chunk 所在的 流的ID，和 Basic Header 的 CSID 一样，它采用  [小端存储]  的方式

	3、 Extended Timestamp（扩展时间戳）： 时间戳实际值，并非时间戳差值
		上面我们提到在chunk中会有时间戳 timestamp 和时间戳差 timestamp delta，并且它们不会和 此字段 同时存在，
		只有这两者之一大于3个字节能表示的最大数值 0xFFFFFF＝16777215 时，才会用这个字段来表示真正的时间戳，否则这个字段为0。
		扩展时间戳占4个字节，能表示的最大数值就是 0xFFFFFFFF＝4294967295。当扩展时间戳启用时，timestamp字段或者timestamp delta要全置为1，
		表示应该去扩展时间戳字段来提取真正的时间戳或者时间戳差。注意扩展时间戳存储的是完整值，而不是减去时间戳或者时间戳差的值。
*/
//#endif


int RTMP_ReadPacket(RTMP *r, RTMPPacket *packet)
{
	uint8_t hbuf[RTMP_MAX_HEADER_SIZE] = { 0 };
	char *header = (char *)hbuf;
	int nSize, hSize, nToRead, nChunk;
	int didAlloc = FALSE;
	int extendedTimestamp;

	//RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d", __FUNCTION__, r->m_sb.sb_socket);

	//1. Basic Header
	if (ReadN(r, (char *)hbuf, 1) == 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header", __FUNCTION__);
		return FALSE;
	}

	packet->m_headerType = (hbuf[0] & 0xc0) >> 6;

	packet->m_nChannel = (hbuf[0] & 0x3f);
	header++;
	if (packet->m_nChannel == 0)
	{
		if (ReadN(r, (char *)&hbuf[1], 1) != 1)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 2nd byte",
				__FUNCTION__);
			return FALSE;
		}
		packet->m_nChannel = hbuf[1];
		packet->m_nChannel += 64;
		header++;
	}
	else if (packet->m_nChannel == 1)
	{
		int tmp;
		if (ReadN(r, (char *)&hbuf[1], 2) != 2)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header 3nd byte",
				__FUNCTION__);
			return FALSE;
		}
		tmp = (hbuf[2] << 8) + hbuf[1];
		packet->m_nChannel = tmp + 64;
		RTMP_Log(RTMP_LOGDEBUG, "%s, m_nChannel: %0x", __FUNCTION__, packet->m_nChannel);
		header += 2;
	}

	//2. Message Header
	nSize = packetSize[packet->m_headerType];

	if (packet->m_nChannel >= r->m_channelsAllocatedIn)
	{
		int n = packet->m_nChannel + 10;
		int *timestamp = realloc(r->m_channelTimestamp, sizeof(int) * n);
		RTMPPacket **packets = realloc(r->m_vecChannelsIn, sizeof(RTMPPacket*) * n);
		if (!timestamp)
			free(r->m_channelTimestamp);
		if (!packets)
			free(r->m_vecChannelsIn);
		r->m_channelTimestamp = timestamp;
		r->m_vecChannelsIn = packets;
		if (!timestamp || !packets) {
			r->m_channelsAllocatedIn = 0;
			return FALSE;
		}
		memset(r->m_channelTimestamp + r->m_channelsAllocatedIn, 0, sizeof(int) * (n - r->m_channelsAllocatedIn));
		memset(r->m_vecChannelsIn + r->m_channelsAllocatedIn, 0, sizeof(RTMPPacket*) * (n - r->m_channelsAllocatedIn));
		r->m_channelsAllocatedIn = n;
	}

	if (nSize == RTMP_LARGE_HEADER_SIZE)	/* if we get a full header the timestamp is absolute */
		packet->m_hasAbsTimestamp = TRUE;

	else if (nSize < RTMP_LARGE_HEADER_SIZE)
	{	/* using values from the last message of this channel */

		//获取上次保存的数据包  get prev packet data
		if (r->m_vecChannelsIn[packet->m_nChannel])
			memcpy(packet, r->m_vecChannelsIn[packet->m_nChannel], sizeof(RTMPPacket));
	}

	nSize--;

	if (nSize > 0 && ReadN(r, header, nSize) != nSize)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet header. type: %x",
			__FUNCTION__, (unsigned int)hbuf[0]);
		return FALSE;
	}

	//MessageHeader + BasicHeader
	hSize = nSize + (header - (char *)hbuf);

	if (nSize >= 3)
	{
		//timestamp (3byte)
		packet->m_nTimeStamp = AMF_DecodeInt24(header);

		/*RTMP_Log(RTMP_LOGDEBUG, "%s, reading RTMP packet chunk on channel %x, headersz %i, timestamp %i, abs timestamp %i", __FUNCTION__, packet.m_nChannel, nSize, packet.m_nTimeStamp, packet.m_hasAbsTimestamp); */

		if (nSize >= 6)
		{
			//message length (3byte)
			packet->m_nBodySize = AMF_DecodeInt24(header + 3);
			packet->m_nBytesRead = 0;

			if (nSize > 6)
			{
				//message type id (1byte)
				packet->m_packetType = header[6];

				//message stream id (4byte)
				if (nSize == 11)
					packet->m_nInfoField2 = DecodeInt32LE(header + 7);
			}
		}
	}

	//RTMP_Log(RTMP_LOGDEBUG, "%s, csid : %0x, pkt_type : %0x", 
		//__FUNCTION__, packet->m_nChannel, packet->m_packetType);

	//3. Extended Timestamp（扩展时间戳）
	extendedTimestamp = packet->m_nTimeStamp == 0xffffff;
	if (extendedTimestamp)
	{
		if (ReadN(r, header + nSize, 4) != 4)
		{
			RTMP_Log(RTMP_LOGERROR, "%s, failed to read extended timestamp",
				__FUNCTION__);
			return FALSE;
		}
		packet->m_nTimeStamp = AMF_DecodeInt32(header + nSize);
		hSize += 4;
	}

	//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)hbuf, hSize);

	if (packet->m_nBodySize > 0 && packet->m_body == NULL)
	{
		if (!RTMPPacket_Alloc(packet, packet->m_nBodySize))
		{
			RTMP_Log(RTMP_LOGDEBUG, "%s, failed to allocate packet", __FUNCTION__);
			return FALSE;
		}
		didAlloc = TRUE;
		packet->m_headerType = (hbuf[0] & 0xc0) >> 6;
	}

	nToRead = packet->m_nBodySize - packet->m_nBytesRead;
	nChunk = r->m_inChunkSize;
	if (nToRead < nChunk)
		nChunk = nToRead;

	/* Does the caller want the raw chunk? */
	if (packet->m_chunk)
	{
		packet->m_chunk->c_headerSize = hSize;
		memcpy(packet->m_chunk->c_header, hbuf, hSize);
		packet->m_chunk->c_chunk = packet->m_body + packet->m_nBytesRead;
		packet->m_chunk->c_chunkSize = nChunk;
	}

	//追加到上一个包数据的后面  app data to prev packet data
	if (ReadN(r, packet->m_body + packet->m_nBytesRead, nChunk) != nChunk)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, failed to read RTMP packet body. len: %u",
			__FUNCTION__, packet->m_nBodySize);
		return FALSE;
	}
	
	//累加总数据长度
	packet->m_nBytesRead += nChunk;

	/* keep the packet as ref for other packets on this channel */
	if (!r->m_vecChannelsIn[packet->m_nChannel])
		r->m_vecChannelsIn[packet->m_nChannel] = malloc(sizeof(RTMPPacket));

	//保存当前总数据包 store current total packet data  
	memcpy(r->m_vecChannelsIn[packet->m_nChannel], packet, sizeof(RTMPPacket));
	if (extendedTimestamp)
	{
		r->m_vecChannelsIn[packet->m_nChannel]->m_nTimeStamp = 0xffffff;
	}

	if (RTMPPacket_IsReady(packet))
	{
		//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)packet->m_body, packet->m_nBodySize);

        RTMP_Log(RTMP_LOGDEBUG, 
        "fmt=%d,csid=%d,time=%lld,mlen=%d,mtype=%02x,msid=%d,prev_time=%lld\n",
        packet->m_headerType,packet->m_nChannel,packet->m_nTimeStamp,packet->m_nBodySize,
        packet->m_packetType,packet->m_nInfoField2, 
        r->m_channelTimestamp[packet->m_nChannel]);
	
		/* make packet's timestamp absolute */
		if (!packet->m_hasAbsTimestamp)
			packet->m_nTimeStamp += r->m_channelTimestamp[packet->m_nChannel];	/* timestamps seem to be always relative!! */

		r->m_channelTimestamp[packet->m_nChannel] = packet->m_nTimeStamp;

		/* reset the data from the stored packet. we keep the header since we may use it later if a new packet for this channel */
		/* arrives and requests to re-use some info (small packet header) */
		r->m_vecChannelsIn[packet->m_nChannel]->m_body = NULL;
		r->m_vecChannelsIn[packet->m_nChannel]->m_nBytesRead = 0;
		r->m_vecChannelsIn[packet->m_nChannel]->m_hasAbsTimestamp = FALSE;	/* can only be false if we reuse header */
	}
	else
	{
		packet->m_body = NULL;	/* so it won't be erased on free */
	}

	return TRUE;
}

#ifndef CRYPTO

// Client handshake to Server
static int
	HandShake(RTMP *r, int FP9HandShake)
{
	int i;
	uint32_t uptime, suptime;
	int bMatch;
	char type;
	char clientbuf[RTMP_SIG_SIZE + 1], *clientsig = clientbuf + 1;
	char serversig[RTMP_SIG_SIZE];

	//C0 
	clientbuf[0] = 0x03;		/* not encrypted */

	//C1 Time
	uptime = htonl(RTMP_GetTime());
	memcpy(clientsig, &uptime, 4);

	//C1 0值
	memset(&clientsig[4], 0, 4);

#ifdef _DEBUG
	for (i = 8; i < RTMP_SIG_SIZE; i++)
		clientsig[i] = 0xff;
#else
	//C1 Random
	for (i = 8; i < RTMP_SIG_SIZE; i++)
		clientsig[i] = (char)(rand() % 256);
#endif

	// Send C0C1
	if (!WriteN(r, clientbuf, RTMP_SIG_SIZE + 1))
		return FALSE;

	// Read S0
	if (ReadN(r, &type, 1) != 1)	/* 0x03 or 0x06 */
		return FALSE;

	RTMP_Log(RTMP_LOGDEBUG, "%s: Type Answer   : %02X", __FUNCTION__, type);

	if (type != clientbuf[0])
		RTMP_Log(RTMP_LOGWARNING, "%s: Type mismatch: client sent %d, server answered %d",
		__FUNCTION__, clientbuf[0], type);

	// Read S1
	if (ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
		return FALSE;

	/* decode server response */

	memcpy(&suptime, serversig, 4);
	suptime = ntohl(suptime);

	RTMP_Log(RTMP_LOGDEBUG, "%s: Server Uptime : %d", __FUNCTION__, suptime);
	RTMP_Log(RTMP_LOGDEBUG, "%s: FMS Version   : %d.%d.%d.%d", __FUNCTION__,
		serversig[4], serversig[5], serversig[6], serversig[7]);

	/* 2nd part of handshake */

	// Send C2
	if (!WriteN(r, serversig, RTMP_SIG_SIZE))
		return FALSE;

	// Read S2
	if (ReadN(r, serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
		return FALSE;

	bMatch = (memcmp(serversig, clientsig, RTMP_SIG_SIZE) == 0);
	if (!bMatch)
	{
		RTMP_Log(RTMP_LOGWARNING, "%s, client signature does not match!", __FUNCTION__);
	}
	return TRUE;
}

/*
	1、 c0与s0格式
		c0和s0包是一个1字节,可以看作是一个byte

		|--------------|
		| Time (4byte) |
		|--------------|

		1、 简单握手：
        		作用：C0和S0一致，都是一个字节，都代表当前使用的rtmp协议的版本号。
        		如果服务器端或者客户端收到的C0/S0字段解析出为非03，对端可以选择以版本3来响应，也可以放弃握手。

        		版本号 : 1字节
			0x03  0x06

		2、 复杂握手：
        		作用：说明是明文还是密文。

        		如果使用的是明文（0X03），同时代表当前使用的rtmp协议的版本号。

        		如果是密文，该位为0x06
			
		目前rtmp版本定义为3,0-2是早期的专利产品所使用的值,现已经废弃,4-31是预留值,32-255是禁用值(这样做是为了区分基于文本的协议,
		因为这些协议通常都是以一个可打印的字符开始),如果服务端不能识别客户请求的版本,那么它应该发送3的响应,客户端这时可以选择下降到版本3,也可以放弃这次握手

	2、 c1与s1格式
		c1与s1长度为1536个字节,它们由以下字段组成

		1、 简单握手：

			|--------------|--------------|-------------------|
			| Time (4byte) | Zero (4byte) | Random (1528byte) |
			|--------------|--------------|-------------------|
			
			1、 时间戳 : 4字节
				包含了一个时间戳,它是所有从这个端点发送出去的将来数据块的起始点,
				它可以是零,或是任意值,为了同步多个数据块流,端点可能会将这个字段设成其它数据块流时间戳的当前值.
				
			2、 0 : 4字节	
				必须是0	这个字段必须都是0。如果不是0，代表要使用complex handshack。
				
			3、 随机数 : 1528字节
				可以是任意值,因为每个端点必须区分已经初始化的握手和对等端点初始化的握手的响应,所以这个数据要足够的随机,
				当然这个也不需要密码级的随机或是动态值.

		2、 复杂握手：
			C1S1 Schemal0
			|--------------|-----------------|-----------------|------------------|
			| Time (4byte) | Version (4byte) |  Key (764byte)  | Digest (764byte) |
			|--------------|-----------------|-----------------|------------------|

			C1S1 Schemal1
			|--------------|-----------------|------------------|-----------------|
			| Time (4byte) | Version (4byte) | Digest (764byte) |  Key (764byte)  |
			|--------------|-----------------|------------------|-----------------|

			1、 Time（4字节）：这个字段包含一个timestamp，用于本终端发送的所有后续块的时间起点。
				这个值可以是0，或者一些任意值。要同步多个块流，终端可以发送其他块流当前的timestamp的值，以此让当前流跟要同步的流保持时间上的同步。

			2、 Version(4个字节)：4bytes 为程序版本。C1一般是0x80000702。S1是0x04050001。貌似这个可以随意填写，但是要采用非0值跟simple handshack区分。

			3、 Key(764个字节)：
				1) random-data：长度由这个字段的最后4个byte决定，即761-764
				2) key-data：128个字节。Key字段对应C1和S1有不同的算法，这个需要注意。后面会详细解释。
					发送端（C1）中的Key应该是随机的，接收端（S1）的key需要按照发送端的key去计算然后返回给发送端。
				3) random-data：（764-offset-128-4）个字节
				4) key_offset：4字节, 最后4字节定义了key的offset（相对于KeyBlock开头而言，相当于第一个random_data的长度）

				Key (764byte)
				|---------------------|---------------|---------------------------------|------------------|
				| Random (Offset byte)| Key (128byte) | Random (764 - Offset - 128 - 4) |  Offset (4byte)  |
				|---------------------|---------------|---------------------------------|------------------|

			4、 Digest(764个字节)：
				1) offset：4字节, 开头4字节定义了digest的offset
				（相对于DigestBlock的第5字节而言，offset=3表示digestBlock[7~38]为digest，【4-6】即为第一个random_data）
				2) random-data：长度由这个字段起始的4个byte决定
				3) digest-data：32个字节。Digest字段对应C1和S1有不同的算法，这个需要注意。后面会详细解释。
				4) random-data：（764-4-offset-32）个字节

				Digest (764字节)
				|-----------------|----------------------|------------------|--------------------------------|
				|  Offset (4byte) | Random (Offset byte) | Digest (32 byte) | Random (764 - 4 - Offset - 32) |
				|-----------------|----------------------|------------------|--------------------------------|

			5、 算法：
				1) C1 的 key 为 128bytes 随机数。
				2) C1_32bytes_digest= HMACsha256(P1+P2, 1504, FPKey, 30) ，
					其中P1为digest之前的部分，P2为digest之后的部分，P1+P2是将这两部分拷贝到新的数组，共1536-32长度。
					
				3) S1的key根据 C1的key算出来。
				4) S1的digest算法同C1。注意，必须先计算S1的key，因为key变化后digest也重新计算。

	3、 c2与s2格式
		c2和s2包长都是1536字节, 几乎是 s1 和 c1 的回显.

		1、 简单握手：
		    	作用：基本是C1&S1的副本

		    	C2S2
			|--------------|--------------|-------------------|
			| Time (4byte) | Time (4byte) | Random (1528byte) |
			|--------------|--------------|-------------------|

			1、 time : 4字节
				包含有对方发送过来 s1 或 c1 的时间戳
				这个字段必须包含终端在S1 (给 C2) 或者 C1 (给 S2) 发的 timestamp。
				
			2、 time2 : 4字节
				包含有对方发送过来的前一个包(s1或者c1)的时间戳
				这个字段必须包含终端先前发出数据包 (s1 或者 c1) timestamp。

			3、 随机数 : 1528字节
				包含有对方发送过来的随机数据字段,每个通信端点可以使用 time1 和 time2 字段,以及当前的时间戳,来快速估计带宽和/或连接延时,
				但这个数值基本上没法用.

			4、 校验 : S1 == C2  或者  C1 == S2

		2、 复杂握手：
			作用：主要是用来提供对 C1 S1 的验证

			C2S2
			|--------------------|------------------|
			| Random (1504 byte) | Digest (32 byte) |
			|--------------------|------------------|

			验证算法: 
				分别拿到C1 和S1的数据，按照上文定义的计算方法再将C1或S1的digest字段计算一遍，跟当前从C1和S1中拿到的Digest字段进行比较
			
*/


// 
static int
	SHandShake(RTMP *r)
{
	int i;
	char serverbuf[RTMP_SIG_SIZE + 1], *serversig = serverbuf + 1;
	char clientsig[RTMP_SIG_SIZE];
	uint32_t uptime;
	int bMatch;

	//Read C0 版本号
	if (ReadN(r, serverbuf, 1) != 1)	/* 0x03 or 0x06 */
		return FALSE;

	RTMP_Log(RTMP_LOGDEBUG, "%s: Type Request  : %02X", __FUNCTION__, serverbuf[0]);

	if (serverbuf[0] != 3)
	{
		RTMP_Log(RTMP_LOGERROR, "%s: Type unknown: client sent %02X",
			__FUNCTION__, serverbuf[0]);
		return FALSE;
	}

	//S1 time 4字节
	uptime = htonl(RTMP_GetTime());
	memcpy(serversig, &uptime, 4);

	//S1 0值 4字节
	memset(&serversig[4], 0, 4);
#ifdef _DEBUG
	for (i = 8; i < RTMP_SIG_SIZE; i++)
		serversig[i] = 0xff;
#else

	//S1 随机数 1528字节
	for (i = 8; i < RTMP_SIG_SIZE; i++)
		serversig[i] = (char)(rand() % 256);
#endif


	// Send   S0S1
	if (!WriteN(r, serverbuf, RTMP_SIG_SIZE + 1))
		return FALSE;


	// Read C1
	if (ReadN(r, clientsig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
		return FALSE;

	/* decode client response */

	memcpy(&uptime, clientsig, 4);
	uptime = ntohl(uptime);

	RTMP_Log(RTMP_LOGDEBUG, "%s: Client Uptime : %d", __FUNCTION__, uptime);
	RTMP_Log(RTMP_LOGDEBUG, "%s: Player Version: %d.%d.%d.%d", __FUNCTION__,
		clientsig[4], clientsig[5], clientsig[6], clientsig[7]);

	/* 2nd part of handshake */

	// Send S2
	if (!WriteN(r, clientsig, RTMP_SIG_SIZE))
		return FALSE;

	// Read C2
	if (ReadN(r, clientsig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
		return FALSE;

	bMatch = (memcmp(serversig, clientsig, RTMP_SIG_SIZE) == 0);
	if (!bMatch)
	{
		RTMP_Log(RTMP_LOGWARNING, "%s, client signature does not match!", __FUNCTION__);
	}
	return TRUE;
}
#endif

int
	RTMP_SendChunk(RTMP *r, RTMPChunk *chunk)
{
	int wrote;
	char hbuf[RTMP_MAX_HEADER_SIZE];

	RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d, size=%d", __FUNCTION__, r->m_sb.sb_socket,
		chunk->c_chunkSize);
	RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)chunk->c_header, chunk->c_headerSize);
	if (chunk->c_chunkSize)
	{
		char *ptr = chunk->c_chunk - chunk->c_headerSize;
		RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)chunk->c_chunk, chunk->c_chunkSize);
		/* save header bytes we're about to overwrite */
		memcpy(hbuf, ptr, chunk->c_headerSize);
		memcpy(ptr, chunk->c_header, chunk->c_headerSize);
		wrote = WriteN(r, ptr, chunk->c_headerSize + chunk->c_chunkSize);
		memcpy(ptr, hbuf, chunk->c_headerSize);
	}
	else
		wrote = WriteN(r, chunk->c_header, chunk->c_headerSize);
	return wrote;
}


int RTMP_SendPacket(RTMP *r, RTMPPacket *packet, int queue)
{
	const RTMPPacket *prevPacket;
	uint32_t last = 0;
	int nSize;
	int hSize, cSize;
	char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
	uint32_t t;
	char *buffer, *tbuf = NULL, *toff = NULL;
	int nChunkSize;
	int tlen;

    

	if (packet->m_nChannel >= r->m_channelsAllocatedOut)
	{
		int n = packet->m_nChannel + 10;
		RTMPPacket **packets = realloc(r->m_vecChannelsOut, sizeof(RTMPPacket*) * n);
		if (!packets) {
			free(r->m_vecChannelsOut);
			r->m_vecChannelsOut = NULL;
			r->m_channelsAllocatedOut = 0;
			return FALSE;
		}
		r->m_vecChannelsOut = packets;
		memset(r->m_vecChannelsOut + r->m_channelsAllocatedOut, 0, sizeof(RTMPPacket*) * (n - r->m_channelsAllocatedOut));
		r->m_channelsAllocatedOut = n;
	}

    RTMP_Log(RTMP_LOGINFO, 
        "Send ==> fmt=%d,csid=%d,time=%lld,mlen=%d,mtype=%02x,msid=%d\n",
        packet->m_headerType,packet->m_nChannel,packet->m_nTimeStamp,packet->m_nBodySize,
        packet->m_packetType,packet->m_nInfoField2);

    RTMP_LogHexString(RTMP_LOGINFO, (uint8_t *)packet->m_body, packet->m_nBodySize);

	prevPacket = r->m_vecChannelsOut[packet->m_nChannel];
	if (prevPacket && packet->m_headerType != RTMP_PACKET_SIZE_LARGE)
	{
		/* compress a bit by using the prev packet's attributes */
		if (prevPacket->m_nBodySize == packet->m_nBodySize
			&& prevPacket->m_packetType == packet->m_packetType
			&& packet->m_headerType == RTMP_PACKET_SIZE_MEDIUM)
			packet->m_headerType = RTMP_PACKET_SIZE_SMALL;

		if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
			&& packet->m_headerType == RTMP_PACKET_SIZE_SMALL)
			packet->m_headerType = RTMP_PACKET_SIZE_MINIMUM;
		last = prevPacket->m_nTimeStamp;
	}

	if (packet->m_headerType > 3)	/* sanity */
	{
		RTMP_Log(RTMP_LOGERROR, "sanity failed!! trying to send header of type: 0x%02x.",
			(unsigned char)packet->m_headerType);
		return FALSE;
	}

	nSize = packetSize[packet->m_headerType];
	hSize = nSize; cSize = 0;
	t = packet->m_nTimeStamp - last;

	if (packet->m_body)
	{
		header = packet->m_body - nSize;
		hend = packet->m_body;
	}
	else
	{
		header = hbuf + 6;
		hend = hbuf + sizeof(hbuf);
	}

	if (packet->m_nChannel > 319)
		cSize = 2;
	else if (packet->m_nChannel > 63)
		cSize = 1;
	if (cSize)
	{
		header -= cSize;
		hSize += cSize;
	}

	if (t >= 0xffffff)
	{
		header -= 4;
		hSize += 4;
		RTMP_Log(RTMP_LOGWARNING, "Larger timestamp than 24-bit: 0x%x", t);
	}

	hptr = header;
	c = packet->m_headerType << 6;
	
	switch (cSize)
	{
	case 0:
		c |= packet->m_nChannel;
		break;
	case 1:
		break;
	case 2:
		c |= 1;
		break;
	}

	// BasicHeader (1byte)
	*hptr++ = c;
	
	if (cSize)
	{
		int tmp = packet->m_nChannel - 64;

		// BasicHeader (2byte)
		*hptr++ = tmp & 0xff;

		// BasicHeader (3byte)
		if (cSize == 2)
			*hptr++ = tmp >> 8;
	}

	if (nSize > 1)
	{	
		// timestamp (3byte)
		hptr = AMF_EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
	}

	if (nSize > 4)
	{	
		// message length (3byte)
		hptr = AMF_EncodeInt24(hptr, hend, packet->m_nBodySize);

		// message type id  (1byte)
		*hptr++ = packet->m_packetType;
	}

	// message stream id  (4byte)
	if (nSize > 8)
		hptr += EncodeInt32LE(hptr, packet->m_nInfoField2);

	// extern timestamp (4byte)
	if (t >= 0xffffff)
		hptr = AMF_EncodeInt32(hptr, hend, t);

	nSize = packet->m_nBodySize;
	buffer = packet->m_body;
	nChunkSize = r->m_outChunkSize;

	//RTMP_Log(RTMP_LOGDEBUG2, "%s: fd=%d, size=%d", __FUNCTION__, r->m_sb.sb_socket,
		//nSize);
	/* send all chunks in one HTTP request */
	if (r->Link.protocol & RTMP_FEATURE_HTTP)
	{
		int chunks = (nSize+nChunkSize-1) / nChunkSize;
		if (chunks > 1)
		{
			tlen = chunks * (cSize + 1) + nSize + hSize;
			tbuf = malloc(tlen);
			if (!tbuf)
				return FALSE;
			toff = tbuf;
		}
	}
	while (nSize + hSize)
	{
		int wrote;

		if (nSize < nChunkSize)
			nChunkSize = nSize;

		//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)header, hSize);
		//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)buffer, nChunkSize);
		if (tbuf)
		{
			memcpy(toff, header, nChunkSize + hSize);
			toff += nChunkSize + hSize;
		}
		else
		{
			// Send one chunk
			wrote = WriteN(r, header, nChunkSize + hSize);
			if (!wrote)
				return FALSE;
		}
		
		nSize -= nChunkSize;	//包大小 减少 chunksize
		buffer += nChunkSize;	//包数据后移 chunksize
		hSize = 0;


		//一个 Chunk 未发送完一个数据包的内容，剩下的数据 由 消息类型 fmt = 3 的 Chunk 发送
		if (nSize > 0)
		{
			header = buffer - 1;
			hSize = 1;
			if (cSize)
			{
				header -= cSize;
				hSize += cSize;
			}
			if (t >= 0xffffff)
			{
				header -= 4;
				hSize += 4;
			}

			// Basic Header
			*header = (0xc0 | c);		// 1 1 0 0 | 0 0 0 0   ===> fmt == 3
			if (cSize)
			{
				int tmp = packet->m_nChannel - 64;
				header[1] = tmp & 0xff;
				if (cSize == 2)
					header[2] = tmp >> 8;
			}

			// extern timestamp
			if (t >= 0xffffff)
			{
				char* extendedTimestamp = header + 1 + cSize;
				AMF_EncodeInt32(extendedTimestamp, extendedTimestamp + 4, t);
			}
		}
	}
	if (tbuf)
	{
		int wrote = WriteN(r, tbuf, toff-tbuf);
		free(tbuf);
		tbuf = NULL;
		if (!wrote)
			return FALSE;
	}

	/* we invoked a remote method */
	if (packet->m_packetType == RTMP_PACKET_TYPE_INVOKE)
	{
		AVal method;
		char *ptr;
		ptr = packet->m_body + 1;
		AMF_DecodeString(ptr, &method);
		RTMP_Log(RTMP_LOGDEBUG, "Invoking %s", method.av_val);
		/* keep it in call queue till result arrives */
		if (queue) {
			int txn;
			ptr += 3 + method.av_len;
			txn = (int)AMF_DecodeNumber(ptr);
			AV_queue(&r->m_methodCalls, &r->m_numCalls, &method, txn);
		}
	}

	if (!r->m_vecChannelsOut[packet->m_nChannel])
		r->m_vecChannelsOut[packet->m_nChannel] = malloc(sizeof(RTMPPacket));

	// 保存本次发送的包
	memcpy(r->m_vecChannelsOut[packet->m_nChannel], packet, sizeof(RTMPPacket));
	return TRUE;
}

int
	RTMP_Serve(RTMP *r)
{
	return SHandShake(r);
}

void
	RTMP_Close(RTMP *r)
{
	CloseInternal(r, 0);
}

static void
	CloseInternal(RTMP *r, int reconnect)
{
	int i;

	if (RTMP_IsConnected(r))
	{
		if (r->m_stream_id > 0)
		{
			i = r->m_stream_id;
			r->m_stream_id = 0;
			if ((r->Link.protocol & RTMP_FEATURE_WRITE))
				SendFCUnpublish(r);
			SendDeleteStream(r, i);
		}
		if (r->m_clientID.av_val)
		{
			HTTP_Post(r, RTMPT_CLOSE, "", 1);
			free(r->m_clientID.av_val);
			r->m_clientID.av_val = NULL;
			r->m_clientID.av_len = 0;
		}
		RTMPSockBuf_Close(&r->m_sb);
	}

	r->m_stream_id = -1;
	r->m_sb.sb_socket = -1;
	r->m_nBWCheckCounter = 0;
	r->m_nBytesIn = 0;
	r->m_nBytesInSent = 0;

	if (r->m_read.flags & RTMP_READ_HEADER) {
		free(r->m_read.buf);
		r->m_read.buf = NULL;
	}
	r->m_read.dataType = 0;
	r->m_read.flags = 0;
	r->m_read.status = 0;
	r->m_read.nResumeTS = 0;
	r->m_read.nIgnoredFrameCounter = 0;
	r->m_read.nIgnoredFlvFrameCounter = 0;

	r->m_write.m_nBytesRead = 0;
	RTMPPacket_Free(&r->m_write);

	for (i = 0; i < r->m_channelsAllocatedIn; i++)
	{
		if (r->m_vecChannelsIn[i])
		{
			RTMPPacket_Free(r->m_vecChannelsIn[i]);
			free(r->m_vecChannelsIn[i]);
			r->m_vecChannelsIn[i] = NULL;
		}
	}
	free(r->m_vecChannelsIn);
	r->m_vecChannelsIn = NULL;
	free(r->m_channelTimestamp);
	r->m_channelTimestamp = NULL;
	r->m_channelsAllocatedIn = 0;
	for (i = 0; i < r->m_channelsAllocatedOut; i++)
	{
		if (r->m_vecChannelsOut[i])
		{
			free(r->m_vecChannelsOut[i]);
			r->m_vecChannelsOut[i] = NULL;
		}
	}
	free(r->m_vecChannelsOut);
	r->m_vecChannelsOut = NULL;
	r->m_channelsAllocatedOut = 0;
	AV_clear(r->m_methodCalls, r->m_numCalls);
	r->m_methodCalls = NULL;
	r->m_numCalls = 0;
	r->m_numInvokes = 0;

	r->m_bPlaying = FALSE;
	r->m_sb.sb_size = 0;

	r->m_msgCounter = 0;
	r->m_resplen = 0;
	r->m_unackd = 0;

	if (r->Link.lFlags & RTMP_LF_FTCU && !reconnect)
	{
		free(r->Link.tcUrl.av_val);
		r->Link.tcUrl.av_val = NULL;
		r->Link.lFlags ^= RTMP_LF_FTCU;
	}
	if (r->Link.lFlags & RTMP_LF_FAPU && !reconnect)
	{
		free(r->Link.app.av_val);
		r->Link.app.av_val = NULL;
		r->Link.lFlags ^= RTMP_LF_FAPU;
	}

	if (!reconnect)
	{
		free(r->Link.playpath0.av_val);
		r->Link.playpath0.av_val = NULL;
	}
#ifdef CRYPTO
	if (r->Link.dh)
	{
		MDH_free(r->Link.dh);
		r->Link.dh = NULL;
	}
	if (r->Link.rc4keyIn)
	{
		RC4_free(r->Link.rc4keyIn);
		r->Link.rc4keyIn = NULL;
	}
	if (r->Link.rc4keyOut)
	{
		RC4_free(r->Link.rc4keyOut);
		r->Link.rc4keyOut = NULL;
	}
#endif
}

int
	RTMPSockBuf_Fill(RTMPSockBuf *sb)
{
	int nBytes;

	if (!sb->sb_size)
		sb->sb_start = sb->sb_buf;

	while (1)
	{
		nBytes = sizeof(sb->sb_buf) - 1 - sb->sb_size - (sb->sb_start - sb->sb_buf);
#if defined(CRYPTO) && !defined(NO_SSL)
		if (sb->sb_ssl)
		{
			nBytes = TLS_read(sb->sb_ssl, sb->sb_start + sb->sb_size, nBytes);
		}
		else
#endif
		{
			nBytes = recv(sb->sb_socket, sb->sb_start + sb->sb_size, nBytes, 0);
		}
		if (nBytes != -1)
		{
			sb->sb_size += nBytes;
		}
		else
		{
			int sockerr = GetSockError();
			RTMP_Log(RTMP_LOGDEBUG, "%s, recv returned %d. GetSockError(): %d (%s)",
				__FUNCTION__, nBytes, sockerr, strerror(sockerr));
			if (sockerr == EINTR && !RTMP_ctrlC)
				continue;

			if (sockerr == EWOULDBLOCK || sockerr == EAGAIN)
			{
				sb->sb_timedout = TRUE;
				nBytes = 0;
			}
		}
		break;
	}

	return nBytes;
}

int
	RTMPSockBuf_Send(RTMPSockBuf *sb, const char *buf, int len)
{
	int rc;

#ifdef _DEBUG
	fwrite(buf, 1, len, netstackdump);
#endif

#if defined(CRYPTO) && !defined(NO_SSL)
	if (sb->sb_ssl)
	{
		rc = TLS_write(sb->sb_ssl, buf, len);
	}
	else
#endif
	{
		rc = send(sb->sb_socket, buf, len, 0);
	}
	return rc;
}

int
	RTMPSockBuf_Close(RTMPSockBuf *sb)
{
#if defined(CRYPTO) && !defined(NO_SSL)
	if (sb->sb_ssl)
	{
		TLS_shutdown(sb->sb_ssl);
		TLS_close(sb->sb_ssl);
		sb->sb_ssl = NULL;
	}
#endif
	if (sb->sb_socket != -1)
		return closesocket(sb->sb_socket);
	return 0;
}

#define HEX2BIN(a)	(((a)&0x40)?((a)&0xf)+9:((a)&0xf))

static void
	DecodeTEA(AVal *key, AVal *text)
{
	uint32_t *v, k[4] = { 0 }, u;
	uint32_t z, y, sum = 0, e, DELTA = 0x9e3779b9;
	int32_t p, q;
	int i, n;
	unsigned char *ptr, *out;

	/* prep key: pack 1st 16 chars into 4 LittleEndian ints */
	ptr = (unsigned char *)key->av_val;
	u = 0;
	n = 0;
	v = k;
	p = key->av_len > 16 ? 16 : key->av_len;
	for (i = 0; i < p; i++)
	{
		u |= ptr[i] << (n * 8);
		if (n == 3)
		{
			*v++ = u;
			u = 0;
			n = 0;
		}
		else
		{
			n++;
		}
	}
	/* any trailing chars */
	if (u)
		*v = u;

	/* prep text: hex2bin, multiples of 4 */
	n = (text->av_len + 7) / 8;
	out = malloc(n * 8);
	ptr = (unsigned char *)text->av_val;
	v = (uint32_t *) out;
	for (i = 0; i < n; i++)
	{
		u = (HEX2BIN(ptr[0]) << 4) + HEX2BIN(ptr[1]);
		u |= ((HEX2BIN(ptr[2]) << 4) + HEX2BIN(ptr[3])) << 8;
		u |= ((HEX2BIN(ptr[4]) << 4) + HEX2BIN(ptr[5])) << 16;
		u |= ((HEX2BIN(ptr[6]) << 4) + HEX2BIN(ptr[7])) << 24;
		*v++ = u;
		ptr += 8;
	}
	v = (uint32_t *) out;

	/* http://www.movable-type.co.uk/scripts/tea-block.html */
#define MX (((z>>5)^(y<<2)) + ((y>>3)^(z<<4))) ^ ((sum^y) + (k[(p&3)^e]^z));
	z = v[n - 1];
	y = v[0];
	q = 6 + 52 / n;
	sum = q * DELTA;
	while (sum != 0)
	{
		e = sum >> 2 & 3;
		for (p = n - 1; p > 0; p--)
			z = v[p - 1], y = v[p] -= MX;
		z = v[n - 1];
		y = v[0] -= MX;
		sum -= DELTA;
	}

	text->av_len /= 2;
	memcpy(text->av_val, out, text->av_len);
	free(out);
}

static int
	HTTP_Post(RTMP *r, RTMPTCmd cmd, const char *buf, int len)
{
	char hbuf[512];
	int hlen = snprintf(hbuf, sizeof(hbuf), 
		"POST /%s%s/%d HTTP/1.1\r\n"
		"Host: %.*s:%d\r\n"
		"Accept: */*\r\n"
		"User-Agent: Shockwave Flash\r\n"
		"Connection: Keep-Alive\r\n"
		"Cache-Control: no-cache\r\n"
		"Content-type: application/x-fcs\r\n"
		"Content-length: %d\r\n\r\n", 
		
		RTMPT_cmds[cmd], r->m_clientID.av_val ? r->m_clientID.av_val : "", r->m_msgCounter, 
		
		r->Link.hostname.av_len, r->Link.hostname.av_val, r->Link.port, len);
	
	RTMPSockBuf_Send(&r->m_sb, hbuf, hlen);
	hlen = RTMPSockBuf_Send(&r->m_sb, buf, len);
	r->m_msgCounter++;
	r->m_unackd++;
	return hlen;
}

static int
	HTTP_read(RTMP *r, int fill)
{
	char *ptr;
	int hlen;

restart:
	if (fill)
		RTMPSockBuf_Fill(&r->m_sb);
	if (r->m_sb.sb_size < 13) {
		if (fill)
			goto restart;
		return -2;
	}
	if (strncmp(r->m_sb.sb_start, "HTTP/1.1 200 ", 13))
		return -1;
	r->m_sb.sb_start[r->m_sb.sb_size] = '\0';
	if (!strstr(r->m_sb.sb_start, "\r\n\r\n")) {
		if (fill)
			goto restart;
		return -2;
	}

	ptr = r->m_sb.sb_start + sizeof("HTTP/1.1 200");
	while ((ptr = strstr(ptr, "Content-"))) {
		if (!strncasecmp(ptr+8, "length:", 7)) break;
		ptr += 8;
	}
	if (!ptr)
		return -1;
	hlen = atoi(ptr+16);
	ptr = strstr(ptr+16, "\r\n\r\n");
	if (!ptr)
		return -1;
	ptr += 4;
	if (ptr + (r->m_clientID.av_val ? 1 : hlen) > r->m_sb.sb_start + r->m_sb.sb_size)
	{
		if (fill)
			goto restart;
		return -2;
	}
	r->m_sb.sb_size -= ptr - r->m_sb.sb_start;
	r->m_sb.sb_start = ptr;
	r->m_unackd--;

	if (!r->m_clientID.av_val)
	{
		r->m_clientID.av_len = hlen;
		r->m_clientID.av_val = malloc(hlen+1);
		if (!r->m_clientID.av_val)
			return -1;
		r->m_clientID.av_val[0] = '/';
		memcpy(r->m_clientID.av_val+1, ptr, hlen-1);
		r->m_clientID.av_val[hlen] = 0;
		r->m_sb.sb_size = 0;
	}
	else
	{
		r->m_polling = *ptr++;
		r->m_resplen = hlen - 1;
		r->m_sb.sb_start++;
		r->m_sb.sb_size--;
	}
	return 0;
}

#define MAX_IGNORED_FRAMES	50

/* Read from the stream until we get a media packet.
* Returns -3 if Play.Close/Stop, -2 if fatal error, -1 if no more media
* packets, 0 if ignorable error, >0 if there is a media packet
*/
static int
	Read_1_Packet(RTMP *r, char *buf, unsigned int buflen)
{
	uint32_t prevTagSize = 0;
	int rtnGetNextMediaPacket = 0, ret = RTMP_READ_EOF;
	RTMPPacket packet = { 0 };
	int recopy = FALSE;
	unsigned int size;
	char *ptr, *pend;
	uint32_t nTimeStamp = 0;
	unsigned int len;

	rtnGetNextMediaPacket = RTMP_GetNextMediaPacket(r, &packet);
	while (rtnGetNextMediaPacket)
	{
		char *packetBody = packet.m_body;
		unsigned int nPacketLen = packet.m_nBodySize;

		/* Return RTMP_READ_COMPLETE if this was completed nicely with
		* invoke message Play.Stop or Play.Complete
		*/
		if (rtnGetNextMediaPacket == 2)
		{
			RTMP_Log(RTMP_LOGDEBUG,
				"Got Play.Complete or Play.Stop from server. "
				"Assuming stream is complete");
			ret = RTMP_READ_COMPLETE;
			break;
		}

		r->m_read.dataType |= (((packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) << 2) |
			(packet.m_packetType == RTMP_PACKET_TYPE_VIDEO));

		if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO && nPacketLen <= 5)
		{
			RTMP_Log(RTMP_LOGDEBUG, "ignoring too small video packet: size: %d",
				nPacketLen);
			ret = RTMP_READ_IGNORE;
			break;
		}
		if (packet.m_packetType == RTMP_PACKET_TYPE_AUDIO && nPacketLen <= 1)
		{
			RTMP_Log(RTMP_LOGDEBUG, "ignoring too small audio packet: size: %d",
				nPacketLen);
			ret = RTMP_READ_IGNORE;
			break;
		}

		if (r->m_read.flags & RTMP_READ_SEEKING)
		{
			ret = RTMP_READ_IGNORE;
			break;
		}
#ifdef _DEBUG
		RTMP_Log(RTMP_LOGDEBUG, "type: %02X, size: %d, TS: %d ms, abs TS: %d",
			packet.m_packetType, nPacketLen, packet.m_nTimeStamp,
			packet.m_hasAbsTimestamp);
		if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO)
			RTMP_Log(RTMP_LOGDEBUG, "frametype: %02X", (*packetBody & 0xf0));
#endif

		if (r->m_read.flags & RTMP_READ_RESUME)
		{
			/* check the header if we get one */
			if (packet.m_nTimeStamp == 0)
			{
				if (r->m_read.nMetaHeaderSize > 0
					&& packet.m_packetType == RTMP_PACKET_TYPE_INFO)
				{
					AMFObject metaObj;
					int nRes =
						AMF_Decode(&metaObj, packetBody, nPacketLen, FALSE);
					if (nRes >= 0)
					{
						AVal metastring;
						AMFProp_GetString(AMF_GetProp(&metaObj, NULL, 0),
							&metastring);

						if (AVMATCH(&metastring, &av_onMetaData))
						{
							/* compare */
							if ((r->m_read.nMetaHeaderSize != nPacketLen) ||
								(memcmp
								(r->m_read.metaHeader, packetBody,
								r->m_read.nMetaHeaderSize) != 0))
							{
								ret = RTMP_READ_ERROR;
							}
						}
						AMF_Reset(&metaObj);
						if (ret == RTMP_READ_ERROR)
							break;
					}
				}

				/* check first keyframe to make sure we got the right position
				* in the stream! (the first non ignored frame)
				*/
				if (r->m_read.nInitialFrameSize > 0)
				{
					/* video or audio data */
					if (packet.m_packetType == r->m_read.initialFrameType
						&& r->m_read.nInitialFrameSize == nPacketLen)
					{
						/* we don't compare the sizes since the packet can
						* contain several FLV packets, just make sure the
						* first frame is our keyframe (which we are going
						* to rewrite)
						*/
						if (memcmp
							(r->m_read.initialFrame, packetBody,
							r->m_read.nInitialFrameSize) == 0)
						{
							RTMP_Log(RTMP_LOGDEBUG, "Checked keyframe successfully!");
							r->m_read.flags |= RTMP_READ_GOTKF;
							/* ignore it! (what about audio data after it? it is
							* handled by ignoring all 0ms frames, see below)
							*/
							ret = RTMP_READ_IGNORE;
							break;
						}
					}

					/* hande FLV streams, even though the server resends the
					* keyframe as an extra video packet it is also included
					* in the first FLV stream chunk and we have to compare
					* it and filter it out !!
					*/
					if (packet.m_packetType == RTMP_PACKET_TYPE_FLASH_VIDEO)
					{
						/* basically we have to find the keyframe with the
						* correct TS being nResumeTS
						*/
						unsigned int pos = 0;
						uint32_t ts = 0;

						while (pos + 11 < nPacketLen)
						{
							/* size without header (11) and prevTagSize (4) */
							uint32_t dataSize =
								AMF_DecodeInt24(packetBody + pos + 1);
							ts = AMF_DecodeInt24(packetBody + pos + 4);
							ts |= (packetBody[pos + 7] << 24);

#ifdef _DEBUG
							RTMP_Log(RTMP_LOGDEBUG,
								"keyframe search: FLV Packet: type %02X, dataSize: %d, timeStamp: %d ms",
								packetBody[pos], dataSize, ts);
#endif
							/* ok, is it a keyframe?:
							* well doesn't work for audio!
							*/
							if (packetBody[pos /*6928, test 0 */ ] ==
								r->m_read.initialFrameType
								/* && (packetBody[11]&0xf0) == 0x10 */ )
							{
								if (ts == r->m_read.nResumeTS)
								{
									RTMP_Log(RTMP_LOGDEBUG,
										"Found keyframe with resume-keyframe timestamp!");
									if (r->m_read.nInitialFrameSize != dataSize
										|| memcmp(r->m_read.initialFrame,
										packetBody + pos + 11,
										r->m_read.
										nInitialFrameSize) != 0)
									{
										RTMP_Log(RTMP_LOGERROR,
											"FLV Stream: Keyframe doesn't match!");
										ret = RTMP_READ_ERROR;
										break;
									}
									r->m_read.flags |= RTMP_READ_GOTFLVK;

									/* skip this packet?
									* check whether skippable:
									*/
									if (pos + 11 + dataSize + 4 > nPacketLen)
									{
										RTMP_Log(RTMP_LOGWARNING,
											"Non skipable packet since it doesn't end with chunk, stream corrupt!");
										ret = RTMP_READ_ERROR;
										break;
									}
									packetBody += (pos + 11 + dataSize + 4);
									nPacketLen -= (pos + 11 + dataSize + 4);

									goto stopKeyframeSearch;

								}
								else if (r->m_read.nResumeTS < ts)
								{
									/* the timestamp ts will only increase with
									* further packets, wait for seek
									*/
									goto stopKeyframeSearch;
								}
							}
							pos += (11 + dataSize + 4);
						}
						if (ts < r->m_read.nResumeTS)
						{
							RTMP_Log(RTMP_LOGERROR,
								"First packet does not contain keyframe, all "
								"timestamps are smaller than the keyframe "
								"timestamp; probably the resume seek failed?");
						}
stopKeyframeSearch:
						;
						if (!(r->m_read.flags & RTMP_READ_GOTFLVK))
						{
							RTMP_Log(RTMP_LOGERROR,
								"Couldn't find the seeked keyframe in this chunk!");
							ret = RTMP_READ_IGNORE;
							break;
						}
					}
				}
			}

			if (packet.m_nTimeStamp > 0
				&& (r->m_read.flags & (RTMP_READ_GOTKF|RTMP_READ_GOTFLVK)))
			{
				/* another problem is that the server can actually change from
				* 09/08 video/audio packets to an FLV stream or vice versa and
				* our keyframe check will prevent us from going along with the
				* new stream if we resumed.
				*
				* in this case set the 'found keyframe' variables to true.
				* We assume that if we found one keyframe somewhere and were
				* already beyond TS > 0 we have written data to the output
				* which means we can accept all forthcoming data including the
				* change between 08/09 <-> FLV packets
				*/
				r->m_read.flags |= (RTMP_READ_GOTKF|RTMP_READ_GOTFLVK);
			}

			/* skip till we find our keyframe
			* (seeking might put us somewhere before it)
			*/
			if (!(r->m_read.flags & RTMP_READ_GOTKF) &&
				packet.m_packetType != RTMP_PACKET_TYPE_FLASH_VIDEO)
			{
				RTMP_Log(RTMP_LOGWARNING,
					"Stream does not start with requested frame, ignoring data... ");
				r->m_read.nIgnoredFrameCounter++;
				if (r->m_read.nIgnoredFrameCounter > MAX_IGNORED_FRAMES)
					ret = RTMP_READ_ERROR;	/* fatal error, couldn't continue stream */
				else
					ret = RTMP_READ_IGNORE;
				break;
			}
			/* ok, do the same for FLV streams */
			if (!(r->m_read.flags & RTMP_READ_GOTFLVK) &&
				packet.m_packetType == RTMP_PACKET_TYPE_FLASH_VIDEO)
			{
				RTMP_Log(RTMP_LOGWARNING,
					"Stream does not start with requested FLV frame, ignoring data... ");
				r->m_read.nIgnoredFlvFrameCounter++;
				if (r->m_read.nIgnoredFlvFrameCounter > MAX_IGNORED_FRAMES)
					ret = RTMP_READ_ERROR;
				else
					ret = RTMP_READ_IGNORE;
				break;
			}

			/* we have to ignore the 0ms frames since these are the first
			* keyframes; we've got these so don't mess around with multiple
			* copies sent by the server to us! (if the keyframe is found at a
			* later position there is only one copy and it will be ignored by
			* the preceding if clause)
			*/
			if (!(r->m_read.flags & RTMP_READ_NO_IGNORE) &&
				packet.m_packetType != RTMP_PACKET_TYPE_FLASH_VIDEO)
			{
				/* exclude type RTMP_PACKET_TYPE_FLASH_VIDEO since it can
				* contain several FLV packets
				*/
				if (packet.m_nTimeStamp == 0)
				{
					ret = RTMP_READ_IGNORE;
					break;
				}
				else
				{
					/* stop ignoring packets */
					r->m_read.flags |= RTMP_READ_NO_IGNORE;
				}
			}
		}

		/* calculate packet size and allocate slop buffer if necessary */
		size = nPacketLen +
			((packet.m_packetType == RTMP_PACKET_TYPE_AUDIO
			|| packet.m_packetType == RTMP_PACKET_TYPE_VIDEO
			|| packet.m_packetType == RTMP_PACKET_TYPE_INFO) ? 11 : 0) +
			(packet.m_packetType != RTMP_PACKET_TYPE_FLASH_VIDEO ? 4 : 0);

		if (size + 4 > buflen)
		{
			/* the extra 4 is for the case of an FLV stream without a last
			* prevTagSize (we need extra 4 bytes to append it) */
			r->m_read.buf = malloc(size + 4);
			if (r->m_read.buf == 0)
			{
				RTMP_Log(RTMP_LOGERROR, "Couldn't allocate memory!");
				ret = RTMP_READ_ERROR;		/* fatal error */
				break;
			}
			recopy = TRUE;
			ptr = r->m_read.buf;
		}
		else
		{
			ptr = buf;
		}
		pend = ptr + size + 4;

		/* use to return timestamp of last processed packet */

		/* audio (0x08), video (0x09) or metadata (0x12) packets :
		* construct 11 byte header then add rtmp packet's data */
		if (packet.m_packetType == RTMP_PACKET_TYPE_AUDIO
			|| packet.m_packetType == RTMP_PACKET_TYPE_VIDEO
			|| packet.m_packetType == RTMP_PACKET_TYPE_INFO)
		{
			nTimeStamp = r->m_read.nResumeTS + packet.m_nTimeStamp;
			prevTagSize = 11 + nPacketLen;

            //FLV Tag type
			*ptr = packet.m_packetType;
			ptr++;
            //datasize
			ptr = AMF_EncodeInt24(ptr, pend, nPacketLen);

#if 0
			if(packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) {

				/* H264 fix: */
				if((packetBody[0] & 0x0f) == 7) { /* CodecId = H264 */
					uint8_t packetType = *(packetBody+1);

					uint32_t ts = AMF_DecodeInt24(packetBody+2); /* composition time */
					int32_t cts = (ts+0xff800000)^0xff800000;
					RTMP_Log(RTMP_LOGDEBUG, "cts  : %d\n", cts);

					nTimeStamp -= cts;
					/* get rid of the composition time */
					CRTMP::EncodeInt24(packetBody+2, 0);
				}
				RTMP_Log(RTMP_LOGDEBUG, "VIDEO: nTimeStamp: 0x%08X (%d)\n", nTimeStamp, nTimeStamp);
			}
#endif
            //timestamp
			ptr = AMF_EncodeInt24(ptr, pend, nTimeStamp);
            //extern_timestamp
			*ptr = (char)((nTimeStamp & 0xFF000000) >> 24);
			ptr++;

			/* stream id */
			ptr = AMF_EncodeInt24(ptr, pend, 0);
		}

		memcpy(ptr, packetBody, nPacketLen);
		len = nPacketLen;

		/* correct tagSize and obtain timestamp if we have an FLV stream */
		if (packet.m_packetType == RTMP_PACKET_TYPE_FLASH_VIDEO)
		{
			unsigned int pos = 0;
			int delta;

			/* grab first timestamp and see if it needs fixing */
			nTimeStamp = AMF_DecodeInt24(packetBody + 4);
			nTimeStamp |= (packetBody[7] << 24);
			delta = packet.m_nTimeStamp - nTimeStamp + r->m_read.nResumeTS;

			while (pos + 11 < nPacketLen)
			{
				/* size without header (11) and without prevTagSize (4) */
				uint32_t dataSize = AMF_DecodeInt24(packetBody + pos + 1);
				nTimeStamp = AMF_DecodeInt24(packetBody + pos + 4);
				nTimeStamp |= (packetBody[pos + 7] << 24);

				if (delta)
				{
					nTimeStamp += delta;
					AMF_EncodeInt24(ptr+pos+4, pend, nTimeStamp);
					ptr[pos+7] = nTimeStamp>>24;
				}

				/* set data type */
				r->m_read.dataType |= (((*(packetBody + pos) == 0x08) << 2) |
					(*(packetBody + pos) == 0x09));

				if (pos + 11 + dataSize + 4 > nPacketLen)
				{
					if (pos + 11 + dataSize > nPacketLen)
					{
						RTMP_Log(RTMP_LOGERROR,
							"Wrong data size (%u), stream corrupted, aborting!",
							dataSize);
						ret = RTMP_READ_ERROR;
						break;
					}
					RTMP_Log(RTMP_LOGWARNING, "No tagSize found, appending!");

					/* we have to append a last tagSize! */
					prevTagSize = dataSize + 11;
					AMF_EncodeInt32(ptr + pos + 11 + dataSize, pend,
						prevTagSize);
					size += 4;
					len += 4;
				}
				else
				{
					prevTagSize =
						AMF_DecodeInt32(packetBody + pos + 11 + dataSize);

#ifdef _DEBUG
					RTMP_Log(RTMP_LOGDEBUG,
						"FLV Packet: type %02X, dataSize: %lu, tagSize: %lu, timeStamp: %lu ms",
						(unsigned char)packetBody[pos], dataSize, prevTagSize,
						nTimeStamp);
#endif

					if (prevTagSize != (dataSize + 11))
					{
#ifdef _DEBUG
						RTMP_Log(RTMP_LOGWARNING,
							"Tag and data size are not consitent, writing tag size according to dataSize+11: %d",
							dataSize + 11);
#endif

						prevTagSize = dataSize + 11;
						AMF_EncodeInt32(ptr + pos + 11 + dataSize, pend,
							prevTagSize);
					}
				}

				pos += prevTagSize + 4;	/*(11+dataSize+4); */
			}
		}
		ptr += len;

		if (packet.m_packetType != RTMP_PACKET_TYPE_FLASH_VIDEO)
		{
			/* FLV tag packets contain their own prevTagSize */
			AMF_EncodeInt32(ptr, pend, prevTagSize);
		}

		/* In non-live this nTimeStamp can contain an absolute TS.
		* Update ext timestamp with this absolute offset in non-live mode
		* otherwise report the relative one
		*/
		/* RTMP_Log(RTMP_LOGDEBUG, "type: %02X, size: %d, pktTS: %dms, TS: %dms, bLiveStream: %d", packet.m_packetType, nPacketLen, packet.m_nTimeStamp, nTimeStamp, r->Link.lFlags & RTMP_LF_LIVE); */
		r->m_read.timestamp = (r->Link.lFlags & RTMP_LF_LIVE) ? packet.m_nTimeStamp : nTimeStamp;

		ret = size;
		break;
	}

	if (rtnGetNextMediaPacket)
		RTMPPacket_Free(&packet);

	if (recopy)
	{
		len = ret > buflen ? buflen : ret;
		memcpy(buf, r->m_read.buf, len);
		r->m_read.bufpos = r->m_read.buf + len;
		r->m_read.buflen = ret - len;
	}
	return ret;
}

static const char flvHeader[] = { 'F', 'L', 'V', 0x01,
	0x00,				/* 0x04 == audio, 0x01 == video */
	0x00, 0x00, 0x00, 0x09,
	0x00, 0x00, 0x00, 0x00
};

#define HEADERBUF	(128*1024)
int
	RTMP_Read(RTMP *r, char *buf, int size)
{
	int nRead = 0, total = 0;

	/* can't continue */
fail:
	switch (r->m_read.status) {
	case RTMP_READ_EOF:
	case RTMP_READ_COMPLETE:
		return 0;
	case RTMP_READ_ERROR:  /* corrupted stream, resume failed */
		SetSockError(EINVAL);
		return -1;
	default:
		break;
	}

	/* first time thru */
	if (!(r->m_read.flags & RTMP_READ_HEADER))
	{
		if (!(r->m_read.flags & RTMP_READ_RESUME))
		{
			char *mybuf = malloc(HEADERBUF), *end = mybuf + HEADERBUF;
			int cnt = 0;
			r->m_read.buf = mybuf;
			r->m_read.buflen = HEADERBUF;

			memcpy(mybuf, flvHeader, sizeof(flvHeader));
			r->m_read.buf += sizeof(flvHeader);
			r->m_read.buflen -= sizeof(flvHeader);
			cnt += sizeof(flvHeader);

			while (r->m_read.timestamp == 0)
			{
				nRead = Read_1_Packet(r, r->m_read.buf, r->m_read.buflen);
				if (nRead < 0)
				{
					free(mybuf);
					r->m_read.buf = NULL;
					r->m_read.buflen = 0;
					r->m_read.status = nRead;
					goto fail;
				}
				/* buffer overflow, fix buffer and give up */
				if (r->m_read.buf < mybuf || r->m_read.buf > end) {
					mybuf = realloc(mybuf, cnt + nRead);
					memcpy(mybuf+cnt, r->m_read.buf, nRead);
					free(r->m_read.buf);
					r->m_read.buf = mybuf+cnt+nRead;
					break;
				}
				cnt += nRead;
				r->m_read.buf += nRead;
				r->m_read.buflen -= nRead;
				if (r->m_read.dataType == 5)
					break;
			}
			mybuf[4] = r->m_read.dataType;
			r->m_read.buflen = r->m_read.buf - mybuf;
			r->m_read.buf = mybuf;
			r->m_read.bufpos = mybuf;
		}
		r->m_read.flags |= RTMP_READ_HEADER;
	}

	if ((r->m_read.flags & RTMP_READ_SEEKING) && r->m_read.buf)
	{
		/* drop whatever's here */
		free(r->m_read.buf);
		r->m_read.buf = NULL;
		r->m_read.bufpos = NULL;
		r->m_read.buflen = 0;
	}

	/* If there's leftover data buffered, use it up */
	if (r->m_read.buf)
	{
		nRead = r->m_read.buflen;
		if (nRead > size)
			nRead = size;
		memcpy(buf, r->m_read.bufpos, nRead);
		r->m_read.buflen -= nRead;
		if (!r->m_read.buflen)
		{
			free(r->m_read.buf);
			r->m_read.buf = NULL;
			r->m_read.bufpos = NULL;
		}
		else
		{
			r->m_read.bufpos += nRead;
		}
		buf += nRead;
		total += nRead;
		size -= nRead;
	}

	while (size > 0 && (nRead = Read_1_Packet(r, buf, size)) >= 0)
	{
		if (!nRead) continue;
		buf += nRead;
		total += nRead;
		size -= nRead;
		break;
	}
	if (nRead < 0)
		r->m_read.status = nRead;

	if (size < 0)
		total += size;
	return total;
}

static const AVal av_setDataFrame = AVC("@setDataFrame");

int
	RTMP_Write(RTMP *r, const char *buf, int size)
{
	RTMPPacket *pkt = &r->m_write;
	char *pend, *enc;
	int s2 = size, ret, num;

	pkt->m_nChannel = 0x04;	/* source channel */
	pkt->m_nInfoField2 = r->m_stream_id;

	while (s2)
	{
		if (!pkt->m_nBytesRead)
		{
			if (size < 11) {
				/* FLV pkt too small */
				return 0;
			}

			if (buf[0] == 'F' && buf[1] == 'L' && buf[2] == 'V')
			{
				buf += 13;
				s2 -= 13;
			}

			pkt->m_packetType = *buf++;
			pkt->m_nBodySize = AMF_DecodeInt24(buf);
			buf += 3;
			pkt->m_nTimeStamp = AMF_DecodeInt24(buf);
			buf += 3;
			pkt->m_nTimeStamp |= *buf++ << 24;
			buf += 3;
			s2 -= 11;

			if (((pkt->m_packetType == RTMP_PACKET_TYPE_AUDIO
				|| pkt->m_packetType == RTMP_PACKET_TYPE_VIDEO) &&
				!pkt->m_nTimeStamp) || pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
			{
				pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
				if (pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
					pkt->m_nBodySize += 16;
			}
			else
			{
				pkt->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
			}

			if (!RTMPPacket_Alloc(pkt, pkt->m_nBodySize))
			{
				RTMP_Log(RTMP_LOGDEBUG, "%s, failed to allocate packet", __FUNCTION__);
				return FALSE;
			}
			enc = pkt->m_body;
			pend = enc + pkt->m_nBodySize;
			if (pkt->m_packetType == RTMP_PACKET_TYPE_INFO)
			{
				enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
				pkt->m_nBytesRead = enc - pkt->m_body;
			}
		}
		else
		{
			enc = pkt->m_body + pkt->m_nBytesRead;
		}
		num = pkt->m_nBodySize - pkt->m_nBytesRead;
		if (num > s2)
			num = s2;
		memcpy(enc, buf, num);
		pkt->m_nBytesRead += num;
		s2 -= num;
		buf += num;
		if (pkt->m_nBytesRead == pkt->m_nBodySize)
		{
			ret = RTMP_SendPacket(r, pkt, FALSE);
			RTMPPacket_Free(pkt);
			pkt->m_nBytesRead = 0;
			if (!ret)
				return -1;
			buf += 4;
			s2 -= 4;
			if (s2 < 0)
				break;
		}
	}
	return size+s2;
}
