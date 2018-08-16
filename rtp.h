
#ifndef __RTP__H__
#define __RTP__H__


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
一、RTP Over UDP：
	一般RTP是基于UDP的，因为UDP发送的是数据报文。规定最大报文为1500byte（字节），即MTU最大传输单元为1500byte。RTP数据长度最多为1400字节。
	
二、RTP Over TCP：
    从上面RTP格式可以看到RTP如果使用TCP有个致命的缺点，就是没有长度。而TCP是传输的数据流。而RTP是数据报文。如果使用TCP传输就无法知道RTP包有多长。不知如何区分RTP包。
    这里用到RTSP。以下是截取的TCP协议中的一段TCP segment data：

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

    H264 over RTP
    
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
(opt)   |         length        |                         reason for leaving	                          |
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


#endif
