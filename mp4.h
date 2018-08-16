#ifndef __MP4__H__
#define __MP4__H__


/*
	MP4文件格式中，所有的内容存在一个称为movie的容器中。一个movie可以由多个tracks组成。每个track就是一个随时间变化的媒体序列，例如，视频帧序列。
	track里的每个时间单位是一个sample，它可以是一帧视频，或者音频。sample按照时间顺序排列。注意，一帧音频可以分解成多个音频sample，所以音频一般用sample作为单位，
	而不用帧。MP4文件格式的定义里面，用sample这个单词表示一个时间帧或者数据单元。每个track会有一个或者多个sample descriptions。
	track里面的每个sample通过引用关联到一个sample description。这个sampledescriptions定义了怎样解码这个sample，例如使用的压缩算法。
*/
/*
一、Box: Header + Data
	1.Header
		1) size, (4byte) (size = 4 + type + datasize)
		2) type, (4byte)
		--3) largesize, (8byte), if(size == 0) 可选
		--4) uuid, (16byte), if(type == uuid) 可选
	2.Data
	|--------------------------------------------------------------------------------------------------------------|
	|									Box									   |
	|--------------------------------------------------------------------------------------------------------------|
	|								Header						     |	Data	   |
	|----------------------------------------------------------------------------------------------|---------------|
	| size (4byte) | type (4byte) | largesize (8byte), if(size==0) | uuid (16byte), if(type==uuid) | 	Data	   |
	|-----------------------------|----------------------------------------------------------------|---------------|
	|		固定部分		|					可选部分				     |		   |
	|-----------------------------|----------------------------------------------------------------|---------------|

	
二、FullBox: Header + Data
	1.Header
		1) size, (4byte)
		2) type, (4byte)
		3) version, (1byte)
		4) flags, (3byte)
		--5) largesize, (8byte), if(size == 0) 可选
		--6) uuid, (16byte), if(type == uuid) 可选
	|--------------------------------------------------------------------------------------------------------------------------------------------------------|
	|												Full Box												   |
	|--------------------------------------------------------------------------------------------------------------------------------------------------------|
	|										Header						    			         |		Data		   |
	|--------------------------------------------------------------------------------------------------------------------------------|-----------------------|
	| size (4byte) | type (4byte) | version (1byte) | flags (3byte) | largesize (8byte), if(size==0) | uuid (16byte), if(type==uuid) | 		Data		   |
	|---------------------------------------------------------------|----------------------------------------------------------------|-----------------------|
	|				固定部分					    |					可选部分					   |				   |
	|---------------------------------------------------------------|----------------------------------------------------------------|-----------------------|
*/

/*
    --Root
	 |--ftype
	 |--moov
	     |--mvhd
	     +--trak
	     +--trak
	     +--udta
	 |--free
	 |--free
	 |--mdat
*/

