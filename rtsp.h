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
  1.OPTIONS rtsp://127.0.0.1/tt.sdp RTSP/1.0\r\n
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
    Accept : application/sdp\r\n
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
  3.SETUP rtsp://127.0.0.1:554/tt.sdp/trackID=0 RTSP/1.0\r\n
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
                                                                Transport : RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=6364-6365;server-port=6970-6971
                                                                x-Dynamic-Rate : 0\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  4.SETUP rtsp://127.0.0.1:554/tt.sdp/trackID=1 RTSP/1.0\r\n
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
                                                                Transport : RTP/AVP/UDP;unicast;mode=record;source=127.0.0.1;client_port=6366-6367;server-port=6970-6971
                                                                x-Dynamic-Rate : 0\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
  5.PLAY rtsp://127.0.0.1:554/tt.sdp/ RTSP/1.0\r\n
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
  6.TEARDOWN rtsp://127.0.0.1:554/tt.sdp/ RTSP/1.0\r\n
    CSeq : 6\r\n
    User-Agent : Lavf57.71.100\r\n
    Session : 7735853132375681257
    \r\n
    ---------------------------------------------------------------------------------
                                                                RTSP/1.0 200 OK\r\n
                                                                Server : EasyDarwin/7.0.5 (Build/16.0518; Platform/Linux; Realease/EasyDarwin; State/Development; )\r\n
                                                                CSeq : 6\r\n
                                                                Session : 7735853132375681257
                                                                Connection : Close\r\n
                                                                \r\n
    ---------------------------------------------------------------------------------
    
*/

