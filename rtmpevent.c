#include "rtmpevent.h"


#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <time.h>
#include <stdarg.h>

#include <sys/ipc.h>
#include <sys/msg.h>


#include "thread.h"

#ifdef linux
#include <linux/netfilter_ipv4.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#endif

#define RD_SUCCESS		0
#define RD_FAILED		1
#define RD_INCOMPLETE		2

#define PACKET_SIZE 1024*1024

#ifdef WIN32
#define InitSockets()	{\
	WORD version;			\
	WSADATA wsaData;		\
	\
	version = MAKEWORD(1,1);	\
	WSAStartup(version, &wsaData);	}

#define	CleanupSockets()	WSACleanup()
#else
#define InitSockets()
#define	CleanupSockets()
#endif

#define DUPTIME	5000	/* interval we disallow duplicate requests, in msec */

enum
{
	STREAMING_ACCEPTING,
	STREAMING_IN_PROGRESS,
	STREAMING_STOPPING,
	STREAMING_STOPPED
};

static const AVal av_dquote = AVC("\"");
static const AVal av_escdquote = AVC("\\\"");

#define STR2AVAL(av,str)	av.av_val = str; av.av_len = strlen(av.av_val)
	
	/* this request is formed from the parameters and used to initialize a new request,
	* thus it is a default settings list. All settings can be overriden by specifying the
	* parameters in the GET request. */
	//RTMP_REQUEST defaultRTMPRequest;
	
#ifdef _DEBUG
	uint32_t debugTS = 0;
	
	int pnum = 0;
	
	FILE *netstackdump = NULL;
	FILE *netstackdump_read = NULL;
#endif


struct APP *myapp = NULL;

#define SAVC(x) static const AVal av_##x = AVC(#x)

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
SAVC(_result);
SAVC(createStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(publish);//
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);

SAVC(releaseStream);
SAVC(FCPublish);


SAVC(onMetaData);
SAVC(duration);
SAVC(video);
SAVC(audio);

SAVC(onStatus);
SAVC(status);

SAVC(error);

static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");
static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_Started_publishing = AVC("Started publishing");

static const AVal av_NetStream_Publish_BadName = AVC("NetStream.Publish.BadName");
static const AVal av_Already_Publishing = AVC("Already publishing");

SAVC(details);
SAVC(clientid);
static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");

static int
	SendConnectResult(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_OBJECT;

	STR2AVAL(av, "FMS/3,5,1,525");
	enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	*enc++ = AMF_OBJECT;

	STR2AVAL(av, "status");
	enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
	STR2AVAL(av, "NetConnection.Connect.Success");
	enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
	STR2AVAL(av, "Connection succeeded.");
	enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
#if 0
	STR2AVAL(av, "58656322c972d6cdf2d776167575045f8484ea888e31c086f7b5ffbd0baec55ce442c2fb");
	enc = AMF_EncodeNamedString(enc, pend, &av_secureToken, &av);
#endif
	STR2AVAL(p.p_name, "version");
	STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
	p.p_type = AMF_STRING;
	obj.o_num = 1;
	obj.o_props = &p;
	op.p_type = AMF_OBJECT;
	STR2AVAL(op.p_name, "data");
	op.p_vu.p_object = obj;
	enc = AMFProp_Encode(&op, enc, pend);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

static int
	SendResultNumber(RTMP *r, double txn, double ID)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeNumber(enc, pend, ID);

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}

static void AVreplace(AVal *src, const AVal *orig, const AVal *repl)
{
	char *srcbeg = src->av_val;
	char *srcend = src->av_val + src->av_len;
	char *dest, *sptr, *dptr;
	int n = 0;

	/* count occurrences of orig in src */
	sptr = src->av_val;
	while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
	{
		n++;
		sptr += orig->av_len;
	}
	if (!n)
		return;

	dest = malloc(src->av_len + 1 + (repl->av_len - orig->av_len) * n);

	sptr = src->av_val;
	dptr = dest;
	while (sptr < srcend && (sptr = strstr(sptr, orig->av_val)))
	{
		n = sptr - srcbeg;
		memcpy(dptr, srcbeg, n);
		dptr += n;
		memcpy(dptr, repl->av_val, repl->av_len);
		dptr += repl->av_len;
		sptr += orig->av_len;
		srcbeg = sptr;
	}
	n = srcend - srcbeg;
	memcpy(dptr, srcbeg, n);
	dptr += n;
	*dptr = '\0';
	src->av_val = dest;
	src->av_len = dptr - dest;
}


