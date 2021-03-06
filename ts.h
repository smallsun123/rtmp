#ifndef __TS__H__
#define __TS__H__

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
    ////////////////////////////////////////////////////////////////////////////////////////////
								ts header
    ////////////////////////////////////////////////////////////////////////////////////////////
*/
/*
TS Header ts包固定长度 188 byte

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
		
/*
	////////////////////////////////////////////////////////////////////////////////////////////
							pes header
	////////////////////////////////////////////////////////////////////////////////////////////
*/

/*
PES Header

Packet_start_code_prefix(3byte)|Stream_id(1byte)|PES_Packet_length(2byte)|
Optional_PES_header(length >= 3)|Stuffing bytes()|Data
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
7. NALU Unit : 06 00 07 80 D8 31 80 87 0D C0 01 07 00 00 18 00 00 03 00 04 80 
00 

	//(SPS -> 27 & 0x1f == 7) (可变长)  (SPS -> 67 & 0x1f == 7)
8. NALU Delimiter : 00 00 00 01 
9. NALU Unit : 27 64 00 28 AC 2B 60 3C 01 13 F2 E0 22 00 00 03 00 02 00 00 03 
00 3D C0 80 00 64 30 00 00 64 19 37 BD F0 76 87 0C B8 00 


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




#endif