/*

Session_Description_Protocol(){

        Session Description Protocol Version (v): 0
        Owner/Creator, Session Id (o): - 0 0 IN IP4 127.0.0.1
            Owner Username: -
            Session ID: 0
            Session Version: 0
            Owner Network Type: IN
            Owner Address Type: IP4
            Owner Address: 127.0.0.1
        Session Name (s): No Name
        Connection Information (c): IN IP4 0.0.0.0
            Connection Network Type: IN
            Connection Address Type: IP4
            Connection Address: 0.0.0.0
        Time Description, active time (t): 0 0
            Session Start Time: 0
            Session Stop Time: 0
        Session Attribute (a): tool:libavformat 57.71.100
            Session Attribute Fieldname: tool
            Session Attribute Value: libavformat 57.71.100
        Session Attribute (a): control:*
            Session Attribute Fieldname: control
            Session Attribute Value: *
        Media Description, name and address (m): video 0 RTP/AVP 96
            Media Type: video
            Media Port: 0
            Media Protocol: RTP/AVP
            Media Format: DynamicRTP-Type-96
        Media Attribute (a): 3GPP-Adaptation-Support:1
            Media Attribute Fieldname: 3GPP-Adaptation-Support
            Media Attribute Value: 1
        Media Attribute (a): rtpmap:96 H264/90000
            Media Attribute Fieldname: rtpmap
            Media Format: 96
            MIME Type: H264
            Sample Rate: 90000
        Media Attribute (a): fmtp:96 packetization-mode=1; sprop-parameter-sets=Z2QAHqzZQKA9oQAAAwABAAADADIPFi2W,aOvjyyLA; profile-level-id=64001E
            Media Attribute Fieldname: fmtp
            Media Format: 96 [H264]
            Media format specific parameters: packetization-mode=1
                [Packetization mode: Non-interleaved mode (1)]
            Media format specific parameters: sprop-parameter-sets=Z2QAHqzZQKA9oQAAAwABAAADADIPFi2W,aOvjyyLA
                NAL unit 1 string: Z2QAHqzZQKA9oQAAAwABAAADADIPFi2W
                NAL unit: 6764001eacd940a03da1000003000100000300
                    0... .... = Forbidden_zero_bit: 0
                    .11. .... = Nal_ref_idc: 3
                    ...0 0111 = Nal_unit_type: Sequence parameter set (7)
                    0110 0100 = Profile_idc: High profile (100)
                    0... .... = Constraint_set0_flag: 0
                    .0.. .... = Constraint_set1_flag: 0
                    ..0. .... = Constraint_set2_flag: 0
                    ...0 .... = Constraint_set3_flag: 0
                    .... 0... = Constraint_set4_flag: 0
                    .... .0.. = Constraint_set5_flag: 0
                    .... ..00 = Reserved_zero_2bits: 0
                    0001 1110 = Level_id: 30 [Level 3.0 10 Mb/s]
                    1... .... = seq_parameter_set_id: 0
                    .010 .... = chroma_format_id: 1
                    .... 1... = bit_depth_luma_minus8: 0
                    .... .1.. = bit_depth_chroma_minus8: 0
                    .... ..0. = qpprime_y_zero_transform_bypass_flag: 0
                    .... ...0 = seq_scaling_matrix_present_flag: 0
                    1... .... = log2_max_frame_num_minus4: 0
                    .1.. .... = pic_order_cnt_type: 0
                    ..01 1... = log2_max_pic_order_cnt_lsb_minus4: 2
                    .... .001  01.. .... = num_ref_frames: 4
                    ..0. .... = gaps_in_frame_num_value_allowed_flag: 0
                    ...0 0000  1010 00.. = pic_width_in_mbs_minus1: 39
                    .... ..00  0011 110. = pic_height_in_map_units_minus1: 29
                    .... ...1 = frame_mbs_only_flag: 1
                    1... .... = direct_8x8_inference_flag: 1
                    .0.. .... = frame_cropping_flag: 0
                    ..1. .... = vui_parameters_present_flag: 1
                    ...0 .... = aspect_ratio_info_present_flag: 0
                    .... 0... = overscan_info_present_flag: 0
                    .... .0.. = video_signal_type_present_flag: 0
                    .... ..0. = chroma_loc_info_present_flag: 0
                    .... ...1 = timing_info_present_flag: 1
                    0000 0000  0000 0000  0000 0011  0000 0000 = num_units_in_tick: 768
                    0000 0001  0000 0000  0000 0000  0000 0011 = time_scale: 16777219
                    0... .... = fixed_frame_rate_flag: 0
                    .0.. .... = nal_hrd_parameters_present_flag: 0
                    ..0. .... = vcl_hrd_parameters_present_flag: 0
                    ...0 .... = pic_struct_present_flag: 0
                    .... 0... = bitstream_restriction_flag: 0
                    .... .0.. = rbsp_stop_bit: 0
                    .... ..00 = rbsp_trailing_bits: 0
                NAL unit: 320f162d96
                    0... .... = Forbidden_zero_bit: 0
                    .01. .... = Nal_ref_idc: 1
                    ...1 0010 = Nal_unit_type: Reserved (18)
                        [Expert Info (Warning/Protocol): Reserved NAL unit type]
                            [Reserved NAL unit type]
                            [Severity level: Warning]
                            [Group: Protocol]
                NAL unit 2 string: aOvjyyLA
                NAL unit: 68ebe3cb22c0
                    0... .... = Forbidden_zero_bit: 0
                    .11. .... = Nal_ref_idc: 3
                    ...0 1000 = Nal_unit_type: Picture parameter set (8)
                    1... .... = pic_parameter_set_id: 0
                    .1.. .... = seq_parameter_set_id: 0
                    ..1. .... = entropy_coding_mode_flag: 1
                    ...0 .... = pic_order_present_flag: 0
                    .... 1... = num_slice_groups_minus1: 0
                    .... .011 = num_ref_idx_l0_active_minus1: 2
                    1... .... = num_ref_idx_l1_active_minus1: 0
                    .1.. .... = weighted_pred_flag: 1
                    ..10 .... = weighted_bipred_idc: 2
                    .... 0011  1... .... = pic_init_qp_minus26(se(v)): 3
                    .1.. .... = pic_init_qs_minus26: 0
                    ..00 101. = chroma_qp_index_offset(se(v)): -2
                    .... ...1 = deblocking_filter_control_present_flag: 1
                    0... .... = constrained_intra_pred_flag: 0
                    .0.. .... = redundant_pic_cnt_present_flag: 0
                    ..1. .... = transform_8x8_mode_flag: 1
                    ...0 .... = pic_scaling_matrix_present_flag: 0
                    .... 0010  1... .... = second_chroma_qp_index_offset(se(v)): -2
                    .1.. .... = rbsp_stop_bit: 1
                    ..00 0000 = rbsp_trailing_bits: 0
            Media format specific parameters: profile-level-id=64001E
                Profile: 64001e
        Media Attribute (a): control:trackID=0
            Media Attribute Fieldname: control
            Media Attribute Value: trackID=0
        Media Description, name and address (m): audio 0 RTP/AVP 97
            Media Type: audio
            Media Port: 0
            Media Protocol: RTP/AVP
            Media Format: DynamicRTP-Type-97
        Bandwidth Information (b): AS:139
            Bandwidth Modifier: AS [Application Specific (RTP session bandwidth)]
            Bandwidth Value: 139 kb/s
        Media Attribute (a): 3GPP-Adaptation-Support:1
            Media Attribute Fieldname: 3GPP-Adaptation-Support
            Media Attribute Value: 1
        Media Attribute (a): rtpmap:97 MPEG4-GENERIC/48000/2
            Media Attribute Fieldname: rtpmap
            Media Format: 97
            MIME Type: MPEG4-GENERIC
            Sample Rate: 48000
        Media Attribute (a): fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=1190
            Media Attribute Fieldname: fmtp
            Media Format: 97 [MPEG4-GENERIC]
            Media format specific parameters: profile-level-id=1
            Media format specific parameters: mode=AAC-hbr
            Media format specific parameters: sizelength=13
            Media format specific parameters: indexlength=3
            Media format specific parameters: indexdeltalength=3
            Media format specific parameters: config=1190
        Media Attribute (a): control:trackID=1
            Media Attribute Fieldname: control
            Media Attribute Value: trackID=1
}
*/

#endif
