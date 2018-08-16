#ifndef __MP4__H__
#define __MP4__H__


/*
	MP4�ļ���ʽ�У����е����ݴ���һ����Ϊmovie�������С�һ��movie�����ɶ��tracks��ɡ�ÿ��track����һ����ʱ��仯��ý�����У����磬��Ƶ֡���С�
	track���ÿ��ʱ�䵥λ��һ��sample����������һ֡��Ƶ��������Ƶ��sample����ʱ��˳�����С�ע�⣬һ֡��Ƶ���Էֽ�ɶ����Ƶsample��������Ƶһ����sample��Ϊ��λ��
	������֡��MP4�ļ���ʽ�Ķ������棬��sample������ʱ�ʾһ��ʱ��֡�������ݵ�Ԫ��ÿ��track����һ�����߶��sample descriptions��
	track�����ÿ��sampleͨ�����ù�����һ��sample description�����sampledescriptions�����������������sample������ʹ�õ�ѹ���㷨��
*/
/*
һ��Box: Header + Data
	1.Header
		1) size, (4byte) (size = 4 + type + datasize)
		2) type, (4byte)
		--3) largesize, (8byte), if(size == 0) ��ѡ
		--4) uuid, (16byte), if(type == uuid) ��ѡ
	2.Data
	|--------------------------------------------------------------------------------------------------------------|
	|									Box									   |
	|--------------------------------------------------------------------------------------------------------------|
	|								Header						     |	Data	   |
	|----------------------------------------------------------------------------------------------|---------------|
	| size (4byte) | type (4byte) | largesize (8byte), if(size==0) | uuid (16byte), if(type==uuid) | 	Data	   |
	|-----------------------------|----------------------------------------------------------------|---------------|
	|		�̶�����		|					��ѡ����				     |		   |
	|-----------------------------|----------------------------------------------------------------|---------------|

	
����FullBox: Header + Data
	1.Header
		1) size, (4byte)
		2) type, (4byte)
		3) version, (1byte)
		4) flags, (3byte)
		--5) largesize, (8byte), if(size == 0) ��ѡ
		--6) uuid, (16byte), if(type == uuid) ��ѡ
	|--------------------------------------------------------------------------------------------------------------------------------------------------------|
	|												Full Box												   |
	|--------------------------------------------------------------------------------------------------------------------------------------------------------|
	|										Header						    			         |		Data		   |
	|--------------------------------------------------------------------------------------------------------------------------------|-----------------------|
	| size (4byte) | type (4byte) | version (1byte) | flags (3byte) | largesize (8byte), if(size==0) | uuid (16byte), if(type==uuid) | 		Data		   |
	|---------------------------------------------------------------|----------------------------------------------------------------|-----------------------|
	|				�̶�����					    |					��ѡ����					   |				   |
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

һ�� ftype
	1. size, (4byte) , ���ݳ���, �������ֶα���������ֶ�
	2. type, (4byte), ftype = 66 74 79 70
	3. data, (size), ��������

���� moov
	Movie box ������һ����Ӱ��������Ϣ������������ 'moov'����һ������ box �����ٱ���������� box �е�һ�֡�
	1. movie header box('mvhd'), �������ٰ��� movie header box �� referencemovie box �е�һ�֡�Ҳ���԰��������� box
		1) һ�� clippingatom ('clip')
		
		3) һ��colortable box ('ctab')
		4) һ��userdata box ('udta')
	2. compressed movie box('cmov')
	3. referencemovie box ('rmra')

	4. һ���򼸸� trackatoms ('trak'), Track ���ǵ�Ӱ�п��Զ���������ý�嵥λ������һ����������һ�� track��
	
	���� movie header box ������������Ӱ�� time scale��duration ��Ϣ�Լ� displaycharacteristics�� 

	mvhd
	-----------------------------------------------------------
	|    �ֶ�	| ����(�ֽ�) | 			����
	|-----------|------------|---------------------------------
	| size	|	4	 |		mvhd size
	|-----------|------------|---------------------------------
	| type	| 	4	 | 		mvhd
	|-----------|------------|---------------------------------
	| version	|	1	 |		�汾
	|-----------|------------|---------------------------------
	| flag	|	3	 |		��չ��־	
	|-----------|------------|---------------------------------
	|create_time|	4	 |		����ʱ��  ��׼ʱ�� 1904-1-1 0:0:0 AM
	|-----------|------------|---------------------------------
	|modify_time|	4	 | 		�޸�ʱ��  ��׼ʱ�� 1904-1-1 0:0:0 AM
	|-----------|------------|---------------------------------
	| timescale |	4	 |		ʱ���
	|-----------|------------|---------------------------------
	| duration  |	4	 |		ʱ��
	|-----------|------------|---------------------------------
	| preferrate|	4	 |		�����ٶ� 1.000�����ٶ�
	|-----------|------------|---------------------------------
	| volume	|	2	 |		�������� 1.000�������
	|-----------|------------|---------------------------------
	| reversed  |	10	 |		����
	|-----------|------------|---------------------------------
	| matrix	|	36	 |	����ṹ������2������ռ�ӳ���ϵ
	|-----------|------------|---------------------------------
	| preview tm|	4	 |	Ԥ��ʱ�䣬��ʼԤ����ʱ��
	|-----------|------------|---------------------------------
	|preview dur|	4	 |	Ԥ��ʱ��
	|-----------|------------|---------------------------------
	|poster tm  |	4	 | the time of movie post
	|-----------|------------|---------------------------------
	|select tm  | 	4	 | the start time of current selection
	|-----------|------------|---------------------------------
	|select dur |	4 	 | the duration of current selection
	|-----------|------------|---------------------------------
	|current tm |	4	 | 	��ǰʱ��
	|-----------|------------|---------------------------------
	|next trakid|	4	 |	��һ������ӵ� trak id
	|-----------|------------|---------------------------------


	trak
	---------------------------------------------------------
	|	Trak header box 		tkhd 			��ѡ
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
	|	Media box			mdia			��ѡ
	---------------------------------------------------------
	|	User define data box	udta
	---------------------------------------------------------


	tkhd
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
	Media box ������track ��ý�����ͺ� sample ���ݣ�������Ƶ����Ƶ������ sample ���ݵ� media handler component��media timescale and track duration 
	�Լ� media-and-track-specific ��Ϣ������������ͼ��ģʽ����Ҳ���԰���һ�����ã�ָ��ý�����ݴ洢����һ���ļ��С�Ҳ���԰���һ�� sample table box��
	ָ�� sample description, duration, byte offset from the data reference for each media sample.
	Media box ��������'mdia'������һ������ box���������һ�� media header box ('mdhd')��һ�� handler reference ('hdlr')��һ��ý����Ϣ����('minf')���û�����atom('udta').

	mdia ----  Media box
	---------------------------------------------------------
	|	size
	---------------------------------------------------------
	|	type == mdia
	---------------------------------------------------------
	|	media header box			mdhd		��ѡ
	---------------------------------------------------------
	|	handler reference box		hdlr
	---------------------------------------------------------
	|	media information	box		minf
	---------------------------------------------------------
	|	user data box			udta
	---------------------------------------------------------


	mdhd
	Media header box ������ý������ԣ����� time scale �� duration������������ 'mdhd'.
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
	Handler reference box ������������ý�����ݵ� media handler component��������'hdlr'��
	�ڹ�ȥ��handler reference box Ҳ���������������ã��������ڣ��Ѿ�����������ʹ���ˡ�
	һ�� media box �ڵ� handler box ������ý�����Ĳ��Ź��̡����磬һ����Ƶ handler ����һ�� video track. 
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
	Mediainformation box �������� 'minf'���洢�˽��͸� track ��ý�����ݵ� handler-specific ����Ϣ��media handler ����Щ��Ϣ��ý��ʱ��ӳ�䵽ý�����ݣ������д���
	����һ������ box �������������� box��
	��Щ��Ϣ����ý�嶨������������ر��Ӧ�ģ����� media information box �ĸ�ʽ������Ҳ������ʹ�ý���������� media handler ������صġ�
	������ media handler ��֪����ν�����Щ��Ϣ��

	--minf
	   +--vmhd / smhd
	   +--dinf
	   +--stbl

	   Video  media information box ����Ƶý��ĵ�һ�� box�����������Ķ�����Ƶý�����ݵ����ԡ�  
	   Sound  media information box ����Ƶý��ĵ�һ�� box�����������Ķ�����Ƶý�����ݵ����ԡ�

	vmhd
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
	|	�ֶ�					|	����(�ֽ�)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == hdrl			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	balance				|	2	��Ƶ�ľ������������Ƽ�������������������������Ч����һ����0��һ��ֵ��0��
	---------------------------------------------------------
	|	reserved				|	2	�����ֶΣ�ȱʡΪ0
	---------------------------------------------------------

	dinf
	handler reference ���� data handler component ��λ�ȡý�����ݣ�data handler ����Щ������Ϣ������ý�����ݡ�
	Data information box ��������'dinf'������һ������ box�������������� box��
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == dinf			| 	4
	---------------------------------------------------------
			|	�ֶ�					|	����(�ֽ�)
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
					|	�ֶ�					|	����(�ֽ�)
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
      �洢ý�����ݵĵ�λ�� samples��һ�� sample ��һϵ�а�ʱ��˳�����е����ݵ�һ�� element��Samples �洢�� media �е� chunk �ڣ������в�ͬ�� durations��
      Chunk �洢һ�����߶�� samples�������ݴ�ȡ�Ļ�����λ�������в�ͬ�ĳ��ȣ�һ�� chunk �ڵ�ÿ�� sample Ҳ�����в�ͬ�ĳ��ȡ�
      ��������ͼ��chunk 2��3 ��ͬ�ĳ��ȣ�chunk 2�ڵ�sample 5��6 �ĳ���һ��������sample 4��5��6�ĳ��Ȳ�ͬ��

      --stbl, size 155659 bytes, Sample Table Box
          +---stsd, size 83 bytes, Sample Description Box
          +---stts, size 89856 bytes, Time-to-sample Box
          +---stsc, size 5060 bytes, Sample-to-Chunk Box
          +---stsz, size 58112 bytes, Sample size Box
          +---stco, size 2508 bytes, Chunk offset Box


	stsd
	sample description box ��������'stsd'��������һ�� sample description �����ݲ�ͬ�ı��뷽���ʹ洢���ݵ��ļ���Ŀ��
	ÿ�� media ������һ������� sample description��sample-to-chunk box ͨ������������ҵ����� medai ��ÿ�� sample �� description��
	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
			|	�ֶ�					|	����(�ֽ�)
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
      Time-to-sampleatoms �洢�� media sample �� duration ��Ϣ���ṩ��ʱ��Ծ��� data sample ��ӳ�䷽����ͨ����� box��������ҵ��κ�ʱ��� sample��������'stts'��
      ��� box ���԰���һ��ѹ���ı���ӳ��ʱ��� sample ��ţ��������ı����ṩÿ�� sample �ĳ��Ⱥ�ָ�롣
      ����ÿ����Ŀ�ṩ����ͬһ��ʱ��ƫ�������������� sample ��ţ� �Լ� samples ��ƫ������������Щƫ�������Ϳ��Խ���һ�������� time-to-sample ��.

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
      ����� samples �� media ʱ���� chunks ��֯��Щ sample���������Է����Ż����ݻ�ȡ��һ�� trunk ����һ������ sample��chunk �ĳ��ȿ��Բ�ͬ��
      chunk �ڵ� sample �ĳ���Ҳ���Բ�ͬ��sample-to-chunkatom �洢 sample �� chunk ��ӳ���ϵ��
	Sample-to-chunkatoms �������� 'stsc'����Ҳ��һ������ӳ�� sample �� trunk ֮��Ĺ�ϵ���鿴���ű��Ϳ����ҵ�����ָ�� sample �� trunk���Ӷ��ҵ���� sample��

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
	|	 1	   |	  1 	��    28	|	13	   | 28 * 13
	---------------|--------------------|--------------|----------------
	|	 2	   |		29		|	12	   |    12
	---------------|--------------------|--------------|----------------
	|	 3	   |    30  ��    57	|	13	   | (57 - 30) * 13
	---------------|--------------------|--------------|----------------
	|	 4	   |  	58		|	12	   |	  12
	---------------|--------------------|--------------|----------------
	|	 5	   |	  59  ��    86	|	12	   | (86 - 59) * 12
	--------------------------------------------------------------------


	stsz  ----   sample size atoms
      sample size box ������ÿ��sample �Ĵ�С������������'stsz'��������ý����ȫ�� sample ����Ŀ��һ�Ÿ���ÿ�� sample ��С�ı�������ý����������Ϳ���û�б߿�����ơ�

	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
	---------------------------------------------------------
	|	size					|	4
	---------------------------------------------------------
	|	type == stsz			| 	4
	---------------------------------------------------------
	|	version				|	1
	---------------------------------------------------------
	|	flags					| 	3
	---------------------------------------------------------
	|	sample size				|	4 	������е�sample����ͬ�ĳ��ȣ�����ֶξ������ֵ����������ֶε�ֵ����0����Щ���ȴ���sample size����
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
	Chunk  Offset Box ������ÿ�� trunk ��ý�����е�λ�ã�����������'stco'��λ�������ֿ��ܣ�32λ�ĺ�64λ�ģ����߶Էǳ���ĵ�Ӱ�����á�
	��һ������ֻ����һ�ֿ��ܣ����λ�����������ļ��еģ����������κ� box �еģ��������Ϳ���ֱ�����ļ����ҵ�ý�����ݣ������ý��� box��
	��Ҫע�����һ��ǰ��� box �����κθı䣬���ű�Ҫ���½�������Ϊλ����Ϣ�Ѿ��ı��ˡ�

	---------------------------------------------------------
	|	�ֶ�					|	����(�ֽ�)
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
	|	chunk offset 			�ֽ�ƫ�������ļ���ʼ����ǰchunk���������� chunk number ��������һ����ǵ�һ��trunk���ڶ�����ǵڶ���trunk
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
	 		|---cmov (Compressed Movie Atom)	//ѹ�����ĵ�Ӱ��Ϣ����
	 		|
	 		|
	 		|---rmra (Reference Movie Atom)	//�ο���Ӱ��Ϣ����
	 		|
	 		|
	 		|	//ȫ�ļ�Ψһ�ģ�һ���ļ���ֻ�ܰ���һ��mvhd box���������ļ���������ý��������ȫ���ȫ�ֵ�������
	 		|	//������ý��Ĵ������޸�ʱ��ʱ��̶ȡ�Ĭ��������ɫ��ʱ������Ϣ��
	 		|
		      |---mvhd (Movie Header Box) 		//���δѹ������ӰƬ��Ϣ��ͷ����	
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
		      |		|---rate (4byte) �����ٶ� (prefer_rate)
		      |		|---volume (2byte) ��������
		      |		|---reserved (10byte)
		      |		|---matrix (4byte * 9)
		      |		|---preview_time (4byte) Ԥ��ʱ��		\
		      |		|---preview_duration (4byte) Ԥ��ʱ��	|
		      |		|---poster_time (4byte)				|====> pre_defined (4byte * 6)
		      |		|---selection_time (4byte)			|
		      |		|---selection_duration (4byte)		|
		      |		|---current_time (4byte) ��ǰʱ��		/
		      |		|---next_trak_id (4byte)
		      |
		      |	// ��ͬ�� Track box ���໥�����ġ�ÿ��track box ��Я��������ʱ�������Ϣ��ͬʱ������ص� media box��
		      |	// track box �����֣�media track �� hint track��ǰ�����ڱ��� media �����Ϣ�����߰���������ý��Ĵ����Ϣ
		      |---track (Track Structure Box) (һ��mp4�ļ����ٰ���һ��)	//video_track | audio_track | subtitle_track ��ÿ��track���Ƕ����ģ������������������Եģ������Ҫ���������������档
		      |	|---Header
		      |	|	|---size (4byte)
		      |	|	|---type (4byte) ('track')
		      |	|
		      |	|---Data
		      |		|---tkhd (track header box) ��ѡ
		      |		|	|---Header (Full_Header) (12 byte)
		      |		|	|	|---size (4byte) ( Full_Header + Data )
		      |		|	|	|---type (4byte) ('tkhd')
		      |		|	|	|---version (1byte)
		      |		|	|	|---flags (3byte)
		      |		|	|
		      |		|	|---Data
		      |		|		|---create_time (4byte)
		      |		|		|---modification_time (4byte)
		      |		|		|---track_ID (4byte)			//����Ψһ�ı�ʾ��ǰtrack
		      |		|		|---reserved (4byte)
		      |		|		|---duration (4byte)			//���ڼ�¼��ǰtrack�Ĳ��ų���
		      |		|		|---reserved (8byte)
		      |		|		|---layer (2byte)
		      |		|		|---alternative_group (2byte)
		      |		|		|---volume (2byte)
		      |		|		|---reserved (2byte)
		      |		|		|---matrix_structure (36byte)
		      |		|		|---track_width (4byte)
		      |		|		|---track_height (4byte)
		      |		|
		      |		|---clip (clipping box) //ӰƬ������Ϣ
		      |		|---matt (track matte box)
		      |		|---edts (Edit atoms)	//�����˴���movie��һ��track��һ����ý�塣���е�edit����һ�������棬����ÿһ���ֵ�ʱ��ƫ�����ͳ��ȡ�Edit atoms ��������'edts'�����û�иñ����track�ᱻ�������š�һ���յ�edit����ƫ��track����ʼʱ�䡣
		      |		|	|---Header
		      |		|	|	|---size (4byte)
		      |		|	|	|---type (4byte) ('edts')
		      |		|	|
		      |		|	|---Data
		      |		|		|---elst (Edit list atom)	//����ӳ��movie��ʱ�䵽��track media��ʱ��
		      |		|			|---Header (Full_Header)
		      |		|			|	|---size (4byte) (Full_Header + Data)
		      |		|			|	|---type (4byte) ('elst')
		      |		|			|	|---version (1byte)
		      |		|			|	|---flags (3byte)
		      |		|			|
		      |		|			|---Data
		      |		|				|---entry_cout (4byte)
		      |		|				|---edit_list[3] (3 * 4byte)
		      |		|					|---track_duration (4byte)	//duration of this edit segment in units of the movie��s timescale.
		      |		|					|---time (4byte)		//starting time within the media of this edit segment (in mediatimescale units)��ֵΪ-1��ʾ�ǿ�edit��Track�е����һ��edit��Զ����Ϊ�ա�Any difference between the movie��s duration and the track��s duration is expressed as an implicit empty edit.
		      |		|					|---speed (4byte)		//relative rate at which to play the media corresponding to this edit segment��������0������
		      |		|
		      |		|---tref (track reference box)	//������track֮���ϵ�� ������Ƶ track �� ��Ƶ track������һ������Ƶ track
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
		      |		|		|	|---type (4byte) ('ssrc')	//Non-primary source. Indicates that the referenced track should send its data to this track, rather than presenting it. The referencing track will use the data to modify how it presents its data. See ��Track Input Map Atoms�� for more information.
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
		      |		|---mdia (media info container box) ��ѡ
		      |		|	|---Header
		      |		|	|	|---size (4byte)
		      |		|	|	|---type (4byte) ('mdia')
		      |		|	|
		      |		|	|---Data
		      |		|		|---mdhd (media header box) ��ѡ
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
		      |		|		|		|---vmhd (video media information header box) (��ѡһ) (video track only)
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
		      |		|		|		|---smhd (sound media information header box) (��ѡһ) (audio track only)
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
		      |		|		|				|---stsd (sample description box) (codec types, initialization etc.)	//��Ҫ������ǰtrack�йصı�����Ϣ���Լ����ڳ�ʼ������ĸ�����Ϣ��
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stsd')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---sample_description[] ��ͬ��ý�������в�ͬ��sample description������ÿ��sample description��ǰ�ĸ��ֶ�����ͬ�ģ��������µ����ݳ�Ա
		      |		|		|				|			|---size (4byte)
		      |		|		|				|			|---type (4byte) ('avc1')	//��Ƶ
		      |		|		|				|			|---reserved (6byte)
		      |		|		|				|			|---data_reference_index (2byte)	//ǰ4���ֶ���ͬ
		      |		|		|				|				|---version (2byte)			//box �汾, 0��1, һ��Ϊ0, (�����ֽ�������version=0)
		      |		|		|				|				|---Revision_level (2byte)
		      |		|		|				|				|---Vendor (4 byte)
		      |		|		|				|				|---Temporal_quality (4 byte)		//ʱ���ѹ��
		      |		|		|				|				|---Spatial_quality (4 byte)		//��Ƶ������
		      |		|		|				|				|---width (2byte)
		      |		|		|				|				|---height (2byte)
		      |		|		|				|				|---horizresolution (4byte)	//��ֱ�ֱ���
		      |		|		|				|				|---vertresolution (4byte)	//ˮƽ�ֱ���
		      |		|		|				|				|---Data_size (4byte)  	//A 32-bit integer that must be set to 0
		      |		|		|				|				|---frame_count (2byte)	//��Ƶ����Ϊ1	//A 16-bit integer that indicates how many frames of compressed data are stored in each sample. Usually set to 1.
		      |		|		|				|				|---compressorname (32byte) (string)	//A 32-byte Pascal string containing the name of the compressor that created the image, such as ��jpeg��
		      |		|		|				|				|---depth (2byte)		//��ʾѹ��ͼ���������ȵ�16λ������1, 2, 4��8, 16, 24��ֵ��32��ʾ��ɫͼ�����ȡ�ֻ����ͼ�����ʱ��ʹ��ֵ32��������ͨ�����Ҷ�ֵ�ֱ�Ϊ34, 36��40�ͱ�ʾ2��4��8λ�Ҷ�ֵ��ͼ��.
		      |		|		|				|				|---Color_table_ID (2byte)	//��ʶҪʹ�õ���ɫ���16λ�������������ֶα�����Ϊ- 1��Ĭ����ɫ��Ӧ����ָ����ȡ�����ÿ����16λ���µ���ȣ����ʾһ����׼��ָ����ȵ�Macintosh��ɫ�����Ϊ16, 24��32û����ɫ�������ɫ��ID����Ϊ0������ɫ�������ʾ�����������С���ɫ
		      |		|		|				|				|---size (4byte)
		      |		|		|				|				|---type (4byte) ('avcC')	//AVC sequence header  //AVCDecoderConfigurationRecord
		      |		|		|				|				|---Configuration_Version (1byte)
		      |		|		|				|				|---AVCProfile_Indication (1byte)
		      |		|		|				|				|---Profile_Compatibility (1byte)
		      |		|		|				|				|---AVCLevel_Indication   (1byte)
		      |		|		|				|				|---[reserved (6 bit) | lengthSizeMinusOne (2 bit)]   //NALUnitLength�ĳ���-1	//�ǳ���Ҫ���� H.264 ��Ƶ�� NALU �ĳ��ȣ����㷽���� 1 + (lengthSizeMinusOne & 3)��ʵ�ʼ�����һֱ��4
		      |		|		|				|				|---[reserved (3 bit) | numOfSequenceParameterSets (5 bit)]   //sps����, һ��Ϊ1  //SPS �ĸ��������㷽���� numOfSequenceParameterSets & 0x1F��ʵ�ʼ�����һֱΪ1
		      |		|		|				|				|---sps_len (2byte)
		      |		|		|				|				|---sequenceParameterSetNALUnits[] (sps_len byte)
		      |		|		|				|				|---numOfPictureParameterSets (1 byte)
		      |		|		|				|				|---pps_len (2 byte)
		      |		|		|				|				|---pictureParameterSetNALUnit[] (pps_len byte)
		      |		|		|				|			|---size (4byte)
		      |		|		|				|			|---type (4byte) ('mp4a') 	//��Ƶ
		      |		|		|				|			|---reserved (6byte)
		      |		|		|				|			|---data_reference_index (2byte)	//ǰ4���ֶ���ͬ
		      |		|		|				|				|---reserved (4byte)
		      |		|		|				|				|---reserved (4byte)
		      |		|		|				|				|---channels (2byte)
		      |		|		|				|				|---sample_size (2byte)	//Ĭ��16������Ϊ8 
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
		      |		|		|				|			|---sample_count (4byte)  ����ͬduration������sample����Ŀ
		      |		|		|				|			|---sample_duration (4byte) ÿ��sample��duration
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
		      |		|		|				|			|---samples_per_chunk (4byte)		ÿ�� trunk �ڵ� sample ��Ŀ
		      |		|		|				|			|---sample_description_index (4byte)  ����Щ sample ������ sample description �����
		      |		|		|				|
		      |		|		|				|---stsz (sample size box)
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stsz')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---sample_size (4byte) ������е�sample����ͬ�ĳ��ȣ�����ֶξ������ֵ���� entry_size[] �ֶβ����ڣ���������ֶε�ֵ����0����Щ���ȴ��� entry_size[] ����
		      |		|		|				|		|---sample_count (4byte) (number_of_entries)
		      |		|		|				|		|---entry_size[] ( uint32[], varies)
		      |		|		|				|			|---sample_size (4byte) ÿ��sample�Ĵ�С, �� sample ˳������
		      |		|		|				|
		      |		|		|				|---stco (chunk offset box) ������ÿ�� trunk ��ý�����е�λ��
		      |		|		|				|	|---Header (Full_Header)
		      |		|		|				|	|	|---size (4byte)
		      |		|		|				|	|	|---type (4byte) ('stco')
		      |		|		|				|	|	|---version (1byte)
		      |		|		|				|	|	|---flags (3byte)
		      |		|		|				|	|
		      |		|		|				|	|---Data
		      |		|		|				|		|---entry_cout (4byte) (number_of_entries)
		      |		|		|				|		|---chunk_offset_table[] (uint32[], varies)
		      |		|		|				|			|---chunk_offset (4byte) �ֽ�ƫ�������ļ���ʼ����ǰchunk
		      |		|		|				|
		      |		|		|				|---stss (sync sample box) ȷ��media�еĹؼ�֡
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
	 |---mdat (movie data container) //ʵ��ý�����ݡ��������ս��벥�ŵ����ݶ��������档
	 	|---Header
	 	|	|---size (4byte)
	 	|	|---type (4byte) ('mdat')
	 	|
	 	|---Data (ʵ��ý������)

*/

/*	Seek
	1��ʹ�� timescale ��Ŀ��ʱ���׼����
	2��ͨ��(stts)time-to-sample box�ҵ�ָ��track�ĸ���ʱ��֮ǰ�ĵ�һ��sample number��
	3��ͨ��(stss)sync sample table��ѯsample number֮ǰ�ĵ�һ��sync sample��
	4��ͨ��(stsc)sample-to-chunk table���ҵ���Ӧ��chunk number��
	5��ͨ��(stco)chunk offset box���ҵ���Ӧchunk���ļ��е���ʼƫ������
	6�����ʹ��(stsc)sample-to-chunk box��(stsz)sample size box����Ϣ�������chunk����Ҫ��ȡ��sample���ݣ������seek��
*/

#endif
