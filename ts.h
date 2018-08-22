#ifndef __TS__H__
#define __TS__H__

/*
1. ����ts��Ҫ���ҵ�PAT����ֻҪ�ҵ�PAT�Ϳ����ҵ�PMT��Ȼ��Ϳ����ҵ�����Ƶ���ˡ�PAT����PIDֵ�̶�Ϊ0��
2. PAT����PMT����Ҫ���ڲ���ts������Ϊ�û���ʱ���ܼ���ts�����������Ƚ�С��ͨ��ÿ��������Ƶ֡��Ҫ����PAT��PMT��
3. PAT��PMT���Ǳ���ģ������Լ�����������SDT��ҵ�����������ȣ�����hls��ֻҪ��PAT��PMT�Ϳ��Բ����ˡ�
4. PAT��������Ҫ�����þ���ָ����PMT����PIDֵ��
5. PMT��������Ҫ�����þ���ָ��������Ƶ����PIDֵ��

6. PID��TS����Ψһʶ���־��Packet Data��ʲô���ݾ�����PID�����ġ�

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
	PAT��ʽ(Program Association Table����Ŀ������)

----------------------------------------------------------------------------------------------------
1. table_id				8bit		PAT���̶�Ϊ0x00
----------------------------------------------------------------------------------------------------1byte
2. section_syntax_indicator	1bit		�̶�Ϊ1
----------------------------------------------------------------------------------------------------
3. zero				1bit		�̶�Ϊ0
----------------------------------------------------------------------------------------------------
4. reserved				2bit		�̶�Ϊ11
----------------------------------------------------------------------------------------------------
5. section_length			12bit		�������ݵĳ���, �� transport_stream_id �� CRC32 �ĳ���
----------------------------------------------------------------------------------------------------2byte
6. transport_stream_id		16bit		������ID���̶�Ϊ0x0001
----------------------------------------------------------------------------------------------------2byte
7. reserved				2bit		�̶�Ϊ11
----------------------------------------------------------------------------------------------------
8. version_number			5bit		�汾�ţ��̶�Ϊ00000�����PAT�б仯��汾�ż�1
----------------------------------------------------------------------------------------------------
9. current_next_indicator	1bit		�̶�Ϊ1����ʾ���PAT�������ã����Ϊ0��Ҫ�ȴ���һ��PAT��
----------------------------------------------------------------------------------------------------1byte
10. section_number		8bit		�̶�Ϊ0x00
----------------------------------------------------------------------------------------------------1byte
11. last_section_number		8bit		�̶�Ϊ0x00
----------------------------------------------------------------------------------------------------1byte
��ʼѭ��	 	 
====================================================================================================
12. program_number		16bit		��Ŀ��Ϊ 0x0000 ʱ,��ʾ���� NetWork_ID		--NIT
							��Ŀ��Ϊ 0x0001 ʱ,��ʾ���� Program_Map_PID	--PMT
----------------------------------------------------------------------------------------------------2byte
13. reserved			3bit		�̶�Ϊ111
----------------------------------------------------------------------------------------------------
14. PID				13bit		��Ŀ�Ŷ�Ӧ���ݵ� NetWork_ID ֵ
    NIT 						��Ŀ�Ŷ�Ӧ���ݵ� Program_Map_PID ֵ
====================================================================================================2byte
����ѭ��	 	 
----------------------------------------------------------------------------------------------------
15. CRC32				32bit		ǰ�����ݵ�CRC32У����
----------------------------------------------------------------------------------------------------4byte

*/