static int
	countAMF(AMFObject *obj, int *argc)
{
	int i, len;

	for (i=0, len=0; i < obj->o_num; i++)
	{
		AMFObjectProperty *p = &obj->o_props[i];
		len += 4;
		(*argc)+= 2;
		if (p->p_name.av_val)
			len += 1;
		len += 2;
		if (p->p_name.av_val)
			len += p->p_name.av_len + 1;
		switch(p->p_type)
		{
		case AMF_BOOLEAN:
			len += 1;
			break;
		case AMF_STRING:
			len += p->p_vu.p_aval.av_len;
			break;
		case AMF_NUMBER:
			len += 40;
			break;
		case AMF_OBJECT:
			len += 9;
			len += countAMF(&p->p_vu.p_object, argc);
			(*argc) += 2;
			break;
		case AMF_NULL:
		default:
			break;
		}
	}
	return len;
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


static int
	HandleMetadata(RTMP *r, char *body, unsigned int len, char *pout, int *size)
{
	/* allright we get some info here, so parse it and print it */
	/* also keep duration or filesize to make a nice progress bar */

	AMFObject obj, obj1;
	AVal metastring;
	int ret = FALSE;

	int nRes = AMF_Decode(&obj, body, len, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding meta data packet", __FUNCTION__);
		return FALSE;
	}

	*size = AMF_Encode1(&obj, pout, pout+len, 1);

	printf("===============size=%d=================\n", *size);

	nRes = AMF_Decode(&obj1, pout, *size, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding meta data packet", __FUNCTION__);
		//return FALSE;
	}

	AMF_Dump(&obj);
	//AMF_Dump(&obj1);
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

static int SendChunkSize(RTMP *r){
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x02;  
	packet.m_headerType = 1; 
	packet.m_packetType = 0x01;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeInt32(enc, pend, r->m_outChunkSize);
	
	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}

static int
SendPlayStart(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf+sizeof(pbuf);

  packet.m_nChannel = 0x03;     // control channel (invoke)
  packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = 0x14;   // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}


static int
SendOnStatus(RTMP *r, AVal status, AVal code, AVal description)
{
	RTMPPacket packet;
	char pbuf[512], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 1; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_onStatus);
	enc = AMF_EncodeNumber(enc, pend, 0);
	enc = AMF_EncodeNull(enc, pend);

	enc = AMF_EncodeObjectStart(enc, pend);
	enc = AMF_EncodeNamedString(enc, pend, &av_level, &status);
	enc = AMF_EncodeNamedString(enc, pend, &av_code, &code);
	enc = AMF_EncodeNamedString(enc, pend, &av_description, &description);
	enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
	enc = AMF_EncodeObjectEnd(enc, pend);

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}


RtmpEvent* create_rtmp_event_node(RTMP *rtmp)
{
	RtmpEvent *rtmpevent = malloc(sizeof(RtmpEvent));
	memset(rtmpevent, 0, sizeof(RtmpEvent));

	INIT_LIST_HEAD(&rtmpevent->node);
	INIT_LIST_HEAD(&rtmpevent->list);

	rtmpevent->rtmp = rtmp;
	rtmpevent->handshake = 0;
	rtmpevent->establish = 0;
	rtmpevent->nplay = 0;
	rtmpevent->sendMetadata = 0;
	rtmpevent->sendaacsequence = 0;

	rtmpevent->Metadata = NULL;
	rtmpevent->sps_pps = NULL;
	rtmpevent->idr = NULL;
	rtmpevent->aac_sequence_header = NULL;
	
	return rtmpevent;
}

int delete_rtmp_event_node(RtmpEvent *rtmpevent)
{
	if(rtmpevent){
		struct list_head *pos, *npos;
		RtmpEvent *node;
		list_for_each_safe(pos, npos, &rtmpevent->list){
			node = list_entry(pos, struct RtmpEvent, node);
			list_del(pos);
			delete_rtmp_event_node(node);
		}

		if(rtmpevent->rtmp){
			RTMP_Close(rtmpevent->rtmp);
			RTMP_Free(rtmpevent->rtmp);
			rtmpevent->rtmp = NULL;
		}

		free(rtmpevent);
		rtmpevent = NULL;
	}
	return 0;
}

void add_play_to_publish(RtmpEvent *publish, RtmpEvent *play)
{
	list_add_tail(&(play->node), &(publish->list));
	publish->nplay++;
	return;
}


void delete_play_from_publish(RtmpEvent *publish, RtmpEvent *play)
{
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &publish->list){
		node = list_entry(pos, struct RtmpEvent, node);

		if(play == node){
			list_del(pos);
			delete_rtmp_event_node(node);
			publish->nplay--;
			break;
		}
	}
	return;
}


APP* create_app_node()
{
	APP *app = malloc(sizeof(APP));
	memset(app, 0, sizeof(APP));

	INIT_LIST_HEAD(&app->node);
	INIT_LIST_HEAD(&app->list);

	return app;
}

int delete_app_node(APP *app)
{
	if(app){
		struct list_head *pos, *npos;
		RtmpEvent *node;
		list_for_each_safe(pos, npos, &app->list){
			node = list_entry(pos, struct RtmpEvent, node);
			list_del(pos);
			delete_rtmp_event_node(node);
		}

		free(app);
		app = NULL;
	}
	return 0;
}

void add_publish_to_app(APP *app, RtmpEvent *publish)
{
	list_add_tail(&(publish->node), &(app->list));
	app->npubulish++;
	return;
}

void delete_publish_from_app(APP *app, RtmpEvent *publish)
{
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &app->list){
		node = list_entry(pos, struct RtmpEvent, node);
		if(node == publish){
			list_del(pos);
			app->npubulish--;
			break;
		}
	}
	return;
}

