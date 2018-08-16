#ifndef __RTSP__H__
#define __RTSP__H__

/*
    /////////////////////////////////////////// publish ///////////////////////////////////////////////
    

            Client                                                      Server
    ---------------------------------------------------------------------------------
  1.OPTIONS rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    CSeq : 1\r\n
    User-Agent : Lavf57.71.100\r\n
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 1\r\n
                                                                Public : DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  2.ANNOUNCE rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    Content-type : application/sdp
    CSeq : 2\r\n
    User-Agent : Lavf57.71.100\r\n
    Content-length : 480
    \r\n
    Session Descriprion Protocol()
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 2\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  3.SETUP rtsp://127.0.0.1:554/tt.sdp/streamid=0 RTSP/1.0\r\n
    Transport : RTP/AVP/UDP;unicast;client_port=17952-17953;mode=record
    CSeq : 3\r\n
    User-Agent : Lavf57.71.100\r\n
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 3\r\n
                                                                Cache-Control : no-cache\r\n
                                                                Session : 9184120365130522022
                                                                Date : Thu, 16 Aug 2018 02:00:01 GMT\r\n
                                                                Expires : Thu, 16 Aug 2018 02:00:01 GMT\r\n
                                                                Transport : RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=17952-17953;server-port=6972-6973
                                                                \r\n
    ---------------------------------------------------------------------------------
  4.SETUP rtsp://127.0.0.1:554/tt.sdp/streamid=1 RTSP/1.0\r\n
    Transport : RTP/AVP/UDP;unicast;client_port=17954-17955;mode=record
    CSeq : 4\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 9184120365130522022
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 4\r\n
                                                                Session : 9184120365130522022
                                                                Cache-Control : no-cache\r\n
                                                                Date : Thu, 16 Aug 2018 02:00:01 GMT\r\n
                                                                Expires : Thu, 16 Aug 2018 02:00:01 GMT\r\n
                                                                Transport : RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=17954-17955;server-port=6974-6975
                                                                \r\n
    ---------------------------------------------------------------------------------
  5.RECORD rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    Range : npt=0.000-\r\n
    CSeq : 5\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 9184120365130522022
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 5\r\n
                                                                Session : 9184120365130522022
                                                                RTP-Info : url=rtsp://127.0.0.1:554/tt.sdp/streamid=0,url=rtsp://127.0.0.1:554/tt.sdp/streamid=1\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
    <<=== Data exchange by rtp/rtcp ===>>
    ---------------------------------------------------------------------------------
  6.TEARDOWN rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    CSeq : 6\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 9184120365130522022
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 6\r\n
                                                                Session : 9184120365130522022
                                                                Connection : Close\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
*/


/*
    /////////////////////////////////////////// play ///////////////////////////////////////////////

    
            Client                                                      Server
    ---------------------------------------------------------------------------------
  1.OPTIONS rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    CSeq : 1\r\n
    User-Agent : Lavf57.71.100\r\n
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 1\r\n
                                                                Public : DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  2.DESCRIBE rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    Accept : application/sdp
    CSeq : 2\r\n
    User-Agent : Lavf57.71.100\r\n
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 2\r\n
                                                                Cache-Control : no-cache\r\n
                                                                Content-length : 547
                                                                Date : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Expires : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Content-type : application/sdp
                                                                x-Accept-Retransmit : our-retransmit\r\n
                                                                x-Accept-Dynamic-Rate : 1\r\n
                                                                Content-Base : rtsp://127.0.0.1:554/tt.sdp/\r\n
                                                                \r\n
                                                                Session Descriprion Protocol()
    ---------------------------------------------------------------------------------
  3.SETUP rtsp://127.0.0.1:554/tt.sdp/streamid=0 RTSP/1.0\r\n
    Transport : RTP/AVP/UDP;unicast;client_port=6364-6365
    x-Dynamic-Rate : 0\r\n
    CSeq : 3\r\n
    User-Agent : Lavf57.71.100\r\n
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 3\r\n
                                                                Cache-Control : no-cache\r\n
                                                                Session : 7735853132375681257
                                                                Date : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Expires : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Transport : 
                                                                RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=6364-6365;server-port=6970-6971
                                                                x-Dynamic-Rate : 0\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  4.SETUP rtsp://127.0.0.1:554/tt.sdp/streamid=1 RTSP/1.0\r\n
    Transport : RTP/AVP/UDP;unicast;client_port=6366-6367
    x-Dynamic-Rate : 0\r\n
    CSeq : 4\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 7735853132375681257
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 4\r\n
                                                                Cache-Control : no-cache\r\n
                                                                Session : 7735853132375681257
                                                                Date : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Expires : Thu, 16 Aug 2018 06:39:57 GMT\r\n
                                                                Transport : 
                                                                RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=6366-6367;server-port=6970-6971
                                                                x-Dynamic-Rate : 0\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  5.PLAY rtsp://127.0.0.1:554/tt.sdp RTSP/1.0\r\n
    Range : npt=0.000-\r\n
    CSeq : 5\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 7735853132375681257
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 5\r\n
                                                                Session : 7735853132375681257
                                                                Range : npt=now-\r\n
                                                                RTP-Info : url=rtsp://127.0.0.1:554/tt.sdp/streamid=0,url=rtsp://127.0.0.1:554/tt.sdp/trackID=1\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
    <<=== Data exchange by rtp/rtcp ===>>
    ---------------------------------------------------------------------------------
    
*/

#endif