/*
	PMT��ʽ( Program Map Table����Ŀӳ��� )
	
----------------------------------------------------------------------------------------------------
1. table_id				8bit		PMT��ȡֵ���⣬0x02
----------------------------------------------------------------------------------------------------1byte	0x02
2. section_syntax_indicator	1bit		�̶�Ϊ1
----------------------------------------------------------------------------------------------------
3. zero				1bit		�̶�Ϊ0
----------------------------------------------------------------------------------------------------
4. reserved				2bit		�̶�Ϊ11
----------------------------------------------------------------------------------------------------
5. section_length			12bit		�������ݵĳ���   �γ���,��program_number��CRC_32(��)���ֽ�����
----------------------------------------------------------------------------------------------------2byte	0xb0 0x17
6. program_number			16bit		��Ŀ�ţ���ʾ��ǰ�� PMT �������� ��Ŀ�ţ�ȡֵ0x0001
----------------------------------------------------------------------------------------------------2byte	0x00 0x01
7. reserved				2bit		�̶�Ϊ11
----------------------------------------------------------------------------------------------------
8. version_number			5bit		�汾�ţ��̶�Ϊ00000�����PAT�б仯��汾�ż�1
							���PMT�����и���,���������1֪ͨ�⸴�ó�����Ҫ���½��ս�Ŀ��Ϣ
----------------------------------------------------------------------------------------------------
9. current_next_indicator	1bit		�̶�Ϊ1
----------------------------------------------------------------------------------------------------1byte	0xc1
10. section_number		8bit		�̶�Ϊ0x00
----------------------------------------------------------------------------------------------------1byte	0x00
11. last_section_number		8bit		�̶�Ϊ0x00
----------------------------------------------------------------------------------------------------1byte	0x00
12. reserved			3bit		�̶�Ϊ111
----------------------------------------------------------------------------------------------------
13. PCR_PID				13bit		PCR (��Ŀ�ο�ʱ��) ����TS�����PID��ָ��Ϊ��ƵPID
----------------------------------------------------------------------------------------------------2byte	0xe1 0x00
14. reserved			4bit		�̶�Ϊ1111
----------------------------------------------------------------------------------------------------
15. program_info_length		12bit		��Ŀ������Ϣ��ָ��Ϊ0x000��ʾû��
							��Ŀ��Ϣ����(֮�����N���������ṹ,һ����Ժ��Ե�,����ֶξʹ����������ܵĳ���,��λ��Bytes)
							�����ž���Ƶ���ڲ������Ľ�Ŀ���ͺͶ�Ӧ��PID������
----------------------------------------------------------------------------------------------------2byte	0xf0 0x00
��ʼѭ��
====================================================================================================
16. stream_type			8bit		�����ͣ���־�� Video ���� Audio ������������
							h.264 �����Ӧ 0x1b
							aac   �����Ӧ 0x0f
							mp3   �����Ӧ 0x03
----------------------------------------------------------------------------------------------------1byte	0x1b			0x0f
17. reserved			3bit		�̶�Ϊ111
----------------------------------------------------------------------------------------------------
18. elementary_PID		13bit		�� stream_type ��Ӧ�� PID
----------------------------------------------------------------------------------------------------2byte	0xe1 0x00		0xe1 0x01
19. reserved			4bit		�̶�Ϊ1111
----------------------------------------------------------------------------------------------------
20. ES_info_length		12bit		������Ϣ��ָ��Ϊ 0x000 ��ʾû��
====================================================================================================2byte	0xf0 0x00		0xf0 0x00
����ѭ��
----------------------------------------------------------------------------------------------------
21. CRC32				32bit		ǰ�����ݵ�CRC32У����
----------------------------------------------------------------------------------------------------4byte	0x2f 0x44 0xb9 0x9b

*/