RtmpEvent* find_rtmp_event_in_app_bysocket(APP *app, int fd)
{
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &app->list){
		node = list_entry(pos, struct RtmpEvent, node);
		if(node->rtmp->m_sb.sb_socket == fd){
			return node;
		}
	}

	return NULL;
}

RtmpEvent* find_rtmp_event_in_app_byrtmp(APP *app, RTMP *rtmp)
{
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &app->list){
		node = list_entry(pos, struct RtmpEvent, node);
		if(node->rtmp == rtmp){
			return node;
		}
	}

	return NULL;
}

RtmpEvent* find_rtmp_event_in_app_byplaypath(APP *app, char *p, int len)
{
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &app->list){
		node = list_entry(pos, struct RtmpEvent, node);

		printf("====%s=========%s====\n", node->playpath, p);
		if(!memcmp(node->playpath, p, len)){
		//if(!strcmp(node->playpath, p)){
			return node;
		}
	}

	return NULL;
}

void distribute_to_players(APP *app, RTMP *rtmp, RTMPPacket *packet)
{
	RtmpEvent *publish = find_rtmp_event_in_app_byrtmp(app, rtmp);

	char *ptr; char flv[14] = {0};

	RTMPPacket pkt = {0};
	
	struct list_head *pos, *npos;
	RtmpEvent *node;
	list_for_each_safe(pos, npos, &publish->list){
		node = list_entry(pos, struct RtmpEvent, node);
		
		if(!node->sendMetadata){
			node->sendMetadata = 1;

			memset(&pkt, 0, sizeof(RTMPPacket));
			RTMPPacket_Alloc(&pkt, publish->Metadata->m_nBodySize);
			RTMPPacket_Copy1(&pkt, publish->Metadata);
			RTMP_SendPacket(node->rtmp, &pkt, FALSE);			
			RTMPPacket_Free(&pkt);
			
			memset(&pkt, 0, sizeof(RTMPPacket));
			RTMPPacket_Alloc(&pkt, publish->sps_pps->m_nBodySize);
			RTMPPacket_Copy1(&pkt, publish->sps_pps);
			RTMP_SendPacket(node->rtmp, &pkt, FALSE);
			//RTMP_LogHexString(RTMP_LOGERROR, pkt.m_body, pkt.m_nBodySize);
			RTMPPacket_Free(&pkt);

			//printf("====================\n");
			
			memset(&pkt, 0, sizeof(RTMPPacket));
			RTMPPacket_Alloc(&pkt, publish->idr->m_nBodySize);
			RTMPPacket_Copy1(&pkt, publish->idr);
			RTMP_SendPacket(node->rtmp, &pkt, FALSE);
			//RTMP_LogHexString(RTMP_LOGERROR, pkt.m_body, pkt.m_nBodySize);
			RTMPPacket_Free(&pkt);
		}	

		if(!node->sendaacsequence && packet->m_packetType == RTMP_PACKET_TYPE_AUDIO){
			node->sendaacsequence = 1;

			memset(&pkt, 0, sizeof(RTMPPacket));
			RTMPPacket_Alloc(&pkt, publish->aac_sequence_header->m_nBodySize);
			RTMPPacket_Copy1(&pkt, publish->aac_sequence_header);
			RTMP_SendPacket(node->rtmp, &pkt, FALSE);
			RTMPPacket_Free(&pkt);
		}

		memset(&pkt, 0, sizeof(RTMPPacket));
		RTMPPacket_Alloc(&pkt, packet->m_nBodySize);
		RTMPPacket_Copy1(&pkt, packet);
		RTMP_SendPacket(node->rtmp, &pkt, FALSE);
		RTMPPacket_Free(&pkt);
	}
	return;
}