/*

一、 ftype
	1. size, (4byte) , 数据长度, 包括该字段本身和类型字段
	2. type, (4byte), ftype = 66 74 79 70
	3. data, (size), 数据内容

二、 moov
	Movie box 定义了一部电影的数据信息。它的类型是 'moov'，是一个容器 box ，至少必须包含三种 box 中的一种—
	1. movie header box('mvhd'), 必须至少包含 movie header box 和 referencemovie box 中的一种。也可以包含其他的 box
		1) 一个 clippingatom ('clip')
		
		3) 一个colortable box ('ctab')
		4) 一个userdata box ('udta')
	2. compressed movie box('cmov')
	3. referencemovie box ('rmra')

	4. 一个或几个 trackatoms ('trak'), Track 就是电影中可以独立操作的媒体单位，例如一个声道就是一个 track。
	
	其中 movie header box 定义了整部电影的 time scale，duration 信息以及 displaycharacteristics。 

	mvhd
	-----------------------------------------------------------
	|    字段	| 长度(字节) | 			描述
	|-----------|------------|---------------------------------
	| size	|	4	 |		mvhd size
	|-----------|------------|---------------------------------
	| type	| 	4	 | 		mvhd
	|-----------|------------|---------------------------------
	| version	|	1	 |		版本
	|-----------|------------|---------------------------------
	| flag	|	3	 |		扩展标志	
	|-----------|------------|---------------------------------
	|create_time|	4	 |		生成时间  基准时间 1904-1-1 0:0:0 AM
	|-----------|------------|---------------------------------
	|modify_time|	4	 | 		修改时间  基准时间 1904-1-1 0:0:0 AM
	|-----------|------------|---------------------------------
	| timescale |	4	 |		时间基
	|-----------|------------|---------------------------------
	| duration  |	4	 |		时长
	|-----------|------------|---------------------------------
	| preferrate|	4	 |		播放速度 1.000正常速度
	|-----------|------------|---------------------------------
	| volume	|	2	 |		播放音量 1.000最大音量
	|-----------|------------|---------------------------------
	| reversed  |	10	 |		保留
	|-----------|------------|---------------------------------
	| matrix	|	36	 |	矩阵结构，定义2个坐标空间映射关系
	|-----------|------------|---------------------------------
	| preview tm|	4	 |	预览时间，开始预览的时间
	|-----------|------------|---------------------------------
	|preview dur|	4	 |	预览时长
	|-----------|------------|---------------------------------
	|poster tm  |	4	 | the time of movie post
	|-----------|------------|---------------------------------
	|select tm  | 	4	 | the start time of current selection
	|-----------|------------|---------------------------------
	|select dur |	4 	 | the duration of current selection
	|-----------|------------|---------------------------------
	|current tm |	4	 | 	当前时间
	|-----------|------------|---------------------------------
	|next trakid|	4	 |	下一个待添加的 trak id
	|-----------|------------|---------------------------------


	trak
	---------------------------------------------------------
	|	Trak header box 		tkhd 			必选
	---------------------------------------------------------
	|	clipping box		clip			
	---------------------------------------------------------
	|	Edit box			edit
	---------------------------------------------------------
	|	Trak reference box	tref
	---------------------------------------------------------
	|	Trak loading setting box 	load
	---------------------------------------------------------
	|	Trak input map box	imap
	---------------------------------------------------------
	|	Media box			mdia			必选
	---------------------------------------------------------
	|	User define data box	udta
	---------------------------------------------------------


	tkhd
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == tkhd			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	Create time				|	4
	---------------------------------------------------------
	|	Modification time			| 	4
	---------------------------------------------------------
	|	Trak ID				|	4
	---------------------------------------------------------
	|	Reserved				|	4
	---------------------------------------------------------
	|	Duration				| 	4
	---------------------------------------------------------
	|	Reserved				|	8
	---------------------------------------------------------
	|	Layer					|	2
	---------------------------------------------------------
	|	Alternate Group			|	2
	---------------------------------------------------------
	|	Volume				|	2
	---------------------------------------------------------
	|	Reserved				|	2
	---------------------------------------------------------
	|	Matrix structure			|	36
	---------------------------------------------------------
	|	Trak width				|	4
	---------------------------------------------------------
	|	Trak height				| 	4
	---------------------------------------------------------

	mdia	----  Media box
	Media box 定义了track 的媒体类型和 sample 数据，例如音频或视频，描述 sample 数据的 media handler component，media timescale and track duration 
	以及 media-and-track-specific 信息，例如音量和图形模式。它也可以包含一个引用，指明媒体数据存储在另一个文件中。也可以包含一个 sample table box，
	指明 sample description, duration, byte offset from the data reference for each media sample.
	Media box 的类型是'mdia'。它是一个容器 box，必须包含一个 media header box ('mdhd')，一个 handler reference ('hdlr')，一个媒体信息引用('minf')和用户数据atom('udta').

	mdia ----  Media box
	---------------------------------------------------------
	|	size
	---------------------------------------------------------
	|	type == mdia
	---------------------------------------------------------
	|	media header box			mdhd		必选
	---------------------------------------------------------
	|	handler reference box		hdlr
	---------------------------------------------------------
	|	media information	box		minf
	---------------------------------------------------------
	|	user data box			udta
	---------------------------------------------------------


	mdhd
	Media header box 定义了媒体的特性，例如 time scale 和 duration。它的类型是 'mdhd'.
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == mdhd			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	Create time				|	4
	---------------------------------------------------------
	|	Modification time			| 	4
	---------------------------------------------------------
	|	Time scale				|	4
	---------------------------------------------------------
	|	Duration				|	4
	---------------------------------------------------------
	|	Language				|	2
	---------------------------------------------------------
	|	Quality				|	2
	---------------------------------------------------------

	hdrl
	Handler reference box 定义了描述此媒体数据的 media handler component，类型是'hdlr'。
	在过去，handler reference box 也可以用来数据引用，但是现在，已经不允许这样使用了。
	一个 media box 内的 handler box 解释了媒体流的播放过程。例如，一个视频 handler 处理一个 video track. 
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == hdrl			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	componet type			|	4
	---------------------------------------------------------
	|	componet sutype			|	4
	---------------------------------------------------------
	|	componet manufacture		|	4
	---------------------------------------------------------
	|	componet flags			|	4
	---------------------------------------------------------
	|	componet flags mask		|	4
	---------------------------------------------------------
	|	componet name			|	variable	"VideoHandler"	"(C) 2007 Google Inc.v08.13.2007"
	---------------------------------------------------------

	minf
	Mediainformation box 的类型是 'minf'，存储了解释该 track 的媒体数据的 handler-specific 的信息。media handler 用这些信息将媒体时间映射到媒体数据，并进行处理。
	它是一个容器 box ，包含其他的子 box。
	这些信息是与媒体定义的数据类型特别对应的，而且 media information box 的格式和内容也是与解释此媒体数据流的 media handler 密切相关的。
	其他的 media handler 不知道如何解释这些信息。

	--minf
	   +--vmhd / smhd
	   +--dinf
	   +--stbl

	   Video  media information box 是视频媒体的第一层 box，包含其他的定义视频媒体数据的特性。  
	   Sound  media information box 是音频媒体的第一层 box，包含其他的定义音频媒体数据的特性。

	vmhd
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == vmhd			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	-----------------------------------------------------------------------------------------------------------------------------------------
	|	graphics mode			|	2	The transfer mode. The  transfer mode specifies which Boolean operation  
	|						|		Quick Draw should perform when drawing or 
	|						|		transferring an image from one location  to another.
	-----------------------------------------------------------------------------------------------------------------------------------------
	|	operation color			|	(r,g,b)3x2	Three 16-bit values  that specify the red, green, and blue colors for the transfer mode operation  
	|						|			indicated in the graphics mode field.
	-----------------------------------------------------------------------------------------------------------------------------------------

	smhd
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == hdrl			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	balance				|	2	音频的均衡是用来控制计算机的两个扬声器的声音混合效果，一般是0。一般值是0。
	---------------------------------------------------------
	|	reserved				|	2	保留字段，缺省为0
	---------------------------------------------------------

	dinf
	handler reference 定义 data handler component 如何获取媒体数据，data handler 用这些数据信息来解释媒体数据。
	Data information box 的类型是'dinf'。它是一个容器 box，包含其他的子 box。
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == dinf			| 	4
	---------------------------------------------------------
			|	字段					|	长度(字节)
			---------------------------------------------------------
			|	size					|	4
			---------------------------------------------------------
			|	type == dref			| 	4
			---------------------------------------------------------
			|	version				|	1
			---------------------------------------------------------
			|	flags					| 	3
			---------------------------------------------------------
			|	number of entries			|	4
			---------------------------------------------------------
					|	字段					|	长度(字节)
					---------------------------------------------------------
					|	size					|	4
					---------------------------------------------------------
					|	type == url				| 	4
					---------------------------------------------------------
					|	version				|	1
					---------------------------------------------------------
					|	flags					| 	3
					---------------------------------------------------------
					|	Data					|	variable
					---------------------------------------------------------

	--dinf, size 28 bytes, Data Info box
	   |--dref, size 20 bytes, Data Reference box
	       |--version:00
	       |--flags:000000
	       |--number of entries:1
	       |--Data Reference #1,selfreferencing
	           |--size:12
	           |--type:url
	           |--version:0
	           |--flags:000001, --selfreferencing

	stbl ---- sample table atom
      存储媒体数据的单位是 samples。一个 sample 是一系列按时间顺序排列的数据的一个 element。Samples 存储在 media 中的 chunk 内，可以有不同的 durations。
      Chunk 存储一个或者多个 samples，是数据存取的基本单位，可以有不同的长度，一个 chunk 内的每个 sample 也可以有不同的长度。
      例如如下图，chunk 2和3 不同的长度，chunk 2内的sample 5和6 的长度一样，但是sample 4和5，6的长度不同。

      --stbl, size 155659 bytes, Sample Table Box
          +---stsd, size 83 bytes, Sample Description Box
          +---stts, size 89856 bytes, Time-to-sample Box
          +---stsc, size 5060 bytes, Sample-to-Chunk Box
          +---stsz, size 58112 bytes, Sample size Box
          +---stco, size 2508 bytes, Chunk offset Box


	stsd
	sample description box 的类型是'stsd'，包含了一个 sample description 表。根据不同的编码方案和存储数据的文件数目，
	每个 media 可以有一个到多个 sample description。sample-to-chunk box 通过这个索引表，找到合适 medai 中每个 sample 的 description。
	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == stsd			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	number of entries			|	4
	---------------------------------------------------------
			|	字段					|	长度(字节)
			---------------------------------------------------------
			|	size					|	4
			---------------------------------------------------------
			|	type == mp4a			| 	4
			---------------------------------------------------------
			|	reserverd				| 	6
			---------------------------------------------------------
			|	data reference index		|	2
			---------------------------------------------------------
	--stsd, size 83 bytes, Sample Description Box
	    |---version:00
	    |---flags:000000
	    |---number of entries:1
	    |---Sample description #1, dref #1 (self-ref)
	    		|---sample description size:75
	    		|---data format mp4a
	    		|---reserved:00
	    		|---reserved:00
	    		|---reserved:00
	    		|---data reference index:1
	    		|---audio media sample info, size #0
	    		|---optional description box

	stts ---- Time-to-sample atoms
      Time-to-sampleatoms 存储了 media sample 的 duration 信息，提供了时间对具体 data sample 的映射方法，通过这个 box，你可以找到任何时间的 sample，类型是'stts'。
      这个 box 可以包含一个压缩的表来映射时间和 sample 序号，用其他的表来提供每个 sample 的长度和指针。
      表中每个条目提供了在同一个时间偏移量里面连续的 sample 序号， 以及 samples 的偏移量。递增这些偏移量，就可以建立一个完整的 time-to-sample 表.

      ---stts, size 43304 bytes, Time-to-sample Box
           |---version:00
           |---flags:000000
           |---number of entries 5412
           |---Time-to-Sample table, length 43296, count 5412
           			+---entry 1: sample count 1, sample duration 42
           			+---entry 2: sample count 1, sample duration 41
           			+---entry 3: sample count 2, sample duration 42
           			+---entry 4: sample count 1, sample duration 41
           			+---entry 5: sample count 2, sample duration 42
           			+---entry 6: sample count 1, sample duration 41
           			+---entry 7: sample count 2, sample duration 42
           			+---entry 8: sample count 1, sample duration 41
           			+---entry 9: sample count 2, sample duration 42

	stsc  ----  sample to chunk box
      当添加 samples 到 media 时，用 chunks 组织这些 sample，这样可以方便优化数据获取。一个 trunk 包含一个或多个 sample，chunk 的长度可以不同，
      chunk 内的 sample 的长度也可以不同。sample-to-chunkatom 存储 sample 与 chunk 的映射关系。
	Sample-to-chunkatoms 的类型是 'stsc'。它也有一个表来映射 sample 和 trunk 之间的关系，查看这张表，就可以找到包含指定 sample 的 trunk，从而找到这个 sample。

	---stsc, size 512 bytes, Sample-to-Chunk Box
	     |---version:00
	     |---flags:000000
	     |---number of entries:42
	     |---Sample-to-Chunk table, length 504, count 42
	     			+---entry 1: first chunk 1, sample per chunk 13, sample description 1 (self-ref)
	     			+---entry 2: first chunk 29, sample per chunk 12, sample description 1 (self-ref)
	     			+---entry 3: first chunk 30, sample per chunk 13, sample description 1 (self-ref)
	     			+---entry 4: first chunk 58, sample per chunk 12, sample description 1 (self-ref)
	     			+---entry 5: first chunk 59, sample per chunk 13, sample description 1 (self-ref)
	     			+---entry 6: first chunk 87, sample per chunk 12, sample description 1 (self-ref)

	--------------------------------------------------------------------
	| entry number |		chunk		| sample/chunk |	samples
	---------------|--------------------|--------------|----------------
	|	 1	   |	  1 	至    28	|	13	   | 28 * 13
	---------------|--------------------|--------------|----------------
	|	 2	   |		29		|	12	   |    12
	---------------|--------------------|--------------|----------------
	|	 3	   |    30  至    57	|	13	   | (57 - 30) * 13
	---------------|--------------------|--------------|----------------
	|	 4	   |  	58		|	12	   |	  12
	---------------|--------------------|--------------|----------------
	|	 5	   |	  59  至    86	|	12	   | (86 - 59) * 12
	--------------------------------------------------------------------


	stsz  ----   sample size atoms
      sample size box 定义了每个sample 的大小，它的类型是'stsz'，包含了媒体中全部 sample 的数目和一张给出每个 sample 大小的表。这样，媒体数据自身就可以没有边框的限制。

	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == stsz			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	sample size				|	4 	如果所有的sample有相同的长度，这个字段就是这个值。否则，这个字段的值就是0。那些长度存在sample size表中
	---------------------------------------------------------
	|	number of entries			|	4
	---------------------------------------------------------

	---stsz, size 32360 bytes, Sample size Box
	     |---version:00
	     |---flags:000000
	     |---number of entries:8087
	     |---Sample Size Table, length 32348, count 8087
	     			|---sample 1, sample size 000000ae
	     			|---sample 2, sample size 0000001e
	     			|---sample 3, sample size 0000001e
	     			|---sample 4, sample size 0000001e
	     			|---sample 5, sample size 0000001e
	     			|---sample 6, sample size 0000001e
	     			|---sample 7, sample size 0000001e
	     			|---sample 8, sample size 0000001e
	     			|---sample 9, sample size 000015cc


	stco	----  Chunk Offset Box
	Chunk  Offset Box 定义了每个 trunk 在媒体流中的位置，它的类型是'stco'。位置有两种可能，32位的和64位的，后者对非常大的电影很有用。
	在一个表中只会有一种可能，这个位置是在整个文件中的，而不是在任何 box 中的，这样做就可以直接在文件中找到媒体数据，而不用解释 box。
	需要注意的是一旦前面的 box 有了任何改变，这张表都要重新建立，因为位置信息已经改变了。

	---------------------------------------------------------
	|	字段					|	长度(字节)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == stco			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	number of entries			| 	4
	---------------------------------------------------------
	|	chunk offset 			字节偏移量从文件开始到当前chunk。这个表根据 chunk number 索引，第一项就是第一个trunk，第二项就是第二个trunk
	---------------------------------------------------------

	---stco, size 2504 bytes, Chunk Offset Box
	     |---version:00
	     |---flags:000000
	     |---number of entries:624
	     |---Chunk Offset Table, length 2496, count 624
           			|---chunk 1, 00039D28
           			|---chunk 2, 0003D19B
           			|---chunk 3, 0003F50D
           			|---chunk 4, 00041B22
*/