/*
    ////////////////////////////////////////////////////////////////////////////////////////////
								ts header
    ////////////////////////////////////////////////////////////////////////////////////////////
*/
/*
TS Header ts���̶����� 188 byte

------------------------------------------------------------------------------------
1. 	Sync_Byte 			  | 8bit |	�̶�Ϊ 0x47
	ͬ���ֽ�    		  |	   |
--------------------------------|------|------------------------------------------------------1byte
2. Transport_Error_Indicator 	  | 1bit |  ������tsͷ��adapt�����һ�������ֽڣ�ͨ����Ϊ0������ֽ�����adapt�򳤶���
	�������ָʾ��         	  |	   |
--------------------------------|------|----------------------------------------------
3. Payload_Unit_Start_Indicator | 1bit |	 һ�����������ݰ���ʼʱ���Ϊ1
	���ص�Ԫ��ʼ��ʾ��	  |      |	��ǰ4���ֽں����һ�������ֽڡ�����ʵ������Ӧ��Ϊȥ����һ���ֽں�����ݡ�Data����ȥ����һ���ֽ�
--------------------------------|------|----------------------------------------------
4. Transport_Priority 		  | 1bit |	0Ϊ�����ȼ���1Ϊ�����ȼ���ͨ��ȡ0
	�������ȼ�			  |      |
--------------------------------|------|----------------------------------------------
5. 		Pid			  | 13bit|	pidֵ(Packet ID���룬Ψһ�ĺ����Ӧ��ͬ�İ�)
--------------------------------|------|------------------------------------------------------2byte
6. Transport_Scrambling_Control | 2bit |	00��ʾδ����
	������ſ���		  |	   |
--------------------------------|------|---------------------------------------------
7. Adaptation_Field_Control 	  | 2bit |	��00����������01��Ϊ������Ӧ�򣬽�����Ч���أ�
	�Ƿ��������Ӧ��		  |	   |			��10��Ϊ��������Ӧ������Ч���أ�
					  |	   |  		��11��Ϊͬʱ��������Ӧ�����Ч���ء�
--------------------------------|------|---------------------------------------------
8. Continuity_Counter 		  | 4bit |	 ��0-f����ʼֵ��һ��ȡ0����������������
	����������			  |	   |
---------------------------------------------------------------------------------------------- 1byte
9. AdaptationField 
	1. Adaptation_Field_Length (8bit)			����Ӧ�򳤶ȣ�������ֽ��� (���������ֶε�1byte)
	---------------------------------------------------------------------------------------- 1byte
	2. Discontinue_Indicator  (1bit)			������ָʾ��		|
	3. Random_Access_Indicator (1bit)   		�����ȡָʾ��		|
	4. Elementary_Streem_Priority_Indicator(1bit)	���������ȼ�ָʾ��	|
	5. 5Flags											|
		1. PCR_Flag (1bit)								|  flags == 0x00 ʱ, 
		2. OPCR_Flag (1bit)								|	����Ӧ�ֶ�Ϊ����ֶ�
		3. Splicing_Point_Flag (1bit)							|		ȡֵΪ 0xFF
		4. Trasport_Private_Data_Flag (1bit)					|		����Ϊ ����Ӧ���� (Adaptation_Field_Length)
		5. Adaptation_Field_Extension_Flag (1bit)					|
	---------------------------------------------------------------------------------------- 1byte
	6. OptionFileds 						��ѡ�ֶ�				�����ʱ�������ѡ�ֶκ���
		1. PCR (42bit)
		2. OPCR (42bit)
		3. ƴ�ӵ��� (8bit)
		4. ����ר�����ݳ��� (8bit)
		5. ����ר������
		6. ����Ӧ�ֶ���չ���� (8bit)
		7. 3����־
		8. ��ѡ�ֶ�
			1. ltw ��Ч��־ (1bit)
			2. ltw ����
			3. (2bit)
			4. �ֶ����� (22bit)
			5. ƴ������ (4bit)
			6. DTS_next_au (33bit)
	----------------------------------------------------------------------------------------
	7. Stuffing_bytes						����ֽ�		flags == 0x00, 0xFF
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
1. ����ʼ��ǰ׺(24bit) 	| 			3byte 	0x00 0x00 0x01
------------------------------------------------------------------------------------3byte
2. ��id(8bit) 		| 			1byte
------------------------------------------------------------------------------------1byte
3. PES������(16bit) 	| 			2byte		0 ֵָʾPES �����ȼ�δָʾҲδ�޶�
				|						-- ��PES ������Ч�غ������Դ�������������������Ƶ���������ֽ����
------------------------------------------------------------------------------------2byte           ================> 6byte
4. ������������Ϣ(3-259byte)  | 
	1. '10' 			| 
	2. PES���ſ���(2bit) 	| 
	3. PES���ȼ�(1bit) 	| 		1byte		--val 0x80 | 0x04   1 0 0 0 - 0 1 0 0
	4. ���ݶ�λָʾ��(1bit) | 
	5. �汾(1bit) 		| 
	6. ԭʼ�Ļ��Ƶ�(1bit) | 
	------------------------------------------------------------------------------1byte
	7. 7����־λ(8bit) 			  	| 	1byte		--flags
		1. PTS_DTS_flags(2bit) 		  	| 	11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS
		2. ESCR_flag(1bit) 		  	|	1��ʾ�ײ���ESCR�ֶΣ�0���޴��ֶ�
		3. ES_rate_flag(1bit) 		  	|	ռλ1bit��1��ʾ�ײ��д��ֶΣ�0�޴��ֶ�
		4. DSM_trick_mode_flag(1bit) 	  	| 	ռλ1bit��1��ʾ��8λ��DSM_trick_mode_flag�ֶΣ�0�޴��ֶ�
		5. Additional_copy_info_flag(1bit) 	| 	ռλ1bit��1��ʾ�ײ��д��ֶΣ�0��ʾ�޴��ֶΣ�
		6. PES_CRC_flag(1bit) 		  	| 	ռλ1bit����1��ʾPES������CRC�ֶΣ�0�޴��ֶΣ�
		7. PES_extension_flag(1bit) 	  	|	ռλ1bit����չ��־λ����1��ʾ����չ�ֶΣ�0�޴��ֶΣ�
	------------------------------------------------------------------------------1byte
	8. PESͷ���ݳ���(8bit) | 		1byte			--header_len  ��������ݳ���
	------------------------------------------------------------------------------1byte         ================> 9byte
	9. ��ѡ�ֶ� | 
		1. PTS/DTS(33bit) 		| 	PTS��DTS������������40bit��ȡ33λ����ʽ��ͬ��
			1. start_code����ʼ�룬ռλ4bit����PTS_DTS_flags == ��10������˵��ֻ��PTS����ʼ��Ϊ0010��
				��PTS_DTS_flags == ��11������PTS��DTS�����ڣ�PTS����ʼ��Ϊ0011��DTS����ʼ��Ϊ0001��(PTS����ʼ���2��bit��flag��ͬ)
			2. PTS[32..30]��ռλ3bit��
			3. marker_bit��ռλ1bit��
			4. PTS[29..15]��ռλ15bit��
			5. marker_bit��ռλ1bit��
			6. PTS[14..0]��ռλ15bit��
			7. marker_bit��ռλ1bit��
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
		2. ESCR(48bit) 			|  ��33bit��ESCR_base�ֶκ�9bit��ESCR_extension�ֶ���ɣ�ESCR_flag == 1ʱ���ֶδ��ڣ�
			1. Reserved�������ֶΣ�		ռλ2bit��
			2. ESCR_base[32..30]��		ռλ3bit��
			3. marker_bit��			ռλ1bit��
			4. ESCR_base[29..15]��		ռλ15bit��
			5. marker_bit��			ռλ1bit��
			6. ESCR_base[14..0]��		ռλ15bit��
			7. marker_bit��			ռλ1bit��
			8. ESCR_extension��(UI)		ռλ9bit����������ȡֵ��Χ0~299��ѭ��һ�Σ�base+1��
			9. marker_bit��			ռλ1bit��
		3. ES����(24bit) 			|  Ŀ�����������PES�����ֽ����ʣ���ֹΪ0��ռλ24bit��ES_rate_flag == 1ʱ���ֶδ��ڣ�
			1. marker_bit��	ռλ1bit��
			2. ES_rate��	ռλ22bit��
			3. marker_bit��	ռλ1bit��
		4. DSM�ؼ���ʽ(8bit) 		|  ��ʾ����trick mode��Ӧ������Ӧ����Ƶ����ռλ8��bit��DSM_trick_mode_flag == 1ʱ���ֶδ��ڣ�
								����trick_mode_controlռǰ3��bit��������ֵ������5��bit�Ĳ�ͬ���ݣ�
			1. ���trick_mode_control == ��000���������ֽ�˳��Ϊ��
				1. field_id��ռλ2bit��
				2. intra_slice_refresh ��ռλ1bit��
				3. frequency_truncation��ռλ2bit��
			2. ���trick_mode_control == ��001���������ֽ�˳��Ϊ��
				1. rep_cntrl��ռλ5bit��
			3. ���trick_mode_control == ��010���������ֽ�˳��Ϊ��
				1. field_id��ռλ2bit��
				2. Reserved��ռλ3bit��
			4. ���trick_mode_control == ��011���������ֽ�˳��Ϊ��
				1. field_id��ռλ2bit��
				2. intra_slice_refresh��ռλ1bit��
				3. frequency_truncation��ռλ2bit��
			5. ���trick_mode_control== ��100���������ֽ�˳��Ϊ��
				1. rep_cntrl��ռλ5bit��
			6. ����������ֽ�˳��Ϊ��
				1. reserved ��ռλ5bit��
		5. ���ӵĸ�����Ϣ(8bit) 	|  ռ8��bit��Additional_copy_info_flag == 1ʱ���ֶδ��ڣ�
			1. marker_bit��ռλ1bit��
			2. copy info�ֶΣ�ռλ7bit����ʾ�Ͱ�Ȩ��ص�˽�����ݣ�
		6. ǰPES_CRC(16bit) 		|  ռλ16bit�ֶΣ�����CRCֵ��PES_CRC_flag == 1ʱ���ֶδ��ڣ�
		7. PES��չ				|  PES��չ�ֶΣ�PES_extension_flag == 1ʱ���ֶδ��ڣ�
			1. 5����־ | 
				1. PES_private_data_flag��				ռλ1bit����1��ʾ��˽�����ݣ�0���ޣ�
				2. Pack_header_field_flag��				ռλ1bit����1��ʾ��Pack_header_field�ֶΣ�0���ޣ�
				3. Program_packet_sequence_counter_flag��		ռλ1bit����1��ʾ�д��ֶΣ�0���ޣ�
				4. P-STD_buffer_flag��					ռλ1bit����1��ʾ��P-STD_buffer�ֶΣ�0���޴��ֶΣ�
				5. Reserved�ֶΣ�						3��bit��
				6. PES_extension_flag_2��				ռλ1bit����1��ʾ����չ�ֶΣ�0���޴��ֶΣ�
			2. ��ѡ�ֶ�|
				1. PESר������(128bit) |	˽���������ݣ�ռλ128bit��PES_private_data_flag == 1ʱ���ֶδ��ڣ� 
				2. ��ͷ�ֶ�(8bit) | 		Pack_header_field_flag == 1ʱ���ֶδ��ڣ��ֶ����˳�����£�
					1. Pack_field_length�ֶΣ�(UI)ָ�������field�ĳ��ȣ�ռλ8bit��
				3. ��Ŀ��˳�����(16bit) |   �������ֶΣ�16��bit����flag�ֶ�Program_packet_sequence_counter_flag == 1ʱ���ֶδ��ڣ��ֽ�˳������Ϊ��
					1. marker_bit��					ռλ1bit��
					2. packet_sequence_counter�ֶΣ�(UI)	ռλ7bit��
					3. marker_bit��					ռλ1bit��
					4. MPEG1_MPEG2_identifier��			ռλ1bit����λ1��ʾ��PES���ĸ�������MPEG1������λ0��ʾ��PES���ĸ�������PS����
					5. original_stuff_length��(UI)		ռλ6bit����ʾPESͷ������ֽڳ��ȣ�
				4. P-STD������(16bit) |  ��ʾP-STD_buffer���ݣ�ռλ16bit��P-STD_buffer_flag == '1'ʱ���ֶδ��ڣ��ֽ�˳������Ϊ��
					1. '01' �ֶΣ�ռλ2bit��
					2. P-STD_buffer_scale��ռλ1bit����ʾ�������ͺ���P-STD_buffer_size�ֶεı������ӣ�
						���֮ǰ��stream_id��ʾ��Ƶ�������ֵӦΪ0����֮ǰ��stream_id��ʾ��Ƶ�������ֵӦΪ1����������stream���ͣ���ֵ����0��1��
					3. P-STD_buffer_size��ռλ13bit���޷������������ڻ��������P-STD���뻺������СBSn�����ֵ��
						��P-STD_buffer_scale == 0����P-STD_buffer_size��128�ֽ�Ϊ��λ����P-STD_buffer_scale == 1����P-STD_buffer_size��1024�ֽ�Ϊ��λ��
				5. PES��չ�ֶγ���(8bit) |  ��չ�ֶε���չ�ֶΣ�ռ��N*8��bit��PES_extension_flag_2 == '1'ʱ���ֶδ��ڣ��ֽ�˳������Ϊ��
					1. marker_bit��ռλ1bit��
					2. PES_extension_field_length��ռλ7bit����ʾ��չ����ĳ��ȣ�
				6. PES��չ�ֶ����� | Reserved�ֶ� | ռλ8*PES_extension_field_length��bit��
	------------------------------------------------------------------------------
	10.����ֽ�(0xFF) | ����ֶΣ��̶�Ϊ0xFF�����ܳ���32���ֽڣ�
------------------------------------------------------------------------------------
5. �ɱ䳤���ݰ�(���65536byte) | PES_packet_data_byte��PES�������е����ݣ���ESԭʼ�����ݣ�
------------------------------------------------------------------------------------

���ݶ�λָʾ��(1bit): 1 indicates that the PES packet header is immediately followed by the video start code or audio syncword

*/