static int ServeInvoke(APP *server, RTMP * r, RTMPPacket *packet, unsigned int offset)
{
	const char *body;
	unsigned int nBodySize;
	int ret = 0, nRes;

	body = packet->m_body + offset;
	nBodySize = packet->m_nBodySize - offset;

	if (body[0] != 0x02)		// make sure it is a string method name we start with
	{
		RTMP_Log(RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
			__FUNCTION__);
		return 0;
	}

	AMFObject obj;
	nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
	if (nRes < 0)
	{
		RTMP_Log(RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
		return 0;
	}

	AMF_Dump(&obj);
	AVal method;
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
	double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
	RTMP_Log(RTMP_LOGDEBUG, "%s, client invoking <%s>, txn = %lf", __FUNCTION__, method.av_val, txn);

	if (AVMATCH(&method, &av_connect))
	{
		AMFObject cobj;
		AVal pname, pval;
		int i;

		printf("connect-------------\n\n");

		server->connect = packet->m_body;
		packet->m_body = NULL;

		AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &cobj);
		for (i=0; i<cobj.o_num; i++)
		{
			pname = cobj.o_props[i].p_name;
			pval.av_val = NULL;
			pval.av_len = 0;
			if (cobj.o_props[i].p_type == AMF_STRING)
				pval = cobj.o_props[i].p_vu.p_aval;
			if (AVMATCH(&pname, &av_app))
			{
				r->Link.app = pval;
				pval.av_val = NULL;
				if (!r->Link.app.av_val)
					r->Link.app.av_val = "";
				server->arglen += 6 + pval.av_len;
				server->argc += 2;
			}
			else if (AVMATCH(&pname, &av_flashVer))
			{
				r->Link.flashVer = pval;
				pval.av_val = NULL;
				server->arglen += 6 + pval.av_len;
				server->argc += 2;
			}
			else if (AVMATCH(&pname, &av_swfUrl))
			{
				r->Link.swfUrl = pval;
				pval.av_val = NULL;
				server->arglen += 6 + pval.av_len;
				server->argc += 2;
			}
			else if (AVMATCH(&pname, &av_tcUrl))
			{
				r->Link.tcUrl = pval;
				pval.av_val = NULL;
				server->arglen += 6 + pval.av_len;
				server->argc += 2;
			}
			else if (AVMATCH(&pname, &av_pageUrl))
			{
				r->Link.pageUrl = pval;
				pval.av_val = NULL;
				server->arglen += 6 + pval.av_len;
				server->argc += 2;
			}
			else if (AVMATCH(&pname, &av_audioCodecs))
			{
				r->m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
			}
			else if (AVMATCH(&pname, &av_videoCodecs))
			{
				r->m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
			}
			else if (AVMATCH(&pname, &av_objectEncoding))
			{
				r->m_fEncoding = cobj.o_props[i].p_vu.p_number;
			}
		}
		/* Still have more parameters? Copy them */
		if (obj.o_num > 3)
		{
			int i = obj.o_num - 3;
			r->Link.extras.o_num = i;
			r->Link.extras.o_props = malloc(i*sizeof(AMFObjectProperty));
			memcpy(r->Link.extras.o_props, obj.o_props+3, i*sizeof(AMFObjectProperty));
			obj.o_num = 3;
			server->arglen += countAMF(&r->Link.extras, &server->argc);
		}
		SendConnectResult(r, txn);

	}
	else if (AVMATCH(&method, &av_releaseStream))
	{
		printf("releaseStream-------------\n\n");
		SendResultNumber(r, txn, ++server->streamID);

		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &r->Link.playpath);
		if (!r->Link.playpath.av_len)
			return 0;

		//SendOnStatus(r, av_error, av_NetStream_Publish_BadName, av_Already_Publishing);
	}
	else if (AVMATCH(&method, &av_FCPublish))
	{
		printf("FCPublish-------------\n\n");
		SendResultNumber(r, txn, ++server->streamID);
	}
	else if (AVMATCH(&method, &av_createStream))
	{
		printf("createStream-------------\n\n");
		SendResultNumber(r, txn, ++server->streamID);
	}
	else if (AVMATCH(&method, &av_getStreamLength))
	{
		printf("getStreamLength-------------\n\n");
		SendResultNumber(r, txn, 10.0);
	}
	else if (AVMATCH(&method, &av_NetStream_Authenticate_UsherToken))
	{
		printf("NetStream.Authenticate.UsherToken-------------\n\n");
		AVal usherToken;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &usherToken);
		AVreplace(&usherToken, &av_dquote, &av_escdquote);
		server->arglen += 6 + usherToken.av_len;
		server->argc += 2;
		r->Link.usherToken = usherToken;
	}
	else if (AVMATCH(&method, &av_play))
	{
		printf("play-------------\n\n");
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &r->Link.playpath);
		if (!r->Link.playpath.av_len)
			//printf("-----3------%s-------\n", r->Link.playpath);
			return 0;