/*

------Root
	 |---ftype (File Type Box)
	 |	|---Header
	 |	|	|---size (4byte) (Header + Data)
	 |	|	|---type (4byte) ('ftype')
	 |	|
	 |	|---Data (size byte)
	 |
	 |---moov (Movie Box)
	 	|---Header
	 	|	|---size (4byte)
	 	|	|---type (4byte) ('moov')	//6D 6F 6F 76
	 	|
	 	|---Data
	 		|---cmov (Compressed Movie Atom)	//压缩过的电影信息容器
	 		|
	 		|
	 		|---rmra (Reference Movie Atom)	//参考电影信息容器
	 		|
	 		|
	 		|	//全文件唯一的（一个文件中只能包含一个mvhd box）对整个文件所包含的媒体数据作全面的全局的描述。
	 		|	//包含了媒体的创建与修改时间时间刻度、默认音量、色域、时长等信息。
	 		|
		      |---mvhd (Movie Header Box) 		//存放未压缩过的影片信息的头容器	
		      |	|---Header (Full_Header) (12 byte)
		      |	|	|---size (4byte)	( Full_Header + Data )
		      |	|	|---type (4byte) ('mvhd')				//6D 76 68 64
		      |	|	|---version (1byte)
		      |	|	|---flags (3byte)
		      |	|
		      |	|---Data (96 btye)
		      |		|---create_time (4byte) 			(version == 0 32/64 version == 1)
		      |		|---modify_time (4byte)				(version == 0 32/64 version == 1)
		      |		|---time_scale (4byte)
		      |		|---duration (4byte)				(version == 0 32/64 version == 1)   RealTimet = duration/time_scale
		      |		|---rate (4byte) 播放速度 (prefer_rate)
		      |		|---volume (2byte) 播放音量
		      |		|---reserved (10byte)
		      |		|---matrix (4byte * 9)
		      |		|---preview_time (4byte) 预览时间		\
		      |		|---preview_duration (4byte) 预览时长	|
		      |		|---poster_time (4byte)				|====> pre_defined (4byte * 6)
		      |		|---selection_time (4byte)			|
		      |		|---selection_duration (4byte)		|
		      |		|---current_time (4byte) 当前时间		/
		      |		|---next_trak_id (4byte)
		      |
		      |	// 不同的 Track box 是相互独立的。每个track box 都携带独立的时域空域信息，同时包含相关的 media box。
		      |	// track box 有两种：media track 和 hint track，前者用于保存 media 相关信息，后者包含用于流媒体的打包信息
		      |---track (Track Structure Box) (一个mp4文件至少包含一个)	//video_track | audio_track | subtitle_track ：每个track都是独立的，具有自我特征与属性的，因此需要各自描述互不干涉。
		      |	|---Header
		      |	|	|---size (4byte)
		      |	|	|---type (4byte) ('track')
		      |	|
		      |	|---Data
		      |		|---tkhd (track header box) 必选
		      |		|	|---Header (Full_Header) (12 byte)
		      |		|	|	|---size (4byte) ( Full_Header + Data )
		      |		|	|	|---type (4byte) ('tkhd')
		      |		|	|	|---version (1byte)
		      |		|	|	|---flags (3byte)
		      |		|	|
		      |		|	|---Data
		      |		|		|---create_time (4byte)
		      |		|		|---modification_time (4byte)
		      |		|		|---track_ID (4byte)			//用于唯一的表示当前track
		      |		|		|---reserved (4byte)
		      |		|		|---duration (4byte)			//用于记录当前track的播放长度
		      |		|		|---reserved (8byte)
		      |		|		|---layer (2byte)
		      |		|		|---alternative_group (2byte)
		      |		|		|---volume (2byte)
		      |		|		|---reserved (2byte)
		      |		|		|---matrix_structure (36byte)
		      |		|		|---track_width (4byte)
		      |		|		|---track_height (4byte)
		      |		|
		      |		|---clip (clipping box) //影片剪辑信息
		      |		|---matt (track matte box)
		      |		|---edts (Edit atoms)	//定义了创建movie中一个track的一部分媒体。所有的edit都在一个表里面，包括每一部分的时间偏移量和长度。Edit atoms 的类型是'edts'。如果没有该表，则此track会被立即播放。一个空的edit用来偏移track的起始时间。
		      |		|	|---Header
		      |		|	|	|---size (4byte)
		      |		|	|	|---type (4byte) ('edts')
		      |		|	|
		      |		|	|---Data
		      |		|		|---elst (Edit list atom)	//用来映射movie的时间到此track media的时间
		      |		|			|---Header (Full_Header)
		      |		|			|	|---size (4byte) (Full_Header + Data)
		      |		|			|	|---type (4byte) ('elst')
		      |		|			|	|---version (1byte)
		      |		|			|	|---flags (3byte)
		      |		|			|
		      |		|			|---Data
		      |		|				|---entry_cout (4byte)
		      |		|				|---edit_list[3] (3 * 4byte)
		      |		|					|---track_duration (4byte)	//duration of this edit segment in units of the movie’s timescale.
		      |		|					|---time (4byte)		//starting time within the media of this edit segment (in mediatimescale units)。值为-1表示是空edit。Track中的最后一个edit永远不能为空。Any difference between the movie’s duration and the track’s duration is expressed as an implicit empty edit.
		      |		|					|---speed (4byte)		//relative rate at which to play the media corresponding to this edit segment。不能是0或负数。
		      |		|
		      |		|---tref (track reference box)	//描述两track之间关系。 关联音频 track 和 视频 track，描述一组音视频 track
		      |		|	|---Header
		      |		|	|	|---size (4byte)
		      |		|	|	|---type (4byte) ('tref')
		      |		|	|
		      |		|	|---Data---track_reference_type_box
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('tmcd')	//Time code. Usually references a time code track.
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('chap')	//Chapter or scene list. Usually references a text track.
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('sync')	//Synchronization. Usually between a video and sound track. Indicates that the two tracks are synchronized. The reference can be from either track to the other, or there may be two references.
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('scpt')	//Transcript. Usually references a text track.
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('ssrc')	//Non-primary source. Indicates that the referenced track should send its data to this track, rather than presenting it. The referencing track will use the data to modify how it presents its data. See “Track Input Map Atoms” for more information.
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('hint')	//the referenced track(s) contain the original media for this hint track 
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('cdsc')	//this track describes the referenced track. 
		      |		|		|
		      |		|		|---Data
		      |		|		|	|---track_IDs (variable)
		      |		|		|
		      |		|		|---Header
		      |		|		|	|---size (4byte)
		      |		|		|	|---type (4byte) ('hind')	//this track depends on the referenced hint track, i.e., it should only be used if the referenced hint track is used. 
		      |		|		|
		      |		|		|---Data
		      |		|			|---track_IDs (variable)
		      |		|		
		      |		|		
		      |		|
		      |		|---trgr (track grouping indication)
		      |		|---load (track loading setting box)
		      |		|---imap (track input map box)
		      |		|---meta (metadata box)
		      |		|
		      |		|---mdia (media info container box) 必选
		      |		|	|---Header
		      |		|	|	|---size (4byte)
		      |		|	|	|---type (4byte) ('mdia')
		      |		|	|
		      |		|	|---Data
		      |		|		|---mdhd (media header box) 必选
		      |		|		|	|---Header (Full_Header)
		      |		|		|	|	|---size (4byte)
		      |		|		|	|	|---type (4byte) ('mdhd')
		      |		|		|	|	|---version (1byte)
		      |		|		|	|	|---flags (3byte)
		      |		|		|	|
		      |		|		|	|---Data
		      |		|		|		|---creation_time (4byte)
		      |		|		|		|---modification_time (4byte)
		      |		|		|		|---time_scale (4byte)
		      |		|		|		|---duration (4byte)
		      |		|		|		|---language (2byte)
		      |		|		|		|---quality (2byte)
		      |		|		|
		      |		|		|---hdlr (handler reference box)
		      |		|		|	|---Header (Full_Header)
		      |		|		|	|	|---size (4byte)
		      |		|		|	|	|---type (4byte) ('hdlr')
		      |		|		|	|	|---version (1byte)
		      |		|		|	|	|---flags (3byte)
		      |		|		|	|
		      |		|		|	|---Data
		      |		|		|		|---component_type (4byte)	('vide','soun','hint')
		      |		|		|		|---component_subtype (4byte) 	//mdir
		      |		|		|		|---component_manufacture (4byte)	//appl
		      |		|		|		|---component_flags (4byte)
		      |		|		|		|---component_flags_mask (4byte)
		      |		|		|		|---component_name (variable byte)
		      |		|		|
		      |		|		|---minf (media info container box)
		      |		|		|	|---Header
		      |		|		|	|	|---size (4byte)
		      |		|		|	|	|---type (4byte) ('minf')
		      |		|		|	|
		      |		|		|	|---Data
		      |		|		|		|---vmhd (video media information header box) (二选一) (video track only)
		      |		|		|		|	|---Header (Full_Header)
		      |		|		|		|	|	|---size (4byte)
		      |		|		|		|	|	|---type (4byte) ('vmhd')
		      |		|		|		|	|	|---version (1byte)
		      |		|		|		|	|	|---flags (3byte)
		      |		|		|		|	|
		      |		|		|		|	|---Data
		      |		|		|		|		|---graphics_mode (2byte)
		      |		|		|		|		|---operation_color (2byte) (r)
		      |		|		|		|		|---operation_color (2byte) (g)
		      |		|		|		|		|---operation_color (2byte) (b)
		      |		|		|		|
		      |		|		|		|---smhd (sound media information header box) (二选一) (audio track only)
		      |		|		|		|	|---Header (Full_Header)
		      |		|		|		|	|	|---size (4byte)
		      |		|		|		|	|	|---type (4byte) ('smhd')
		      |		|		|		|	|	|---version (1byte)
		      |		|		|		|	|	|---flags (3byte)
		      |		|		|		|	|
		      |		|		|		|	|---Data
		      |		|		|		|		|---balance (2byte)
		      |		|		|		|		|---reserved (2byte)
		      |		|		|		|
		      |		|		|		|---hmhd (hint media header box) (hint track only)
		      |		|		|		|
		      |		|		|		|---sthd (subtitle media header box) (subtitle track only)
		      |		|		|		|
		      |		|		|		|---nmhd (null media header box) (some tracks only)
		      |		|		|		|
		      |		|		|		|---dinf (data information box)
		      |		|		|		|	|---Header
		      |		|		|		|	|	|---size (4byte)
		      |		|		|		|	|	|---type (4byte) ('dinf')
		      |		|		|		|	|
		      |		|		|		|	|---Data
		      |		|		|		|		|---dref (data reference box)
		      |		|		|		|			|---Header (Full_Header)
		      |		|		|		|			|	|---size (4byte) ( Full_Header + Data)
		      |		|		|		|			|	|---type (4byte) ('dref')
		      |		|		|		|			|	|---version (1byte)
		      |		|		|		|			|	|---flags (3byte)
		      |		|		|		|			|
		      |		|		|		|			|---Data
		      |		|		|		|				|---number_of_entries (4byte)
		      |		|		|		|				|---data_reference (box)
		      |		|		|		|					|---Header (Full_Header)
		      |		|		|		|					|	|---size (4byte) ( Full_Header + Data)
		      |		|		|		|					|	|---type (4byte) ('url')
		      |		|		|		|					|	|---version (1byte)
		      |		|		|		|					|	|---flags (3byte)
		      |		|		|		|					|
		      |		|		|		|					|---Data
		      |		|		|		|						|---Data (variable byte)
		      |		|		|		|
		      |		|		|		|---stbl (sample table box, container for time/space map)
		      |		|		|			|---Header
		      |		|		|			|	|---size (4byte)
		      |		|		|			|	|---type (4byte) ('stbl')
		      |		|		|			|
		      |		|		|			|---Data
		      |		|		|				|---stsd (sample description box) (codec types, initialization etc.)	//主要描述当前track有关的编码信息，以及用于初始化解码的附加信息。
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stsd')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---sample_description[] 不同的媒体类型有不同的sample description，但是每个sample description的前四个字段是相同的，包含以下的数据成员
		      |		|		|				|			|---size (4byte)
		      |		|		|				|			|---type (4byte) ('avc1')	//视频
		      |		|		|				|			|---reserved (6byte)
		      |		|		|				|			|---data_reference_index (2byte)	//前4个字段相同
		      |		|		|				|				|---version (2byte)			//box 版本, 0或1, 一般为0, (以下字节数均按version=0)
		      |		|		|				|				|---Revision_level (2byte)
		      |		|		|				|				|---Vendor (4 byte)
		      |		|		|				|				|---Temporal_quality (4 byte)		//时间的压缩
		      |		|		|				|				|---Spatial_quality (4 byte)		//视频的质量
		      |		|		|				|				|---width (2byte)
		      |		|		|				|				|---height (2byte)
		      |		|		|				|				|---horizresolution (4byte)	//垂直分辨率
		      |		|		|				|				|---vertresolution (4byte)	//水平分辨率
		      |		|		|				|				|---Data_size (4byte)  	//A 32-bit integer that must be set to 0
		      |		|		|				|				|---frame_count (2byte)	//视频必须为1	//A 16-bit integer that indicates how many frames of compressed data are stored in each sample. Usually set to 1.
		      |		|		|				|				|---compressorname (32byte) (string)	//A 32-byte Pascal string containing the name of the compressor that created the image, such as “jpeg”
		      |		|		|				|				|---depth (2byte)		//表示压缩图像的像素深度的16位整数。1, 2, 4，8, 16, 24的值，32表示彩色图像的深度。只有在图像包含时才使用值32。阿尔法通道。灰度值分别为34, 36、40和表示2、4和8位灰度值。图像.
		      |		|		|				|				|---Color_table_ID (2byte)	//标识要使用的颜色表的16位整数。如果这个字段被设置为- 1，默认颜色表应用于指定深度。对于每像素16位以下的深度，这表示一个标准。指定深度的Macintosh颜色表。深度为16, 24，32没有颜色表。如果颜色表ID设置为0，则颜色表包含在示例描述本身中。颜色
		      |		|		|				|				|---size (4byte)
		      |		|		|				|				|---type (4byte) ('avcC')	//AVC sequence header  //AVCDecoderConfigurationRecord
		      |		|		|				|				|---Configuration_Version (1byte)
		      |		|		|				|				|---AVCProfile_Indication (1byte)
		      |		|		|				|				|---Profile_Compatibility (1byte)
		      |		|		|				|				|---AVCLevel_Indication   (1byte)
		      |		|		|				|				|---[reserved (6 bit) | lengthSizeMinusOne (2 bit)]   //NALUnitLength的长度-1	//非常重要，是 H.264 视频中 NALU 的长度，计算方法是 1 + (lengthSizeMinusOne & 3)，实际计算结果一直是4
		      |		|		|				|				|---[reserved (3 bit) | numOfSequenceParameterSets (5 bit)]   //sps个数, 一般为1  //SPS 的个数，计算方法是 numOfSequenceParameterSets & 0x1F，实际计算结果一直为1
		      |		|		|				|				|---sps_len (2byte)
		      |		|		|				|				|---sequenceParameterSetNALUnits[] (sps_len byte)
		      |		|		|				|				|---numOfPictureParameterSets (1 byte)
		      |		|		|				|				|---pps_len (2 byte)
		      |		|		|				|				|---pictureParameterSetNALUnit[] (pps_len byte)
		      |		|		|				|			|---size (4byte)
		      |		|		|				|			|---type (4byte) ('mp4a') 	//音频
		      |		|		|				|			|---reserved (6byte)
		      |		|		|				|			|---data_reference_index (2byte)	//前4个字段相同
		      |		|		|				|				|---reserved (4byte)
		      |		|		|				|				|---reserved (4byte)
		      |		|		|				|				|---channels (2byte)
		      |		|		|				|				|---sample_size (2byte)	//默认16，或者为8 
		      |		|		|				|				|---pre_defined (2byte)
		      |		|		|				|				|---reserved (2byte)
		      |		|		|				|				|---sample_rate (4byte)
		      |		|		|				|
		      |		|		|				|---stts (time to sample box) (decoding time-to-sample)
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stts')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---time_to_sample_talbe[] (uint32[2])
		      |		|		|				|			|---sample_count (4byte)  有相同duration的连续sample的数目
		      |		|		|				|			|---sample_duration (4byte) 每个sample的duration
		      |		|		|				|
		      |		|		|				|---stsc (sample to chunk box) (sample-to-chunk, partial data-offset info)
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stsc')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---chunk_entry[] (uint32[3]) (sample_to_chunk_table)
		      |		|		|				|			|---first_chunk (4byte)
		      |		|		|				|			|---samples_per_chunk (4byte)		每个 trunk 内的 sample 数目
		      |		|		|				|			|---sample_description_index (4byte)  与这些 sample 关联的 sample description 的序号
		      |		|		|				|
		      |		|		|				|---stsz (sample size box)
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stsz')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---sample_size (4byte) 如果所有的sample有相同的长度，这个字段就是这个值，且 entry_size[] 字段不存在，否则，这个字段的值就是0。那些长度存在 entry_size[] 表中
		      |		|		|				|		|---sample_count (4byte) (number_of_entries)
		      |		|		|				|		|---entry_size[] ( uint32[], varies)
		      |		|		|				|			|---sample_size (4byte) 每个sample的大小, 按 sample 顺序排列
		      |		|		|				|
		      |		|		|				|---stco (chunk offset box) 定义了每个 trunk 在媒体流中的位置
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stco')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---chunk_offset_table[] (uint32[], varies)
		      |		|		|				|			|---chunk_offset (4byte) 字节偏移量从文件开始到当前chunk
		      |		|		|				|
		      |		|		|				|---stss (sync sample box) 确定media中的关键帧
		      |		|		|					|---Header (Full_Header)
		      |		|		|					|	|---size (4byte)
		      |		|		|					|	|---type (4byte) ('stss')
		      |		|		|					|	|---version (1byte)
		      |		|		|					|	|---flags (3byte)
		      |		|		|					|
		      |		|		|					|---Data
		      |		|		|						|---entry_cout (4byte) (number_of_entries)
		      |		|		|						|---sync_sample_table[] (uint32[], varies)
		      |		|		|							|---key_frame_sample_seq (4byte) 
		      |		|		|
		      |		|		|---udta (user data box)
		      |		|
		      |		|---udta (user-defined data box)
		      |
		      +---track
		      +---udta
	 |---free
	 |	|---Header
	 |		|---size (4byte) (Header 8 byte)
	 |		|---type (4byte) ('free')
	 |
	 |---mdat (movie data container) //实际媒体数据。我们最终解码播放的数据都在这里面。
	 	|---Header
	 	|	|---size (4byte)
	 	|	|---type (4byte) ('mdat')
	 	|
	 	|---Data (实际媒体数据)

*/

/*	Seek
	1、使用 timescale 将目标时间标准化。
	2、通过(stts)time-to-sample box找到指定track的给定时间之前的第一个sample number。
	3、通过(stss)sync sample table查询sample number之前的第一个sync sample。
	4、通过(stsc)sample-to-chunk table查找到对应的chunk number。
	5、通过(stco)chunk offset box查找到对应chunk在文件中的起始偏移量。
	6、最后使用(stsc)sample-to-chunk box和(stsz)sample size box的信息计算出该chunk中需要读取的sample数据，即完成seek。
*/

#endif