/*

һ����һ��TS��   ��һ֡����(Ҳ����һ��PES��),����119756���ֽ�,һ����637��TS��(119756 / 188 = 637).

1. TS Header : 47 41 01 30 
2. TS Header AdaptationField : 07 10 00 07 24 00 7E 00

3. PES Header : 00 00 01 E0 00 00 
4. PES Optional Header : 80 C0 0A 31 00 39 EE 05 11 00 39 90 81

	//(AUD -> 09 & 0x1f == 9)   //��ÿһ֡����Ƶ֡�������pes��ʱ���俪ͷһ��Ҫ���� 00 00 00 01 09 F0 ���NALU AUD. 
5. NALU AUD : 00 00 00 01 09 F0 (��ѡ) �ֽ��

	//(SEI -> 06 & 0x1f == 6)     //Supplementary Enhancement Information //������ǿ��Ϣ (�ɱ䳤)
6. NALU Delimiter : 00 00 00 01 	//NALU �ָ���
7. NALU Unit : 06 00 07 80 D8 31 80 87 0D C0 01 07 00 00 18 00 00 03 00 04 80 
00 

	//(SPS -> 27 & 0x1f == 7) (�ɱ䳤)  (SPS -> 67 & 0x1f == 7)
8. NALU Delimiter : 00 00 00 01 
9. NALU Unit : 27 64 00 28 AC 2B 60 3C 01 13 F2 E0 22 00 00 03 00 02 00 00 03 
00 3D C0 80 00 64 30 00 00 64 19 37 BD F0 76 87 0C B8 00 


	//(PPS-> 28 & 0x1f == 8) (�ɱ䳤)  (PPS-> 68 & 0x1f == 8)
10. NALU Delimiter : 00 00 00 01 
11. NALU Unit : 28 EE 3C B0 


	//(IDR Frame -> 41 & 0x1f == 1)   i ֡

	//(IDR Frame -> 25 & 0x1f == 5)   (IDR Frame -> 65 & 0x1f == 5)
12. NALU Delimiter : 00 00 00 01 
13. NALU Unit : 25 88 80 0E 3F D5 2E 71 35 C8 A5 E1 CE F4 89 B3 F2 CA D2 65 75 33 63 B1 BA B6 33 B0 7B 80 A8 26 D0 77 01 FF 9A CB 85 C7 D1 DC A8 22 E9 BE 10 89 F9 CF 1A BA 6D 12 3D 19 0C 77 33 1B 7C 03 9B 3D F1 FF 02 AB C6 73 8A DB 51 

���� �ڶ���TS������֡�����ڶ���TS��:

�м��TS��(��2��-��636��),�̶��ĸ�ʽ:
TsPacket=TsHeader(4bytes)+TsPayload(184bytes)
TsPacket=TsHeader(4bytes)+TsPayload(184bytes)
Ψһ�仯�ľ���TsHeader�е��ֶ�ContinuityCounter,��0-15ѭ���仯.


�������һ��TS�� = TSͷ + TS����Ӧ�ֶ� + ����ֶ� + TS Payload

*/




#endif