#ifndef _RTMP_EVENT_H_
#define _RTMP_EVENT_H_

#include "list.h"

#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"


#define PATH_MAX 128
#define CONCURRENT_MAX 10

typedef struct RtmpEvent{
	struct list_head	node;
	struct list_head	list;

	RTMPPacket *Metadata;
	RTMPPacket *sps_pps;
	RTMPPacket *idr;
	RTMPPacket *aac_sequence_header;
	char playpath[PATH_MAX];
	int 	handshake;
	int 	establish;
	RTMP *rtmp;

	int sendMetadata;
	int sendaacsequence;
	int nplay;

}RtmpEvent;

typedef struct APP{
	struct list_head 	node;
	struct list_head 	list;

	AVal app;
	
	int npubulish;

	int socket;
	int state;
	int streamID;
	int arglen;
	int argc;
	uint32_t filetime;	/* time of last download we started */
	AVal filename;	/* name of last download */
	char *connect;
}APP;

RtmpEvent* create_rtmp_event_node(RTMP *rtmp);
int delete_rtmp_event_node(RtmpEvent *rtmpevent);
void add_play_to_publish(RtmpEvent *publish, RtmpEvent *play);
void delete_play_from_publish(RtmpEvent *publish, RtmpEvent *play);

APP* create_app_node();
int delete_app_node(APP *app);
void add_publish_to_app(APP *app, RtmpEvent *publish);
void delete_publish_from_app(APP *app, RtmpEvent *publish);

RtmpEvent* find_rtmp_event_in_app_bysocket(APP *app, int fd);
RtmpEvent* find_rtmp_event_in_app_byrtmp(APP *app, RTMP *rtmp);
RtmpEvent* find_rtmp_event_in_app_byplaypath(APP *app, char *p, int len);


void distribute_to_players(APP *app, RTMP *rtmp, RTMPPacket *packet);

int sps_pps_predicte(RTMPPacket *packet);
int key_frame_predicte(RTMPPacket *packet);

#endif