//#if 0
		RtmpEvent *play = find_rtmp_event_in_app_byrtmp(myapp, r);
		if(!play)
			printf("--------------play is null-------------\n");

		delete_publish_from_app(myapp, play);


		play->establish = 1;

		RtmpEvent *publish = find_rtmp_event_in_app_byplaypath(myapp, r->Link.playpath.av_val, r->Link.playpath.av_len);
		if(!publish)
			printf("--------------publish is null-------------\n");
		
		add_play_to_publish(publish, play);
//#endif

		SendChunkSize(r);
		RTMP_SendCtrl(r, 0, 1, 0);
		SendOnStatus(r, av_status, av_NetStream_Play_Start, av_Started_playing);

		//SendPlayStart(r);
	}
	else if (AVMATCH(&method, &av_publish)){
		printf("publish-------------\n\n");
		AVal val;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &r->Link.playpath);
		if (!r->Link.playpath.av_len)
			return 0;
		
		RtmpEvent* publish = find_rtmp_event_in_app_byrtmp(myapp, r);
		sprintf(publish->playpath, "%s", r->Link.playpath.av_val);
		publish->establish = 1;

		SendOnStatus(r, av_status, av_NetStream_Publish_Start, av_Started_publishing);
	}

	AMF_Reset(&obj);
	return ret;
}


int sps_pps_predicte(RTMPPacket *packet)
{
	if(packet->m_body[0] == 0x17 && packet->m_body[1] == 0x00)
		return 1;
	return 0;
}

int key_frame_predicte(RTMPPacket *packet)
{
	if(sps_pps_predicte(packet) == 0 && ((packet->m_body[0]>>4)&0x0F) == 1)
		return 1;
	return 0;
}


static int ServePacket(APP *server, RTMP *r, RTMPPacket *packet)
{
	int ret = 0;
	RTMPPacket pkt;
	RtmpEvent* publish = NULL;
	static FILE *pf=NULL;
	char *pbuf;
	
	char *ptr;

	//RTMP_Log(RTMP_LOGDEBUG, "%s, received packet type %02X, size %u bytes", __FUNCTION__,
		//packet->m_packetType, packet->m_nBodySize);

		//printf("===>> received packet type %02X \n", packet->m_packetType);

		//RTMP_LogHexString(RTMP_LOGERROR, (uint8_t *)packet->m_body, packet->m_nBodySize);

	switch (packet->m_packetType)
	{
	case RTMP_PACKET_TYPE_CHUNK_SIZE:
		HandleChangeChunkSize(r, packet);
		break;

	case RTMP_PACKET_TYPE_BYTES_READ_REPORT:
		break;

	case RTMP_PACKET_TYPE_CONTROL:
		HandleCtrl(r, packet);
		break;

	case RTMP_PACKET_TYPE_SERVER_BW:
		HandleServerBW(r, packet);
		break;

	case RTMP_PACKET_TYPE_CLIENT_BW:
		HandleClientBW(r, packet);
		break;

	case RTMP_PACKET_TYPE_AUDIO:

		publish = find_rtmp_event_in_app_byrtmp(server, r);

		if(!publish->aac_sequence_header){
			if(!publish->aac_sequence_header){
				publish->aac_sequence_header = malloc(sizeof(RTMPPacket));
				memset(publish->aac_sequence_header, 0, sizeof(RTMPPacket));
			}
			RTMPPacket_Free(publish->aac_sequence_header);
			RTMPPacket_Alloc(publish->aac_sequence_header, packet->m_nBodySize);
			RTMPPacket_Copy1(publish->aac_sequence_header, packet);

			RTMP_LogHexString(RTMP_LOGERROR, (uint8_t *)packet->m_body, packet->m_nBodySize);

			Parse_AacConfigration(&publish->aacc, packet);

			break;
		}

		//Ts_Packet_AV(packet, publish->avcc.sequenceParameterSetNALUnit, 
			//publish->avcc.sequenceParameterSetLength, 
			//publish->avcc.pictureParameterSetNALUnit, 
			//publish->avcc.pictureParameterSetLength, &publish->audiocount, &publish->aacc);

		//distribute_to_players(myapp, r, packet);

		rtmp_mpegts_write_frame(packet, &publish->audiocount, &publish->avcc, &publish->aacc);

		break;

	case RTMP_PACKET_TYPE_VIDEO:
		//RTMP_Log(RTMP_LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet.m_nBodySize);

		//printf("------------1-----------\n");
		//RTMP_LogHexString(RTMP_LOGERROR, packet->m_body, 5);
		//printf("------------2-----------\n");

		publish = find_rtmp_event_in_app_byrtmp(server, r);

		if(sps_pps_predicte(packet) == 1){
			if(!publish->sps_pps){
				publish->sps_pps = malloc(sizeof(RTMPPacket));
				memset(publish->sps_pps, 0, sizeof(RTMPPacket));
			}
			RTMPPacket_Free(publish->sps_pps);
			RTMPPacket_Alloc(publish->sps_pps, packet->m_nBodySize);
			RTMPPacket_Copy1(publish->sps_pps, packet);

			//RTMP_LogHexString(RTMP_LOGERROR, packet->m_body, packet->m_nBodySize);
			memset(&publish->avcc, 0, sizeof(Tag_Video_AvcC));

			Parse_AvcConfigration(&publish->avcc, packet);

			//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)publish->sps_pps->m_body, publish->sps_pps->m_nBodySize);
			break;
		}

		if(key_frame_predicte(packet) == 1){
			if(!publish->idr){
				publish->idr = malloc(sizeof(RTMPPacket));
				memset(publish->idr, 0, sizeof(RTMPPacket));
			}
			RTMPPacket_Free(publish->idr);
			RTMPPacket_Alloc(publish->idr, packet->m_nBodySize);
			RTMPPacket_Copy1(publish->idr, packet);

			//RTMP_LogHexString(RTMP_LOGDEBUG2, (uint8_t *)publish->idr->m_body, publish->idr->m_nBodySize);
		}

		//Ts_Packet_AV(packet, publish->avcc.sequenceParameterSetNALUnit, 
			//publish->avcc.sequenceParameterSetLength, 
			//publish->avcc.pictureParameterSetNALUnit, 
			//publish->avcc.pictureParameterSetLength, &publish->videocount, NULL);

		rtmp_mpegts_write_frame(packet, &publish->videocount, &publish->avcc, &publish->aacc);
		
		//distribute_to_players(myapp, r, packet);

		break;

	case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
		break;

	case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
		break;

	case RTMP_PACKET_TYPE_FLEX_MESSAGE:
		{
			RTMP_Log(RTMP_LOGDEBUG, "%s, flex message, size %u bytes, not fully supported",
				__FUNCTION__, packet->m_nBodySize);
			
			if (ServeInvoke(server, r, packet, 1))
				RTMP_Close(r);
			break;
		}
	case RTMP_PACKET_TYPE_INFO:
		publish = find_rtmp_event_in_app_byrtmp(server, r);

		if(publish->Metadata){
			RTMPPacket_Free(publish->Metadata);
		}

		publish->Metadata = malloc(sizeof(RTMPPacket));
		RTMPPacket_Alloc(publish->Metadata, packet->m_nBodySize);
		RTMPPacket_Copy1(publish->Metadata, packet);

		//RTMP_LogHexString(RTMP_LOGERROR, packet->m_body, packet->m_nBodySize);

		break;

	case RTMP_PACKET_TYPE_SHARED_OBJECT:
		break;

	case RTMP_PACKET_TYPE_INVOKE:
		RTMP_Log(RTMP_LOGDEBUG, "%s, received: invoke %u bytes", __FUNCTION__,
			packet->m_nBodySize);
		//RTMP_LogHex(packet.m_body, packet.m_nBodySize);

		if (ServeInvoke(server, r, packet, 0))
			RTMP_Close(r);
		break;

	case RTMP_PACKET_TYPE_FLASH_VIDEO:
		printf("-------default------0x%02x---------------\n", packet->m_packetType);
		distribute_to_players(myapp, r, packet);
		break;
	default:
		printf("-------default------0x%02x---------------\n", packet->m_packetType);
		RTMP_Log(RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
			packet->m_packetType);
#ifdef _DEBUG
		RTMP_LogHex(RTMP_LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
	}
	return ret;
}



int main(int argc, char **argv){

	int s_fd, c_fd, ret, on, max_fd, i;
	struct sockaddr_in s_addr;
	struct sockaddr_in c_addr;
	struct timeval tv;
	fd_set fdset;
	int c_fds[CONCURRENT_MAX] = {0};
	socklen_t addr_len = -1;

	RTMP_debuglevel = RTMP_LOGINFO;

	  if (argc > 1 && !strcmp(argv[1], "-z"))
	    RTMP_debuglevel = RTMP_LOGALL;

	myapp = create_app_node();
	
	if((s_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("loop socket error, errno=%s\n", strerror(errno));
		return -1;
    }

	on = 1;
	if ((ret = setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))) < 0){
		printf("loop setsockopt(SO_REUSEADDR) failed=%d, errno=%s\n", ret, strerror(errno));
		return -1;
	}

	on = 1;
	if ((ret = setsockopt(s_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int))) < 0){
		printf("loop setsockopt(SO_REUSEPORT) failed=%d, errno=%s\n", ret, strerror(errno));
		return -1;
	}

	memset(&s_addr,0,sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s_addr.sin_port = htons(1935);

	if((ret = bind(s_fd, (struct sockaddr *)&s_addr, sizeof(s_addr))) < 0){
		printf("loop bind failed=%d, errno=%s\n", ret, strerror(errno));
        return -1;
    }

	on = 1;
	if ((ret = setsockopt(s_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int))) < 0){
		printf("loop setsockopt(TCP_NODELAY) failed=%d, errno=%s\n", ret, strerror(errno));
		return -1;
	}
	
	if((ret = fcntl(s_fd, F_SETFL, fcntl(s_fd, F_GETFL) | O_NONBLOCK)) < 0){
		printf("loop fcntl(O_NONBLOCK) failed=%d, errno=%s\n", ret, strerror(errno));
        return -1;
	}

	if((ret = fcntl(s_fd, F_SETFD, fcntl(s_fd, F_GETFL) | FD_CLOEXEC)) < 0) {
		printf("loop fcntl(FD_CLOEXEC) failed=%d, errno=%s\n", ret, strerror(errno));
        return -1;
	}

    if((ret = listen(s_fd, 10)) < 0){
		printf("loop listen failed=%d, errno=%s\n", ret, strerror(errno));
        return -1;
    }


	for(;;){

		max_fd = 0;
        tv.tv_sec = 20;
        tv.tv_usec = 0;
		
        FD_ZERO(&fdset);
        FD_SET(s_fd, &fdset);
        if(max_fd < s_fd){  
            max_fd = s_fd;
        }

        for(i =0; i < CONCURRENT_MAX; i++){
            if(c_fds[i] != 0){
                FD_SET(c_fds[i], &fdset);
                if(max_fd < c_fds[i]){
                    max_fd = c_fds[i];
                }
            }
        }
		
        ret = select(max_fd + 1, &fdset, NULL, NULL, &tv);
        if(ret < 0){  
			printf("select failed=%d, errno=%s\n", ret, strerror(errno));
			break;
            continue;
        } else if(ret == 0) {  
            //printf("-----select time out \n");  
            continue;  
        } else {
        	int cindex = -1;
			RTMPPacket packet = { 0 };
            if(FD_ISSET(s_fd, &fdset)){
            	//new client
            	memset(&c_addr,0,sizeof(c_addr));
				addr_len = sizeof(c_addr);
                c_fd = accept(s_fd, (struct sockaddr *)&c_addr, &addr_len);  
				printf("loop new connection client fd = %d\n", c_fd);
                if(c_fd > 0){
                    for(i = 0; i < CONCURRENT_MAX; i++){
                        if(c_fds[i] == 0){  
                            cindex = i;  
                            c_fds[i] = c_fd;  
                            break;  
                        }  
                    }

                    if(cindex >= 0){

						RTMP *rtmp = RTMP_Alloc();
						RTMP_Init(rtmp);
						rtmp->m_sb.sb_socket = c_fd;

						RtmpEvent* rtmpev = create_rtmp_event_node(rtmp);
						if(!rtmpev)
							continue;
						add_publish_to_app(myapp, rtmpev);

						if(!rtmpev->handshake){
							if(!RTMP_Serve(rtmpev->rtmp)){
								printf("[client] (%d) handshake failed\n", i);
								continue;
							}
							rtmpev->handshake = 1;
							//continue;
						}

						memset(&packet, 0, sizeof(RTMPPacket));
						while(RTMP_ReadPacket(rtmpev->rtmp, &packet)){
							if (!RTMPPacket_IsReady(&packet)){
								continue;
							}
							
							ServePacket(myapp, rtmpev->rtmp, &packet);
							RTMPPacket_Free(&packet);

							if(rtmpev->establish)
								break;
						}

						
                        printf("new connection client[%d] %s:%d\n",
							cindex, inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));
						//continue;
                    }else{
                        printf("client add failed %s:%d\n", 
							inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));  
                    }
                }  
            }  
            for(i =0; i < CONCURRENT_MAX; i++){
                if(c_fds[i] != 0){
                    if(FD_ISSET(c_fds[i], &fdset)){
						//handle client msg
						
						//printf("client msg fd = %d\n", c_fds[i]);
						

						RtmpEvent* rtmpev = find_rtmp_event_in_app_bysocket(myapp, c_fds[i]);
						if(!rtmpev)
							continue;
						if(!rtmpev->handshake){
							if(!RTMP_Serve(rtmpev->rtmp)){
								printf("[client] (%d) handshake failed\n", i);
								continue;
							}
							rtmpev->handshake = 1;
							//continue;
						}
						
						memset(&packet, 0, sizeof(RTMPPacket));
						while(RTMP_ReadPacket(rtmpev->rtmp, &packet)){
							if (!RTMPPacket_IsReady(&packet)){
								continue;
							}
							
							ServePacket(myapp, rtmpev->rtmp, &packet);
							RTMPPacket_Free(&packet);

							if(rtmpev->establish)
								break;
						}
	                }  
	            }  
	        }  
	    }  
	}

	delete_app_node(myapp);
	
	return 0;
}

