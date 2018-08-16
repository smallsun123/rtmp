
#ifndef _H_264_H_
#define _H_264_H_

/*

    ֡��Ԥ��ģʽȫ�����㷨

    < Step 1-3, ���� ���� 4x4�� �� 9��ģʽ ��Cost, ѡȡ��С Cost_4x4
    < Step 5-7, ���� 16x16�� �ĵ���ģʽ�� Cost_16x16

    Step l���Ե�ǰ 4X4���ȿ� ѡ�� һ��ģʽ ����Ԥ�⣬���� ԭʼ�� �� Ԥ��� �� �в�ֵ;

    Step 2������ SAD���� �� RDO���� �����ģʽ�Ĵ���ֵ Cost_4x4;

    Step 3: �Ե�ǰ 4X4�� �ظ����� 1��2, ֱ�� 9�� Ԥ��ģʽ ��������, ѡ�� ��С Cost_4x4;

    Step 4: �Ե�ǰ����ڵ� 16�� 4X4�� �ظ����� l��3, Ȼ������� Cost_4x4 ���;

    Step 5: �� 16X16��� ѡ�� һ��ģʽ ����Ԥ��, �� �в��� ��ÿ�� 4��4�� ������ Hadamard�任;

    Step 6: �ӱ任��� 16X16��� �зֱ���ȡ 16�� 4X4�� DCϵ����1��4 �γ���һ�� 4X4��, �ٶ� DCϵ���� ���� Hadamard�任;

    Step 7�������� Hadamard�任ϵ���� ����ֵ ���, ȡ �͵�l��2 ��Ϊ ���ۺ���ֵ Cost_16x16;

    Step 8: �ظ����ϲ���� 5��7, ֱ���� 16X16��� �� 4��Ԥ��ģʽ ��������, ѡ����С�Ĵ��ۺ���ֵ Cost_16x16;

    Step 9���Ƚ����ϲ���4,8, �Ĵ��ۺ���ֵ

                                   i=16
                if ( Cost_16x16 >=  ��Cost_4x4_i + 24��(QP) )
                                   i=1
                {
                    ��ǰ���ѡȡʹ 4X4����ֵ��С�� Ԥ��ģʽ ���б���;
                }
                else
                {
                    ѡȡʹ16��16����ֵ��С��Ԥ��ģʽ���б���;
                }

    Step 10������8��8ɫ�ȿ�, ���ǽ� 4�� Ԥ��ģʽ �е� ÿһ�� ɫ�� Ԥ��ģʽ �ظ�һ���������в���, �� 8��8ɫ�ȿ� ��Ԥ�� ���������ѭ��;


    �ɴ˿ɼ���һ������ڵ�ģʽ�����Ϊ:

        M_8 x ( M_4 x 16 + M_16 ),  M_8 = 4, M_4 = 9, M_16 = 4;

    ��Ҫ���� 592�� ��ͬ�� ���ۺ��� �Եõ� ���� ɫ��ģʽ �� ���� ���Ⱥ��ģʽ ����ϡ�
                
*/


////////  ֡��Ԥ������㷨
/*

    �ռ����ڿ���ص�֡��Ԥ��

    
    һ�� ��������򵥡�ƽ̹��ͼ����ʺϰ��� 16��16 ��С����Ԥ�⣬
         ���������ӡ�ϸ�ڷḻ��ͼ�������ʺϰ��� 4��4 ��С����Ԥ�⡣
         
    ���� �ж�ͼ���ϸ���Ƿ�ḻ�����Ǽ���ͼ�������ػҶ�ֵ��ҶȾ�ֵ��ƫ��̶ȣ�����ȡ����������ʾ�������ֵ��

                  1  i=16 j=16         _
            V = ---- ��   ��  (x_i,j - x)^2
                 16  i=1  j=1


            ��Ϊ DCģʽ�����з����ԣ�������ѡ���ѡģʽʱ��Ӧ���ǵ� DCģʽ��
            
                     1  i=4 j=4          _
            V_dc = ---- ��  ��  (x_i,j - x)^2
                     4  i=1 j=1

    ���� ����ͼ

        1) ���㵱ǰ 16x16 ��� V ֵ

        if ( V <= TV1 ) //4 ѡ 1
        {
            16x16 ģʽԤ��,  ===> 16x16 �����Ԥ��ģʽ��Ϊ ��ǰ���Ԥ��ģʽ
        } 
        else if ( TV1 < V <= TV2 ) // 13 ѡ 1
        {
            16x16 ģʽԤ��, 4x4 ģʽԤ��,  ===> �Ƚ� 4x4, 16x16 Ԥ������ģʽ�� RDCose ֵ, ��С��Ϊ ��ǰ���Ԥ��ģʽ
        } 
        else if ( V > TV2 ) //9 ѡ 1
        {
            4x4 ģʽԤ��,   ===> 4x4 �����Ԥ��ģʽ��Ϊ ��ǰ���Ԥ��ģʽ
        }


    �ġ� ���Ԥ�� ����ͼ

        1) ���ݵ�ǰ������ڿ��ģʽ ModeA �� ModeB ���㵱ǰ������ܵ�Ԥ��ģʽ MPM (ModeA, ModeB)

        if ( V_dc <= Tdc )
        {
            �� DCģʽ Ϊ��ѡԤ��ģʽ, ===> ���� MPM (ModeA, ModeB) �� Mode_DC �� RDCost, ȡ��Сֵ�Ķ�ӦģʽΪ ��ǰ 4x4�� ��Ԥ��ģʽ
        }
        else 
        {
            ���� 8�� ������ģ�� Dֵ, ������Сֵ��Ӧ��ģʽ��Ϊ Mode_D,  ===> 
            ���� MPM (ModeA, ModeB) �� Mode_D �� RDCost, ȡ��Сֵ�Ķ�ӦģʽΪ ��ǰ 4x4�� ��Ԥ��ģʽ
        }
*/

/*
    һ�� ��������ָ�ӱ�������ȡ�䷨Ԫ�صķ��������䷨Ԫ�صĽ����㷨
            ���� H.264 
��������һ�����ر��룬��������������Ӵ�����ر���Ľ����㷨

    H.264���������¼��������� :
        a)  ae(v) ��������������Ӧ�Ķ����� �����ر���
        b)  b(8) ���������� 8 ������
        c)  ce(v) ��������������Ӧ�� �ɱ䳤�ر���
        d)  f(n) ���������� n ������
        e)  i(n)/i(v) �������������ɱ��أ��������ǽ���Ϊ�з�������
        f)  me(v) ӳ��ָ�� Golomb �ر���
        g)  se(v) �з���ָ�� Golomb �ر���
        h)  te(v) �ض�ָ�� Golomb �ر���
        i)  u(n)/u(v) �������������ɱ��أ��������ǽ���Ϊ�޷�������
        j)  ue(v)  �޷���ָ�� Golomb �ر���
    
    1. ue(v)���޷�������ָ�����ײ��������﷨Ԫ�أ���λ����
    2. se(v)���з�������ָ�����ײ��������﷨Ԫ�أ���λ����
    3. u(n)��nλ�޷�������
    4. ���﷨����, ���n��'v', ��������������﷨Ԫ��ֵȷ��. ���������ɺ� �� 
read_bits(n) �ķ���ֵ�涨, �÷���ֵ�������Чλ��ǰ�Ķ����Ʊ�ʾ
*/

/*
nal_unit_type       NAL ����                 C
0                   δʹ�� 
1                   ���������� IDR ͼ���Ƭ  2, 3, 4
2                   Ƭ���� A                 2
3                   Ƭ���� B                 3
4                   Ƭ���� C                 4
5                   IDR ͼ���е�Ƭ           2, 3
6                   ������ǿ��Ϣ��Ԫ��SEI��  5
7                   ���в�����               0
8                   ͼ�������               1
9                   �ֽ��                   6
10                  ���н���                 7
11                  ��������                 8
12                  ���                     9
13..23              ���� 
24..31              δʹ��
*/

/*
    �� SODB �� RBSP �����ɹ��̣�
    -- ��� SODB �����ǿյģ����ɵ� RBSP Ҳ�ǿյ�
    -- ����RBSP �����µķ�ʽ���ɣ�
        1�� RBSP �ĵ�һ���ֽ�ֱ��ȡ�� SODB �ĵ� 1 �� 8 �����أ���RBSP �ֽ��ڵı��ذ��մ����Ҷ�ӦΪ�Ӹߵ��͵�˳�����У�most significant��,�Դ����ƣ�RBSP 
            �����ÿ���ֽڶ�ֱ��ȡ�� SODB ����Ӧ���ء�RBSP �����һ���ֽڰ��� SODB ����󼸸����أ������µ� rbsp_trailing_bits()

        2�� rbsp_trailing_bits()�ĵ�һ�������� 1,��������� 0��ֱ���ֽڶ��롣����� 0 ��Ŀ��Ҳ��Ϊ���ֽڶ��룩

        3�� ���������ɸ� cabac_zero_word(��ֵ���� 0x0000)
*/

NAL_Unit( int NumBytesInNALunit ) {
    char forbidden_zero_bit,                 //f(1) == 0
    char nal_ref_idc,                        //u(2) ָʾ��ǰ NAL �����ȼ�, ȡֵ��ΧΪ 0-3, ,ֵԽ��,��ʾ��ǰ NAL Խ��Ҫ,��Ҫ�����ܵ�����
                                                // H.264 �涨�����ǰ NAL �����ڲο�֡��Ƭ���������в�����������ͼ���������Щ��Ҫ�����ݵ�λʱ��
                                                // ���䷨Ԫ�ر������ 0�����ڴ��� 0 ʱ�����ȡ��ֵ��ȴû�н�һ���涨,ͨ��˫�����������ƶ�����
    char nal_unit_type,                      //u(5) nal_unit_type=5 ʱ����ʾ��ǰ NAL �� IDR ͼ���һ��Ƭ��
                                                // ����������£�IDR ͼ���е�ÿ��Ƭ��nal_unit_type ��Ӧ�õ��� 5��ע�� IDR ͼ����ʹ��Ƭ������

    int rbsp_byte[1024];
    int i, NumBytesInRBSP = 0, emulation_prevention_three_byte,

    for( i = 1; i < NumBytesInNALunit; i++ ) {

        //���������� NAL �ڲ���⵽�� 0x000003 ������ʱ������ 0x03 �������ָ�ԭʼ����

        //  0x000000  --->  0x00000300
        //  0x000001  --->  0x00000301
        //  0x000002  --->  0x00000302
        //  0x000003  --->  0x00000303
        
        if( i + 2 < NumBytesInNALunit && next_bits( 24 ) == 0x000003 ) {
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8) 
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8)
            i += 2,
            emulation_prevention_three_byte, //f(8) NAL �ڲ�Ϊ��ֹ����ʼ�뾺�������������ֽ� ,ֵΪ 0x03��
        } else 
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8) RBSP ָԭʼ�ֽ��غɣ����� NAL ��Ԫ�����ݲ��ֵķ�װ��ʽ��
                                                // ��װ���������� SODB��ԭʼ���ݱ���������
                                                // SODB �Ǳ�����ԭʼ���ݣ�SODB ����װΪ RBSP ����� NAL �����ݲ��֡�
    }
}

/*
    Exp-Golomb ���뼼��

    Exp-Golomb ���� Yuji Itoh ��[9]������� UVLC Universal Variable Length Coding
    ���뷽���ڽ�Ϊ 2 ʱ������ ���й̶��ı���ṹ ����:

    [0, . ,0][1][INFO...]
    |<- M ->|   |<- M ->|

        1) ������ ( M bit ) ǰ���� 0,
        2) 1 ���ص� 1,
        3) ( M bit) ����Ϣλ����,

    ���� M �ɹ�ʽ 4.4.1-1 ȷ��:

    M    = [LOG 2 (code_num + 1)] (����ȡ��)
    INFO = code_num + 1 - 2^M

        1) code_num Ҳ����Ҫ��ķ���ֵ
        2) ���� 0 û��ǰ���� INFO ֱ�ӱ���Ϊ 1

    �±��г��� Exp-Golomb ����ǰ 8 ���ṹ

    -----------+-----+---------------
    code_num   |bits |    code
    -----------+-----+---------------
        0      |  1  |      1
    -----------+-----+---------------
        1      |  3  |     010
    -----------+-----+---------------
        2      |  3  |     011
    -----------+-----+---------------
        3      |  5  |    00100
    -----------+-----+---------------
        4      |  5  |    00101
    -----------+-----+---------------
        5      |  5  |    00110
    -----------+-----+---------------
        6      |  5  |    00111
    -----------+-----+---------------
        7      |  7  |   0001000
    -----------+-----+---------------


    SPS : 00 00 00 01 67 4D 00 2A 95 A8 1E 00 89 F9 50 
    
    1) 4D [0100 1101] ---- [77]
        profile_idc = 77, //u(8)    main

    2) 00 [0000 0000] ---- [0]
        constraint_set0_flag = 0, //u(1)
        constraint_set1_flag = 0, //u(1)
        constraint_set2_flag = 0, //u(1)
        reserved_zero_5bits = 0, //u(5)

    3) 2A [0010 1010] ---- [42]
        level_idc = 42, //u(8)
  
    4) 95 A8 1E 00 89 F9 50  ---- [1001 0101] [1010 1000] [0001 1110] [0000 0000] [1000 1001] [1111 1001] [0101 0000]
    4) 95 A8 1E 00 89 F9 50  ---- 1 00101 011 010 1 0000001111000 0000001000100 1 1 1 1 1 1 00101 010 000
           
        seq_parameter_set_id = 1, //ue(v) ---- 0
        log2_max_frame_num_minus4 = 00101, //ue(v) ---- 4
        pic_order_cnt_type = 011,   //ue(v) ---- 2

        num_ref_frames = 010, //ue(v) ---- 1
        gaps_in_frame_num_value_allowed_flag = 1, //u(1)

        pic_width_in_mbs_minus1 = 0000001111000, //ue(v) ---- 119
        pic_height_in_map_units_minus1 = 0000001000100, //ue(v) ---- 71

        frame_mbs_only_flag = 1, //u(1)

        direct_8x8_inference_flag = 1, //u(1)

        frame_cropping_flag = 1, //u(1)
            frame_crop_left_offset = 1, //ue(v) ---- 0
            frame_crop_right_offset = 1, //ue(v) ---- 0
            frame_crop_top_offset = 1, //ue(v) ---- 0
            frame_crop_bottom_offset = 00101, //ue(v) ---- 4

        vui_parameters_present_flag = 0, //u(1)

        rbsp_trailing_bits() = 10000, 
        {
            rbsp_stop_one_bit = 1,
            rbsp_alignment_zero_bit = 0000,
        }

    PPS : 00 00 00 01 68 EE 3C 80
        EE 3C 80 ---- [1110 1110] [0011 1100] [1000 0000]

    1) pic_parameter_set_id = 1, //ue(v) ---- 0

    2) seq_parameter_set_id = 1, //ue(v) ---- 0

    3) entropy_coding_mode_flag = 1, //u(1)

    4) pic_order_present_flag = 0, //u(1)

    5) num_slice_groups_minus1 = 1, //ue(v) ---- 0

    6) num_ref_idx_l0_active_minus1 = 1, //ue(v) ---- 0

    7) num_ref_idx_l1_active_minus1 = 1, //ue(v) ---- 0

    8) weighted_pred_flag = 0, //u(1)

    9) weighted_bipred_idc = 00, //u(2)

    10) pic_init_qp_minus26 = 1, //se(v) ---- 0

    11) pic_init_qs_minus26 = 1, //se(v) ---- 0

    12) chroma_qp_index_offset = 1, //se(v) ---- 0

    13) deblocking_filter_control_present_flag = 1, //u()

    14) constrained_intra_pred_flag = 0, //u(1)

    15) redundant_pic_cnt_present_flag = 0, //u(1)

    16) rbsp_trailing_bits() = 1000 0000
*/

//SPS
Seq_Parameter_Set_rbsp(){

    //ָ������ profile��level��
    profile_idc,    //u(8) ��ʶ��ǰ H.264 ������ profile
                        //66 -- baseline    profile
                        //77 -- main        profile
                        //88 -- extended    profile

    //ע��: �� constraint_set0_flag,constraint_set1_flag,constraint_set2_flag �е��������ϵ��� 1 ʱ��A.2�е�������Լ������Ҫ�����
    constraint_set0_flag,   //u(1) ���� 1 ʱ��ʾ������Ӹ�¼ A.2.1 ��ָ����������Լ���������� 0 ʱ��ʾ���������������
    constraint_set1_flag,   //u(1) ���� 1 ʱ��ʾ������Ӹ�¼ A.2.2 ��ָ����������Լ���������� 0 ʱ��ʾ���������������
    constraint_set2_flag,   //u(1) ���� 1 ʱ��ʾ������Ӹ�¼ A.2.3 ��ָ����������Լ���������� 0 ʱ��ʾ���������������
    
    reserved_zero_5bits,    //u(5) ��Ŀǰ�ı�׼�б��䷨Ԫ�ر������ 0��������ֵ�����������ã�������Ӧ�ú��Ա��䷨Ԫ�ص�ֵ
    
    //ָ������ profile��level��
    level_idc,  //u(8) ��ʶ��ǰ������ Level, �����Level������ĳ�������µ������Ƶ�ֱ��ʡ������Ƶ֡�ʵȲ�������������ӵ� level�� level_idc ָ��

    //ע�⣺����������Ҫ�����µ����в�����ʱ��Ӧ��ʹ���µ� seq_parameter_set_id,��ʹ���µ����в�������������ȥ�ı�ԭ���Ĳ������е�����
    seq_parameter_set_id,   //ue(v) ָ�������в������� id �ţ���� id �Ž��� picture ���������ã����䷨Ԫ�ص�ֵӦ����[0��31]


    //ֵ��ע����� frame_num ��ѭ�������ģ����������� MaxFrameNum ���ִ� 0 ���¿�ʼ��һ�ֵļ�����
    //����������Ҫ�л��Ƽ������ѭ������Ȼ����������ǧ�������⣬��ͼ���˳������ɻ��ҡ��ڵڰ��»���ϸ���� H.264 �������ѭ���Ļ���
    log2_max_frame_num_minus4,  //ue(v) ����䷨Ԫ����Ҫ��Ϊ��ȡ��һ���䷨Ԫ�� frame_num ����ģ�frame_num ������Ҫ�ľ䷨Ԫ��֮һ��
                                    // ����ʶ����ͼ��Ľ���˳�򡣿����ھ䷨������fram-num �Ľ��뺯���� ue��v���������е� v ������ָ����
                                    // v = log2_max_frame_num_minus4 + 4
                                    //
                                    // ����һ���Ƕȿ�������䷨Ԫ��ͬʱҲָ���� frame_num �����ܴﵽ�����ֵ :
                                    // MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 )
                                    // ���� MaxFrameNum ��ʾ frame_num �����ֵ���ں����п��Կ������ڽ����������Ҳ��һ���ǳ���Ҫ�ı���
    
    pic_order_cnt_type, //ue(v) ָ���� poc (picture order count) �ı��뷽����poc ��ʶͼ��Ĳ���˳��
                            // ����H.264 ʹ���� B ֡Ԥ�⣬ʹ��ͼ��Ľ���˳�򲢲�һ�����ڲ���˳�򣬵�����֮�����һ����ӳ���ϵ��
                            // poc ������ frame-num ͨ��ӳ���ϵ���������Ҳ���������ɱ�������ʽ�ش��͡�
                            // H.264 ��һ������������ poc �ı��뷽��������䷨Ԫ�ؾ�������֪ͨ�������������ַ��������� poc��
                            // �����µļ����䷨Ԫ���Ƿֱ��ڸ��ַ������õ������ݡ�

                            // �����µ���Ƶ�����б��䷨Ԫ�ز�Ӧ�õ��� 2:
                            // - һ���ǲο�֡�Ľ��뵥Ԫ���������һ���ǲο�ͼ��(ָ�ο�֡��ο���)�Ľ��뵥Ԫ
                            // - �����ֱ���������ǲο����ԵĽ��뵥Ԫ���������һ���ǲο�ͼ��Ľ��뵥Ԫ
                            // - һ���ǲο����Ľ��뵥Ԫ�������������һ���ǲο���,���������������ܹ���һ����������

    if ( pic_order_cnt_type == 0 ) {
        log2_max_pic_order_cnt_lsb_minus4,  //ue(v) ָ���˱��� MaxPicOrderCntLsb ��ֵ:
                                                // MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )
                                                // �ñ����� pic_order_cnt_type = 0 ʱʹ��
    } else if ( pic_order_cnt_type == 1 ) {
        delta_pic_order_always_zero_flag,   //u(1) ���� 1 ʱ,�䷨Ԫ�� delta_pic_order_cnt[0]�� delta_pic_order_cnt[1]����Ƭͷ����,�������ǵ�ֵĬ��Ϊ 0; 
                                                // ���䷨Ԫ�ص��� 0 ʱ,�����������䷨Ԫ�ؽ���Ƭͷ����
                                                
        offset_for_non_ref_pic, //se(v) ����������ǲο�֡�򳡵� picture order count (�� 8.2.1),���䷨Ԫ�ص�ֵӦ����[-2^31 , 2^31 �C 1]
        
        offset_for_top_to_bottom_field, //se(v) ����������֡�ĵ׳��� picture order count (�� 8.2.1), ���䷨Ԫ�ص�ֵӦ����[-2^31 , 2^31 �C 1]
        
        num_ref_frames_in_pic_order_cnt_cycle,  //ue(v) ���������� picture order count (�� 8.2.1),���䷨Ԫ�ص�ֵӦ����[0,255]

        for ( i=0; i<num_ref_frames_in_pic_order_cnt_cycle; ++i ) {
            offset_for_ref_frame[i],    //se(v) picture order count type=1 ʱ�ã����ڽ��� POC��
                                            // ���䷨Ԫ�ض�ѭ�� num_ref_frames_in_pic_order_cycle �е�ÿһ��Ԫ��ָ��һ��ƫ��
        }
    }
    num_ref_frames, //ue(v) ָ���ο�֡���п��ܴﵽ����󳤶ȣ���������������䷨Ԫ�ص�ֵ���ٴ洢��������洢�����ڴ���ѽ���Ĳο�֡��
                        // H.264 �涨������ 16 ���ο�֡�����䷨Ԫ�ص�ֵ���Ϊ 16��ֵ��ע��������������֡Ϊ��λ������ڳ�ģʽ�£�Ӧ����Ӧ����չһ��
            
    gaps_in_frame_num_value_allowed_flag,   //u(1) ����䷨Ԫ�ص��� 1 ʱ����ʾ����䷨Ԫ�� frame_num ���Բ�������
                                                // �������ŵ���������ʱ����������������������ͼ��ȫ����������ʱ����������֡ͼ��
                                                // �����������ÿһ֡ͼ�������������� frame_num ֵ��
                                                // ��������鵽��� frame_num ������������ȷ����ͼ�񱻱�����������
                                                // ��ʱ���������������������ڲصĻ��������Ƶػָ���Щͼ����Ϊ��Щͼ���п��ܱ�����ͼ�������ο�֡
                                                //
                                                // ������䷨Ԫ�ص��� 0 ʱ�������� frame_num �������������������κ�����¶����ܶ���ͼ��
                                                // ��ʱ��H.264 ������������Բ�ȥ��� frame_num ���������Լ��ټ�������
                                                // ��������������Ȼ���� frame_num ����������ʾ�ڴ����з���������
                                                // ��������ͨ���������Ƽ�⵽�����ķ�����Ȼ�����������ڲصĻָ�ͼ��
            
    pic_width_in_mbs_minus1,    //ue(v) ���䷨Ԫ�ؼ� 1 ��ָ��ͼ���ȣ��Ժ��Ϊ��λ��
                                    // PicWidthInMbs = pic_width_in_mbs_minus1 + 1
                                    // ͨ������䷨Ԫ�ؽ��������Լ���õ� ���ȷ��� ������Ϊ��λ��ͼ���ȣ�
                                    // PicWidthInSamplesL = PicWidthInMbs * 16
                                    // �Ӷ�Ҳ���Եõ� ɫ�ȷ��� ������Ϊ��λ��ͼ���ȣ�
                                    // PicWidthInSamplesC = PicWidthInMbs * 8
                                    // ���ϱ��� PicWidthInSamplesL��PicWidthInSamplesC �ֱ��ʾͼ������ȡ�ɫ�ȷ���������Ϊ��λ�Ŀ�
                                    //
                                    // H.264 ��ͼ��Ĵ�С�����в������ж��壬��ζ�ſ�����ͨ�Ź������������в�������̬�ظı�ͼ��Ĵ�С��
                                    // �ں����п��Կ������������Խ����͵�ͼ����ú����
                                                                
    pic_height_in_map_units_minus1, //ue(v) ���䷨Ԫ�ؼ� 1 ��ָ��ͼ��߶ȣ�
                                        // PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1
                                        // PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits
                                        // ͼ��ĸ߶ȵļ���Ҫ�ȿ�ȵļ��㸴�ӣ���Ϊһ��ͼ�������֡Ҳ�����ǳ���
                                        // ������䷨Ԫ�ؿ�����֡ģʽ�ͳ�ģʽ�·ֱ����������ȡ�ɫ�ȵĸߡ�
                                        // ֵ��ע����ǣ������� map_unit Ϊ��λ��map_unit�ĺ����ɺ�������
                                                                
    frame_mbs_only_flag,    //u(1) ���䷨Ԫ�ص��� 0 ʱ��ʾ������������ͼ��ı���ģʽ����֡��û����������ģʽ���ڣ�
                                // ���䷨Ԫ�ص��� 1 ʱ ����ʾ��������ͼ��ı���ģʽ������֡��Ҳ�����ǳ���֡������Ӧ��ĳ��ͼ���������һ��Ҫ�������䷨Ԫ�ؾ�����
                                // ��� map_unit �ĺ��壬���������һ���䷨Ԫ��pic_height_in_map_units_minus1�Ľ�һ���������裺
                                // �� frame_mbs_only_flag == 1��pic_height_in_map_units_minus1 ָ����һ�� picture �� ֡ �ĸ߶ȣ�
                                // �� frame_mbs_only_flag == 0��pic_height_in_map_units_minus1 ָ����һ�� picture �� �� �ĸ߶ȣ�
                                // ���Կ��Եõ������Ժ��Ϊ��λ��ͼ��߶ȣ�
                                // FrameHeightInMbs = ( 2 �C frame_mbs_only_flag ) * PicHeightInMapUnits
                                // PictureHeightInMbs= ( 2 �C frame_mbs_only_flag ) * PicHeightInMapUnits
    
    if( !frame_mbs_only_flag ) {
        mb_adaptive_frame_field_flag,   //u(1)  ָ���������Ƿ�����֡������Ӧģʽ��
                                            // mb_adaptive_frame_field_flag == 1 
                                            // ʱ�����ڱ������е�ͼ��������� ��ģʽ ���� ֡������Ӧģʽ��
                                            // mb_adaptive_frame_field_flag == 0 
                                            // ʱ��ʾ�������е�ͼ��������� ��ģʽ ���� ֡ģʽ
                                            // �о���һ�������п��ܳ��ֵı���ģʽ��
                                            // a. ȫ����֡����Ӧ�� frame_mbs_only_flag =1  �����
                                            // b. ֡�ͳ����档frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =0
                                            // c. ֡������Ӧ�ͳ����档frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =1
                                            // ֵ��ע����ǣ�֡��֡������Ӧ���ܹ�����һ��������
    }

    direct_8x8_inference_flag,  //u(1) ����ָ�� B Ƭ��ֱ�Ӻ� skip ģʽ���˶�ʸ����Ԥ�ⷽ����
    
    frame_cropping_flag,    //u(1) ����ָ���������Ƿ�Ҫ��ͼ��ü��������
                                // ����ǵĻ�����������ŵ��ĸ��䷨Ԫ�طֱ�ָ�����ҡ����²ü��Ŀ�ȡ�

    if ( frame_cropping_flag ) {
        frame_crop_left_offset,                 //ue(v)
        frame_crop_right_offset,                //ue(v)
        frame_crop_top_offset,                  //ue(v)
        frame_crop_bottom_offset,               //ue(v)
    }

    vui_parameters_present_flag,    //u(1) ָ�� vui �ӽṹ�Ƿ�����������У�vui �������ṹ�ڸ�¼��ָ�������Ա�����Ƶ��ʽ�ȶ�����Ϣ��

    if ( vui_parameters_present_flag ) {
        vui_parameters(),                       //
    }

    rbsp_trailing_bits(),                       //
}



    //PPS
Pic_Parameter_Set_rbsp () {

    pic_parameter_set_id,  //ue(v) ����ָ��������������ţ�������ڸ�Ƭ��Ƭͷ������
                                // slice ���� PPS �ķ�ʽ������ Slice header �б��� PPS �� id ֵ����ֵ��ȡֵ��ΧΪ[0,255]��
                                                        
    seq_parameter_set_id,  //ue(v) ָ����ͼ������������õ����в����������, ��ֵ��ȡֵ��ΧΪ[0,31]��
                                                        
    entropy_coding_mode_flag,  //u(1) ָ���ر����ѡ�񣬱��䷨Ԫ��Ϊ��ʱ����ʾ�ر���ʹ�� CAVLC�����䷨Ԫ��Ϊ��ʱ��ʾ�ر���ʹ�� CABAC
                                    // ���ڲ����﷨Ԫ�أ��ڲ�ͬ�ı��������£�ѡ����ر��뷽ʽ��ͬ��
                                    // ������һ������﷨���У�������� m b_type ���﷨Ԫ��������Ϊ��ue(v) | ae(v)����
                                    // �� baseline profile �������²���ָ�����ײ����룬
                                    // �� main profile �������²��� CABAC ���롣
                                    // ��ʶλ entropy_coding_mode_flag �����þ��ǿ��������㷨ѡ��
                                    // ����ֵΪ 0 ʱ��ѡ����ߵ��㷨��ͨ��Ϊָ�����ײ�������� CAVLC��
                                    // ����ֵΪ 1 ʱ��ѡ���ұߵ��㷨��ͨ��Ϊ CABAC��
                                                        
    pic_order_present_flag,    //u(1) POC �����ּ��㷽����Ƭ�㻹����Ҫ��һЩ�䷨Ԫ����Ϊ������
                                    // pic_order_present_flag == 1 ʱ ��ʾ��Ƭͷ���о䷨Ԫ��ָ����Щ������
                                    // pic_order_present_flag == 0 ʱ ��ʾƬͷ���������Щ��������Щ����ʹ��Ĭ��ֵ
                                                        
    num_slice_groups_minus1,   //ue(v) ���䷨Ԫ�ؼӣ���ָ��ͼ����Ƭ��ĸ�����
                                    // ��.264 ��û��ר�ŵľ䷨Ԫ������ָ���Ƿ�ʹ��Ƭ��ģʽ��
                                    // num_slice_groups_minus1 == 0,����ֻ��һ��Ƭ�飩����ʾ��ʹ��Ƭ��ģʽ������Ҳ����������ڼ���Ƭ��ӳ��ľ䷨Ԫ��

    if ( num_slice_groups_minus1 > 0 ) {
        slice_group_map_type,  //ue(v) �� num_slice_group_minus1 ���ڣ�����ʹ��Ƭ��ģʽʱ�����䷨Ԫ�س����������У�����ָ��Ƭ��ָ����͡�
                                    // map_units �Ķ��壺
                                    // �� frame_mbs_only_flag == 1 ʱ��map_units ָ�ľ��Ǻ��
                                    // �� frame_mbs_only_falg == 0 ʱ,
                                    //  ֡������Ӧģʽʱ��map_units ָ���Ǻ���
                                    //  ��ģʽʱ��map_units ָ���Ǻ��
                                    //  ֡ģʽʱ��map_units ָ��������������Ƶģ����������������������

        if ( slice_group_map_type == 0 ) {
            for( iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                run_length_minus1[ iGroup ],    //ue(v) ����ָ����Ƭ�����͵��ڣ�ʱ��ÿ��Ƭ�������� map_units ����
            }
        } else if ( slice_group_map_type == 2 ) {
            for( iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                top_left[ iGroup ],             //ue(v) ����ָ����Ƭ�����͵��ڣ�ʱ��������������ϼ�����λ�á�
                bottom_right[ iGroup ],         //ue(v)
            }
        } else if ( slice_group_map_type == 3 ||
                    slice_group_map_type == 4 ||
                    slice_group_map_type == 5 ) {
            slice_group_change_direction_flag,  //u(1) ��Ƭ�����͵��ڣ���������ʱ�����䷨Ԫ������һ���䷨Ԫ��һ��ָ��ȷ�е�Ƭ��ָ��
            slice_group_change_rate_minus1,     //ue(v) ����ָ������ SliceGroupChangeRAte
        } else if ( slice_group_map_type == 6 ) {
            pic_size_in_map_units_minus1,       //ue(v) ��Ƭ�����͵��ڣ�ʱ������ָ��ͼ���� map_units Ϊ��λ�Ĵ�С

            for( i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                slice_group_id[ i ],            //u(v) ��Ƭ�����͵��ڣ�ʱ������ָ��ĳ�� map_units �����ĸ�Ƭ�顣
            }
        }
    }

    // �� 1 ��, ָ��Ŀǰ�ο�֡���еĳ��ȣ����ж��ٸ��ο�֡���������ںͳ��ڣ���ֵ��ע����ǣ���Ŀǰ����ͼ���ǳ�ģʽ�£��ο�֡���еĳ���Ӧ���Ǳ��䷨Ԫ���ٳ���2��
    // ��Ϊ��ģʽ�¸�֡���뱻�ֽ��Գ�����ʽ���ڡ���������˵�ĳ�ģʽ����ͼ��ĳ���֡������Ӧ�µĴ��ڳ�ģʽ�ĺ��ԣ� ���䷨Ԫ�ص�ֵ�п�����Ƭͷ�����ء�
    // ���߿��ܻ��ǵ������в��������о䷨Ԫ�� num_ref_frames Ҳ�Ǹ��ο�֡�����йأ����ǵ������� :
    // num_ref_frames ָ�� �ο�֡���� �� ���ֵ, ������������ֵ�������ڴ�ռ�;
    // num_ref_idx_l0_active_minus1 ָ������������е�ǰʵ�ʵġ��Ѵ��ڵĲο�֡��Ŀ������������֡�active����Ҳ���Կ�������
    // ����䷨Ԫ���� H.264 ������Ҫ�ľ䷨Ԫ��֮һ���ڵ������ǿ��Կ�����������Ҫ֪ͨ������ĳ���˶�ʸ����ָ������ĸ��ο�ͼ��ʱ, 
    // ������ֱ�Ӵ��͸�ͼ��ı�ţ����Ǵ��͸�ͼ���ڲο�֡�����е���š������Ų������������д��͵ģ����Ǳ������ͽ�����ͬ���ء�����ͬ�ķ������ο�ͼ�������У�
    // �Ӷ����һ����š����������ÿ��һ��ͼ��������ÿ��Ƭ�󶼻ᶯ̬�ظ��¡�ά���ο�֡�����Ǳ������ʮ����Ҫ�Ĺ����������䷨Ԫ����ά���ο�֡���е���Ҫ���ݡ�
    // �ο�֡���еĸ��ӵ�ά�������� H.264 ��ҪҲ�Ǻ�����ɫ����ɲ���
    num_ref_idx_l0_active_minus1,   //ue(v) 
    
    num_ref_idx_l1_active_minus1,   //ue(v) ����һ���䷨Ԫ�ص�����һ�£�ֻ�Ǳ��䷨Ԫ������ list��������һ�䷨Ԫ������ list 0

    weighted_pred_flag, //u(1) ����ָ���Ƿ����� P �� SP Ƭ�ļ�ȨԤ�⣬���������Ƭͷ��������Լ����ȨԤ��ľ䷨Ԫ��
    
    weighted_bipred_idc,    //u(2) ����ָ���Ƿ����� B Ƭ�ļ�ȨԤ�⣬
                                // weighted_bipred_idc == 0 ʱ��ʾʹ�� Ĭ�� ��ȨԤ��ģʽ,
                                // weighted_bipred_idc == 1 ʱ��ʾʹ�� ��ʽ ��ȨԤ��ģʽ,
                                // weighted_bipred_idc == 2 ʱ��ʾʹ�� ��ʽ ��ȨԤ��ģʽ

    pic_init_qp_minus26,    //se(v) �� 26 ������ָ�����ȷ��������������ĳ�ʼֵ.
                                //�� H.264 �У������������������������ͼ���������Ƭͷ����顣��ͼ���������������һ����ʼֵ��
                                
    pic_init_qs_minus26,    //se(v) ����һ���䷨Ԫ������һ�£�ֻ������ SP �� SI
    
    chroma_qp_index_offset, //se(v) ɫ�ȷ��������������Ǹ������ȷ���������������������ģ����䷨Ԫ������ָ������ʱ�õ��Ĳ��� ȡֵ��ΧΪ[-12,12]��

    //����������ͨ���䷨Ԫ����ʽ�ؿ���ȥ���˲���ǿ�ȣ����䷨Ԫ��ָ������Ƭͷ�Ƿ���о䷨Ԫ�ش������������Ϣ��
    deblocking_filter_control_present_flag,     //u(1) ��ʶλ�����ڱ�ʾ Slice header ���Ƿ��������ȥ���˲������Ƶ���Ϣ��
                                                    // ���ñ�־λΪ 1 ʱ��slice header �а���ȥ���˲���Ӧ����Ϣ��
                                                    // ���ñ�ʶλΪ 0 ʱ��slice header ��û����Ӧ����Ϣ���������������ؼ�����˲�ǿ��
                                                        
    // �� P �� B Ƭ�У�֡�ڱ���ĺ����ڽ��������ǲ��õ�֡����롣
    // �����䷨Ԫ�ص��� 1 ʱ����ʾ֡�ڱ���ĺ�鲻����֡�����ĺ���������Ϊ�Լ���Ԥ�⣬��֡�ڱ���ĺ��ֻ�����ڽ�֡�ڱ���ĺ���������Ϊ�Լ���Ԥ��;
    // �����䷨Ԫ�ص��� 0 ʱ����ʾ��������������
    constrained_intra_pred_flag,    //u(1) ���ñ�ʶΪ 1����ʾ I ����ڽ���֡��Ԥ��ʱֻ��ʹ������ I �� SI ���ͺ�����Ϣ��
                                        //���ñ�ʶλ 0����ʾ I ������ʹ������ Inter ���ͺ�����Ϣ��
                                                     
    redundant_pic_cnt_present_flag, //u(1) ��ʶλ�����ڱ�ʾ Slice header ���Ƿ���� redundant_pic_cnt �﷨Ԫ�ء�
                                        // ���ñ�־λΪ 1 ʱ��slice header �а��� redundant_pic_cnt��
                                        // ���ñ�ʶλΪ 0 ʱ��slice header ��û����Ӧ����Ϣ��

    rbsp_trailing_bits(),                       //
}


//Ƭ��䷨(������)
slice_layer_without_partitioning_rbsp () {
    slice_header(),
    slice_data(),                   // all categories of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//Ƭ�� A �����䷨
slice_data_partition_A_layer_rbsp () {
    slice_header(),
    slice_id,                       //ue(v)
    slice_data(),                   // only category 2 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(), 
}

//Ƭ�� B �����䷨
slice_data_partition_B_layer_rbsp () {
    slice_id,                       //ue(v)

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,          //ue(v)
    }

    slice_data(),                   //  only category 3 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//Ƭ�� C �����䷨
slice_data_partition_C_layer_rbsp () {
    slice_id,                       //ue(v)

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,          //ue(v)
    }

    slice_data(),                   // only category 4 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//��β��trailing bits���䷨
rbsp_trailing_bits () {
    rbsp_stop_one_bit,              //f(1) equal to 1

    while ( !byte_aligned() ) {
        rbsp_alignment_zero_bit,    //f(1) equal to 0
    }
}

/*

slice_type      Name_of_slice_type
0               P   (P slice)
1               B   (B slice)
2               I   (I slice)
3               SP  (SP slice)
4               SI  (SI slice)
5               P   (P slice)
6               B   (B slice)
7               I   (I slice)
8               SP  (SP slice)
9               SI  (SI slice)

*/

slice_header () {
    
    first_mb_in_slice,  // ue(v) Ƭ�еĵ�һ�����ĵ�ַ, Ƭͨ������䷨Ԫ�����궨���Լ��ĵ�ַ.
                            // Ҫע�������֡������Ӧģʽ�£���鶼�ǳɶԳ��֣���ʱ���䷨Ԫ�ر�ʾ���ǵڼ������ԣ���Ӧ�ĵ�һ��������ʵ��ַӦ����:
                            // 2 * first_mb_in_slice
                            
    slice_type, // ue(v) ָ��Ƭ�����ͣ������������7.21
    
    pic_parameter_set_id,   // ue(v) ͼ���������������. ��Χ 0 �� 255

    // H.264 �� frame_num ��ֵ�������¹涨: ���������еľ䷨Ԫ�� gaps_in_frame_num_value_allowed_flag ��Ϊ 1 ʱ��
    // ÿ��ͼ��� frame_num ֵ����ǰһ���ο�֡�� frame_num ֵ���� 1, ��仰������������˼:
    // 1) �� gaps_in_frame_num_value_allowed_flag == 0���� frame_num ����������£�ÿ��ͼ���frame_num ��ǰһ���ο�֡ͼ���Ӧ��ֵ�� 1�����ص��ǡ�ǰһ���ο�֡��
    // 2) �� gaps_in_frame_num_value_allowed_flag == 1��ǰ���Ѿ��ᵽ����ʱ���������������������Խ�����������ͼ����������������֪ͨ��������
    //  ����������£������������л��ƽ�ȱʧ�� frame_num ������Ӧ��ͼ������������ͼ�������˶�ʸ��ָ��ȱʧ��ͼ�񽫻�����������
    frame_num,  // u(v) ÿ���ο�֡����һ������������ frame_num ��Ϊ���ǵı�ʶ,��ָ���˸�ͼ��Ľ���˳��
                    // ����ʵ�������ڱ� �п��Կ�����frame_num �ĳ���û�� if ����޶�������������ǲο�֡��ƬͷҲ����� frame_num��
                    // ֻ�ǵ��ø�ͼ���ǲο�֡ʱ������Я��������䷨Ԫ���ڽ���ʱ��������

    if ( !frame_mbs_only_flag ) {

        /*
                if ( frame_mbs_only_flag == 1 ) {
                    ֡����
                } else {
                    if ( mb_adaptive_frame_field_flag == 1 ) {
                        if ( field_pic_flag == 1 ) {
                            ������
                        } else {
                            ֡������Ӧ
                        }
                    } else {
                        if ( field_pic_flag == 1 ) {
                            ������
                        } else {
                            ֡����
                        }
                    }
                }
            */
            
        //�����в������������Ѿ��ܹ������ͼ��ĸߺͿ�Ĵ�С������ǿ������ĸ���ָ�ĸ�������ͼ���֡�ĸ߶ȣ���һ��ʵ�ʵ�ͼ�������֡Ҳ�����ǳ���
        //����ͼ���ʵ�ʸ߶ȣ�Ӧ��һ�������´��� :
        // PicHeightInMbs = FrameHeightInMbs / ( 1 + field_pic_flag ) 
        // �Ӷ����ǿ��Եõ��ڽ����������õ���������ͼ���С�йصı�����
        // PicHeightInSamplesL = PicHeightInMbs * 16 
        // PicHeightInSamplesC = PicHeightInMbs * 8 
        // PicSizeInMbs = PicWidthInMbs * PicHeightInMbs

        // ǰ�����ᵽ��frame_num �ǲο�֡�ı�ʶ�������ڽ������У�������ֱ�����õ� frame_num ֵ�������� frame_num ��һ����������ı��� PicNum,
        // �ڵڰ��»���ϸ������ frame_num ӳ�䵽PicNum ���㷨����������ڸ��㷨���õ����������� :
        //
        // MaxPicNum :
        // ���� PicNum �����ֵ��PicNum �� frame_num һ����Ҳ��Ƕ��ѭ���У����ﵽ������ֵʱ��PicNum ���� 0 ��ʼ���¼���
        // ���field_pic_flag= 0, MaxPicNum = MaxFrameNum.
        // ����MaxPicNum =2*MaxFrameNum.
        //
        // CurrPicNum :
        // ��ǰͼ��� PicNum ֵ���ڼ��� PicNum �Ĺ����У���ǰͼ��� PicNum ֵ���� frame_num ֱ�����
        //���ڵڰ����лῴ�����ڽ�ĳ��ͼ��ʱ��Ҫ���Ѿ�����ĸ��ο�֡�� PicNum ���¼���һ�飬�µ�ֵ�ο���ǰͼ��� PicNum ������
        // ���field_pic_flag= 0�� CurrPicNum = frame_num.
        // ����, CurrPicNum= 2 * frame_num + 1.

        // Frame_num �Ƕ� ֡ ��ŵģ�Ҳ����˵����ڳ�ģʽ�£�ͬ��һ�����ԵĶ����͵׳�����ͼ��� frame_num ��ֵ����ͬ�ġ�
        // ��֡��֡������Ӧģʽ�£���ֱ�ӽ�ͼ��� frame_num ���� PicNum�����ڳ�ģʽ�£���2 * frame_num �� 2 * frame_num + 1 ����ֵ�ֱ𸳸���������
        // 2 * frame_num + 1 ���ֵ��Զ��������ǰ�������뵽��ǰ���Ե���һ����ʱ, 
        // �ղű���Ϊ 2 * frame_num + 1 �ĳ��� PicNum ֵ�����¼���Ϊ 2 * frame_num ������ 2 * frame_num + 1 �����µĵ�ǰ��
        field_pic_flag, // u(1) ������Ƭ���ʶͼ�����ģʽ��Ψһһ���䷨Ԫ�ء���ν�ı���ģʽ��ָ��֡���롢�����롢֡������Ӧ���롣
                            // ������䷨Ԫ��ȡֵΪ 1 ʱ ���ڳ����룻 0 ʱΪ�ǳ�����
                            // ���в������еľ䷨Ԫ�� frame_mbs_only_flag �� mb_adaptive_frame_field_flag �ټ��ϱ��䷨Ԫ�ع�ͬ����ͼ��ı���ģʽ
        
        if ( field_pic_flag ) {
            bottom_field_flag,  // u(1) ���� 1 ʱ��ʾ��ǰͼ�������ڵ׳������� 0 ʱ��ʾ��ǰͼ�������ڶ���
        }
    }

    if ( nal_unit_type == 5 ) {
        idr_pic_id, // ue(v) ��ͬ�� IDR ͼ���в�ͬ�� idr_pic_id ֵ��ֵ��ע����ǣ�IDR ͼ���в��ȼ��� I ͼ��
                        // ֻ������Ϊ IDR ͼ��� I ֡��������䷨Ԫ�أ��ڳ�ģʽ�£�IDR ֡������������ͬ�� idr_pic_id ֵ��
                        // idr_pic_id ��ȡֵ��Χ�� [0��65535]���� frame_num ���ƣ�������ֵ���������Χʱ��������ѭ���ķ�ʽ���¿�ʼ����
    }

    if ( pic_order_cnt_type == 0 ) {
        pic_order_cnt_lsb,  //u(v) �� POC �ĵ�һ���㷨�б��䷨Ԫ�������� POC ֵ���� POC �ĵ�һ���㷨������ʽ�ش��� POC ��ֵ��
                                // �����������㷨��ͨ�� frame_num ��ӳ�� POC ��ֵ��ע������䷨Ԫ�صĶ�ȡ������ u(v),��� v ��������ͼ�������: 
                                // v=log2_max_pic_order_cnt_lsb_minus4 + 4

        if ( pic_order_present_flag && !field_pic_flag ) {
            delta_pic_order_cnt_bottom, //se(v) ������ڳ�ģʽ�£������е������������Ա�����Ϊһ��ͼ��
                                            // �����и��Ե� POC �㷨���ֱ������������ POC ֵ��Ҳ����һ������ӵ��һ�� POC ֵ��
                                            // ������֡ģʽ����֡������Ӧģʽ�£�һ��ͼ��ֻ�ܸ���Ƭͷ�ľ䷨Ԫ�ؼ����һ�� POC ֵ��
                                            // ���� H.264 �Ĺ涨�����������п��ܳ��ֳ���������� frame_mbs_only_flag ��Ϊ 1 ʱ��
                                            // ÿ��֡��֡������Ӧ��ͼ���ڽ���������ֽ�Ϊ���������Թ�����ͼ���еĳ���Ϊ�ο�ͼ��
                                            // ���Ե� frame_mb_only_flag ��Ϊ 1ʱ��֡��֡������Ӧ�а�����������Ҳ�����и��Ե� POC ֵ��
                                            // �ڵڰ��������ǻῴ����ͨ�����䷨Ԫ�أ��������Ѿ��⿪��֡��֡������Ӧͼ��� POC ��������ӳ��һ�� POC ֵ��
                                            // �����������׳�����Ȼ����䷨��ָ��������������䷨Ԫ��ֻ���� POC �ĵ�һ���㷨��
        }
    }

    if ( pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag ) {

        delta_pic_order_cnt[ 0 ],   //se(v) ����֡���뷽ʽ�µ� �׳� �� �����뷽ʽ�� ��

        if ( pic_order_present_flag && !field_pic_flag ) {
            delta_pic_order_cnt[ 1 ],   //se(v) ����֡���뷽ʽ�µ� ����
        }
    }

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,  //ue(v) ����Ƭ�� id ��
    }

    if ( slice_type == B ) {
        direct_spatial_mv_pred_flag,    //u(1) ָ���� Bͼ�� �� ֱ��Ԥ�� ��ģʽ�£���ʱ��Ԥ�⻹���ÿռ�Ԥ�⡣1���ռ�Ԥ�⣻0��ʱ��Ԥ��
    }

    if ( slice_type == P || slice_type == SP || slice_type == B ) {
        num_ref_idx_active_override_flag,//u(1) ��ͼ������� �� ���ǿ����Ѿ����־䷨Ԫ�� 
                                           // num_ref_idx_l0_active_minus1 �� num_ref_idx_l1_active_minus1 ָ����ǰ�ο�֡������ʵ�ʿ��õĲο�֡����Ŀ��
                                           // ��Ƭͷ����������Ծ䷨Ԫ�أ��Ը�ĳ�ض�ͼ���������ȡ�����䷨Ԫ�ؾ���ָ��Ƭͷ�Ƿ�����أ�
                                           // ����þ䷨Ԫ�ص��� 1�����������µ� num_ref_idx_l0_active_minus1 �� num_ref_idx_l1_active_minus1 ֵ

        if ( num_ref_idx_active_override_flag ) {
            num_ref_idx_l0_active_minus1,       //ue(v) ���ص� num_ref_idx_l0_active_minus1

            if ( slice_type == B ) {
                num_ref_idx_l1_active_minus1,   //ue(v) ���ص� num_ref_idx_l1_active_minus1
            }
        }
    }

    ref_pic_list_reordering () {
        if ( (weighted_pred_flag && ( slice_type == P || slice_type == SP )) || 
             ( weighted_bipred_idc == 1 && slice_type == B ) ) {
            pred_weight_table(),
        }

        if ( nal_ref_idc != 0 ) {
            dec_ref_pic_marking(),
        }

        if ( entropy_coding_mode_flag && slice_type != I && slice_type != SI ) {
            cabac_init_idc, //ue(v) ���� cabac ��ʼ��ʱ����ѡ�񣬷�Χ 0 �� 2
        }

        slice_qp_delta, //se(v) ָ�������ڵ�ǰƬ�����к������������ĳ�ʼֵ QPy
                            // SliceQPy = 26 + pic_init_qp_minus26 + slice_qp_delta 
                            // QPy �ķ�Χ�� [0, 51]
                            // ����ǰ���Ѿ��ᵽ��H.264 �����������Ƿ�ͼ���������Ƭͷ�����ͷ��������ģ�ǰ������Ը���һ��ƫ��ֵ������䷨Ԫ�ؾ���Ƭ���ƫ��

        if ( slice_type == SP || slice_type == SI ) {
            if ( slice_type == SP ) {
                sp_for_switch_flag, //u(1) ָ��SP֡�е�p���Ľ��뷽ʽ�Ƿ���switchingģʽ���ڵڰ�������ϸ��˵��
            }

            slice_qs_delta, //se(v) �� slice_qp_delta �����������ƣ����� SI �� SP �е�
                                // QSy = 26 + pic_init_qs_minus26 + slice_qs_delta
                                // QSy ֵ�ķ�Χ�� [0, 51]
        }

        if ( deblocking_filter_control_present_flag ) {
            disable_deblocking_filter_idc,  //ue(v) H.264ָ����һ���㷨�����ڽ������˶����ؼ���ͼ���и��߽���˲�ǿ�Ƚ����˲���
                                                // ���˽�������������֮�⣬������Ҳ���Դ��ݾ䷨Ԫ���������˲�ǿ�ȣ�
                                                // ������䷨Ԫ��ָ�����ڿ�ı߽��Ƿ�Ҫ���˲���ͬʱָ���Ǹ���ı߽粻�ÿ��˲����ڵڰ��»���ϸ����

            if ( disable_deblocking_filter_idc != 1 ) {
                slice_alpha_c0_offset_div2, //se(v) ����������ǿ �� �� t_C0 ��ƫ��ֵ
                                                // FilterOffsetA = slice_alpha_c0_offset_div2 << 1
                                                // ȡֵ�ķ�Χ�� [-6, +6]
                                                
                slice_beta_offset_div2, //se(v) ����������ǿ �� �� t_C0 ��ƫ��ֵ
                                            // FilterOffsetB = slice_beta_offset_div2 << 1
                                            // ȡֵ�ķ�Χ�� [-6, +6]
            }
        }

        if ( num_slice_groups_minus1 > 0 && slice_group_map_type >= 3 && slice_group_map_type <= 5 ) {
            slice_group_change_cycle,   //u(v) ��Ƭ��������� 3, 4, 5���ɾ䷨Ԫ�ؿɻ��Ƭ���� ӳ�䵥Ԫ����Ŀ:
                                            // MapUnitsInSliceGroup0 = Min(  slice_group_change_cycle  *  SliceGroupChangeRate,PicSizeInMapUnits )
                                            // slice_group_change_cycle �� Ceil( Log2( PicSizeInMapUnits �� SliceGroupChangeRate + 1 ) )λ���ر�ʾ
                                            // slice_group_change_cycle ֵ�ķ�Χ�� 0 �� Ceil( PicSizeInMapUnits��SliceGroupChangeRate )
        }
    }
}


/*
    ÿһ��ʹ�� ֡��Ԥ�� ��ͼ�񶼻�����ǰ���ѽ����ͼ����Ϊ �ο�֡����ǰ����������������ÿ�� �ο�֡ �������һ��Ψһ�Եı�ʶ�����䷨Ԫ�� frame_num��
    ���ǣ���������Ҫָ����ǰͼ��� �ο�ͼ�� ʱ��������ֱ��ָ����ͼ��� frame_num ֵ������ʹ��ͨ�����沽�����յó��� ref_id �ţ�

    frame_num  --����-->  PicNum  --����-->  ref_id

    �� frame_num ���任������ PicNum ��Ҫ�ǿ��ǵ���ģʽ����Ҫ����������������ֳ�ʱ��ÿ���ǳ���ͼ��֡�� ֡������Ӧ����������ֽ�Ϊһ�� ���ԣ�
    �Ӷ���ҪΪ���Ƿֽ��� ������ ����ָ��һ����ʶ; ��һ���� PicNum �� ref_id ��Ϊ���ܽ�ʡ��������Ϊ PicNum ��ֵͨ�����Ƚϴ󣬶���֡��Ԥ��ʱ, 
    ��ҪΪÿ���˶�ʸ����ָ�����Ӧ�Ĳο�֡�ı�ʶ����������ʶѡ�� PicNum �����ͻ�Ƚϴ����� H.264 �ֽ� PicNum ӳ��Ϊһ����С�ı��� ref_id��
    �ڱ������ͽ�������ͬ����ά��һ�� �ο�֡����, ÿ����һ��Ƭ�ͽ��ö���ˢ��һ��, �Ѹ�ͼ�����ض��Ĺ����������, ������ͼ���ڶ����е���ž��Ǹ�ͼ��� ref_id ֵ, 
    �����������ǿ��Կ����ں����ʾ�ο�ͼ��ı�ʶ���� ref_id, �ڵڰ������ǻ���ϸ�������еĳ�ʼ���������ά���㷨��
    ���ں��½ڽ��ܵ�����ά������ʱ������Ҫ������������(reordering) �� ���(marking) ���õ��ľ䷨Ԫ��

    ˵�����䷨Ԫ�صĺ�׺������ L0 ָ���ǵ�һ�������б��䷨Ԫ�صĺ�׺������ L1 ָ���ǵڶ��������б����� B ֡Ԥ���У�
*/

/*
    ���������

    ---------------------------------------------------------------------------------------------------
    reordering_of_pic_nums_idc          ����
    ---------------------------------------------------------------------------------------------------
            0                           ���ڲο�֡������abs_diff_pic_num_minus1 ������������У��ӵ�
                                        ǰͼ��� PicNum ��ȥ (abs_diff_pic_num_minus1 + 1) ��ָ����Ҫ��
                                        �����ͼ��
    ---------------------------------------------------------------------------------------------------
            1                           ���ڲο�֡������abs_diff_pic_num_minus1 ������������У��ӵ�
                                        ǰͼ��� PicNum ���� (abs_diff_pic_num_minus1 + 1) ��ָ����Ҫ��
                                        �����ͼ��
    ---------------------------------------------------------------------------------------------------
            2                           ���ڲο�֡������long_term_pic_num ������������У�ָ����Ҫ��
                                        �����ͼ��
    ---------------------------------------------------------------------------------------------------
            3                           ����ѭ�����˳������������
    ---------------------------------------------------------------------------------------------------
*/

//�ο�֡����������reordering���䷨
ref_pic_list_reordering(){

    if ( slice_type != I && slice_type != SI ) {
        ref_pic_list_reordering_flag_l0,    //u(1) ָ���Ƿ�������������,����䷨Ԫ�ص��� 1 ʱ���������Ż���һϵ�о䷨Ԫ�����ڲο�֡���е�������

        if ( ref_pic_list_reordering_flag_l0 ) {
            do {
                reordering_of_pic_nums_idc, //ue(v) ָ��ִ���������������,����������� 7.22

                if ( reordering_of_pic_nums_idc == 0 || reordering_of_pic_nums_idc == 1) {
                    
                    abs_diff_pic_num_minus1,    //ue(v) ���ڲο�֡������, PicNum - ( abs_diff_pic_num_minus1 + 1) ָ����Ҫ�������ͼ��
                } else if ( reordering_of_pic_nums_idc == 2) {

                    long_term_pic_num,  //ue(v) ���ڲο�֡������, ָ����Ҫ�������ͼ��
                }
            } while ( reordering_of_pic_nums_idc != 3 );
        }
    }

    if ( slice_type == B ) {
        ref_pic_list_reordering_flag_l1,    //u(1) �ο�֡���� L1

        if ( ref_pic_list_reordering_flag_l1 ) {
            do {
                reordering_of_pic_nums_idc,             //ue(v)

                if ( reordering_of_pic_nums_idc == 0 || reordering_of_pic_nums_idc == 1 ) {

                    abs_diff_pic_num_minus1,            //ue(v)
                } else if ( reordering_of_pic_nums_idc == 2 ) {

                    long_term_pic_num,                  //ue(v)
                }
            } while ( reordering_of_pic_nums_idc != 3 );
        }
    }
}


//��ȨԤ��䷨
pred_weight_table() {

    luma_log2_weight_denom, //ue(v) �����ο�֡�б��вο�ͼ���������ȵļ�Ȩϵ�����Ǹ���ʼֵ, ֵ�ķ�Χ�� [0, 7]
    
    chroma_log2_weight_denom,   //ue(v) �����ο�֡�б��вο�ͼ������ɫ�ȵļ�Ȩϵ�����Ǹ���ʼֵ, �Ǹ���ʼֵ, ֵ�ķ�Χ�� [0, 7]
    
    for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) {
        luma_weight_l0_flag,    //u(1) ���� 1 ʱ��ָ�����ڲο����� 0 �е����ȵļ�Ȩϵ�����ڣ�
                                    // ���� 0 ʱ���ڲο����� 0 �е����ȵļ�Ȩϵ��������

        if ( luma_weight_l0_flag ) {
            luma_weight_l0[ i ],    //se(v) �òο����� 0 Ԥ������ֵʱ�����õļ�Ȩϵ����
                                        // ��� luma_weight_l0_flag ==  0, luma_weight_l0[ i ] = 2^luma_log2_weight_denom
                                        
            luma_offset_l0[ i ],    //se(v) �òο����� 0 Ԥ������ֵʱ�����õļ�Ȩϵ���Ķ����ƫ�ơ�luma_offset_l0[i] ֵ�ķ�Χ[-128, 127],
                                        // ��� luma_weight_l0_flag is = 0, luma_offset_l0[ i ] = 0 
        }

        chroma_weight_l0_flag,  //u(1)

        if ( chroma_weight_l0_flag ) {
            for( j =0; j < 2; j++ ) {
                chroma_weight_l0[ i ][ j ],     //se(v)
                chroma_offset_l0[ i ][ j ],     //se(v)
            }
        }
    }

    if ( slice_type == B ) {
        for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ ) {
            luma_weight_l1_flag,        //u(1)

            if( luma_weight_l1_flag ) {
                luma_weight_l1[ i ],    //se(v)
                luma_offset_l1[ i ],    //se(v)
            }

            chroma_weight_l1_flag,      //u(1)

            if ( chroma_weight_l1_flag ) {
                for( j = 0; j < 2; j++ ) {
                    chroma_weight_l1[ i ][ j ], //se(v)
                    chroma_offset_l1[ i ][ j ], //se(v)
                }
            }
        }
    }
}


/*
    ǰ�Ľ��ܵ�������reordering�������ǶԲο�֡������������
    ����ǣ�marking���������𽫲ο�ͼ�� ���� �� �Ƴ� �ο�֡����
*/

/*
    -----------------------------------------------------------------------------------------------------------
    adaptive_ref_pic_marking_mode_flag                  ��ǣ�marking ��ģʽ
    -----------------------------------------------------------------------------------------------------------
        0                                       �����ȳ���FIFO����ʹ�û������Ļ��ƣ������ȳ���������ģʽ
                                                ��û�а취�Գ��ڲο�֡���в�����
    -----------------------------------------------------------------------------------------------------------
        1                                       ����Ӧ��ǣ�marking�������������л���һϵ�о䷨Ԫ����ʽָ
                                                �������Ĳ��衣����Ӧ��ָ�������ɸ��������������������ߡ�
    -----------------------------------------------------------------------------------------------------------
*/

/*
    --------------------------------------------------------------------------------------------------
    memory_management_control_operation                 ��ǣ�marking ������
    --------------------------------------------------------------------------------------------------
        0                                       ����ѭ�����˳���ǣ�marding��������
    --------------------------------------------------------------------------------------------------
        1                                       ��һ�����ڲο�ͼ����Ϊ�ǲο�ͼ��Ҳ
                                                ����һ�����ڲο�ͼ���Ƴ��ο�֡���С�
    --------------------------------------------------------------------------------------------------
        2                                       ��һ�����ڲο�ͼ����Ϊ�ǲο�ͼ��Ҳ
                                                ����һ�����ڲο�ͼ���Ƴ��ο�֡���С�
    --------------------------------------------------------------------------------------------------
        3                                       ��һ�����ڲο�ͼ��תΪ���ڲο�ͼ��
    --------------------------------------------------------------------------------------------------
        4                                       ָ�����ڲο�֡�������Ŀ��
    --------------------------------------------------------------------------------------------------
        5                                       ��ղο�֡���У������вο�ͼ���Ƴ��ο�
                                                ֡���У������ó��ڲο�����
    --------------------------------------------------------------------------------------------------
        6                                       ����ǰͼ���Ϊһ�����ڲο�֡��
    --------------------------------------------------------------------------------------------------
*/

//�ο�֡���б��(marking)�䷨
dec_ref_pic_marking() {

    if ( nal_unit_type == 5 ) {
        no_output_of_prior_pics_flag,   //u(1) ָ���Ƿ�Ҫ��ǰ���ѽ����ͼ��ȫ�����
        
        long_term_reference_flag,   //u(1) ����䷨Ԫ��ָ���Ƿ�ʹ�ó��ڲο�������ơ�
                                        // ���ȡֵΪ 1������ʹ�ó��ڲο�������ÿ�� IDR ͼ�񱻽�����Զ���Ϊ���ڲο�֡, 
                                        // ����ȡֵΪ 0����IDR ͼ�񱻽�����Զ���Ϊ���ڲο�֡
    } else {
        adaptive_ref_pic_marking_mode_flag, //u(1) ָ����ǣ�marking��������ģʽ

        if ( adaptive_ref_pic_marking_mode_flag ) {

            do {
                memory_management_control_operation,    //ue(v) ������Ӧ��ǣ�marking��ģʽ�У�ָ�����β����ľ������ݣ������������ 7.24

                if ( memory_management_control_operation == 1 || memory_management_control_operation == 3) {
                    difference_of_pic_nums_minus1,  //ue(v) ����䷨Ԫ�ؿ��Լ���õ���Ҫ������ͼ���ڶ��ڲο������е���š�
                                                        // �ο�֡�����б���������ͼ��
                }

                if ( memory_management_control_operation == 2 ) {
                    long_term_pic_num,  //ue(v) �Ӵ˾䷨Ԫ�صõ���Ҫ�����ĳ��ڲο�ͼ������
                }

                if ( memory_management_control_operation == 3 || memory_management_control_operation == 6 ) {
                    long_term_frame_idx,    //ue(v) ����һ�����ڲο�֡����Ÿ�һ��ͼ��
                }

                if ( memory_management_control_operation == 4 ) {
                    max_long_term_frame_idx_plus1,  //ue(v) ָ�����ڲο����е������Ŀ, ȡֵ��Χ [0, num_ref_frames]
                }
            } while ( memory_management_control_operation != 0 );
        }
    }
}

//Ƭ�����ݾ䷨
slice_data() {

    if ( entropy_coding_mode_flag ) {

        while ( !byte_aligned() ) {
            cabac_alignment_one_bit,    //f(1) ���ر���ģʽ�� CABAC ʱ,��ʱҪ�������ֽڶ���,�����ݴ���һ���ֽڵĵ�һ�����ؿ�ʼ,
                                            // �����û���ֽڶ��뽫�������ɸ� cabac_alignment_one_bit ��Ϊ���
        }
    }

    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag ),
    moreDataFlag = 1,
    prevMbSkipped = 0,

    do {
        if ( slice_type != I && slice_type != SI ) {

            if ( !entropy_coding_mode_flag ) {

                mb_skip_run,    //ue(v) ��ͼ�����֡��Ԥ�����ʱ��H.264 ������ͼ��ƽ̹������ʹ�á���Ծ���飬����Ծ���� ����Я���κ����ݣ�
                                    // ������ͨ����Χ���ؽ��ĺ����������ָ�����Ծ����.���ǿ��Կ���,���ر���Ϊ CAVLC �� CABAC ʱ,"��Ծ"��ı�ʾ������ͬ.
                                    // �� entropy_coding_mode_flag == 1�����ر���Ϊ CABAC ʱ, ÿ��"�� Ծ"�� �����о䷨Ԫ�� mb_skip_flag ָ��,
                                    // �� entropy_coding_mode_flag == 0�����ر���Ϊ CAVLC ʱ����һ���г̵ķ������������ŵġ���Ծ�������Ŀ��
                                    // ���䷨Ԫ�� mb_skip_run. mb_skip_run ֵ�ķ�Χ [0, PicSizeInMbs �C CurrMbAddr]

                prevMbSkipped = ( mb_skip_run > 0 ),
                for( i=0; i<mb_skip_run; i++ ) {
                    CurrMbAddr = NextMbAddress( CurrMbAddr ),
                }
                moreDataFlag = more_rbsp_data( ),
            } else {

                mb_skip_flag,   //ue(v)
                moreDataFlag = !mb_skip_flag,
            }
        }

        if ( moreDataFlag ) {

            if ( MbaffFrameFlag && ( CurrMbAddr%2 == 0 || ( CurrMbAddr % 2 = = 1 && prevMbSkipped ) )) {
                mb_field_decoding_flag, //u(1) | ae(v) ��֡������Ӧͼ���У�ָ����ǰ��������ĺ�����֡ģʽ���ǳ�ģʽ��
                                            // 0 -- ֡ģʽ; 1 -- ��ģʽ�����һ�����Ե��������䷨�ṹ�ж�û�г�������䷨Ԫ�أ�
                                            // �����Ƕ��ǡ���Ծ����ʱ�����䷨Ԫ�������¾��� :
                // ���������������ڵġ���ߵĺ�������ͬһ��Ƭʱ��������Ե� mb_field_decoding_flag ��ֵ������ߵĺ��Ե� mb_field_decoding_flag ��ֵ��
                // ����, ������� �� mb_field_decoding_flag ��ֵ�����ϱ�ͬ����һ��Ƭ�� ���� �� mb_field_decoding_flag ��ֵ
                // ������ ���� ��û�����ڵġ��ϱ�ͬ����һ��Ƭ�ĺ���,Ҳû�����ڵġ����ͬ����һ��Ƭ�ĺ��ԣ�������Ե� mb_field_decoding_flag ��ֵ����0����֡ģʽ
            }

            macroblock_layer(),
        }

        if ( !entropy_coding_mode_flag ) {
            moreDataFlag = more_rbsp_data( ),
        } else {

            if ( slice_type != I && slice_type != SI ) {
                prevMbSkipped = mb_skip_flag,
            }

            if( MbaffFrameFlag && CurrMbAddr % 2 = = 0 ) {
                moreDataFlag = 1,
            } else {
                end_of_slice_flag,  //ae(v) ָ���Ƿ���Ƭ�Ľ�β
                moreDataFlag = !end_of_slice_flag,
            }
        }

        CurrMbAddr = NextMbAddress( CurrMbAddr ),

    } while ( moreDataFlag );
}

/*
    --------------------------------------------
    Ƭ����                  ������ֵĺ������
    --------------------------------------------
    I (slice)               I ���
    --------------------------------------------
    P (slice)               P ��顢 I ���
    --------------------------------------------
    B (slice)               B ��顢 I ���
    --------------------------------------------
    SI (slice)              SI ��顢 I ���
    --------------------------------------------
    SP (slice)              P ��顢 I ���
    --------------------------------------------
*/

/*
    ��֡��Ԥ��ģʽ�£��������������˶�ʸ���Ļ��ַ�����
    ��֡��Ԥ��ģʽ�£�������֡�� 16x16 Ԥ�⣬��ʱ���Ժ��������Ԥ�ⷽ��������������, 
                    Ҳ������ 4x4 Ԥ�⣬��ʱÿ�� 4x4 ������о���Ԥ�ⷽ����������鹲�� 144 �����͡�
    mb_type �������������������йغ�����͵���Ϣ��
    ��ʵ�Ͽ�����ᵽ��mb_tye �ǳ����ں���ĵ�һ���䷨Ԫ�أ�����������������йصĻ�����������Ϣ
*/

/*
    -------------------------------------------------------------------------------------------------
    CodedBlockPatternChroma                     ����
    -------------------------------------------------------------------------------------------------
            0                   ���вв�������ͣ������������вв�ϵ����Ϊ0��
    -------------------------------------------------------------------------------------------------
            1                   ֻ��DCϵ�������ͣ�������������ACϵ����Ϊ0��
    -------------------------------------------------------------------------------------------------
            2                   ���вв�ϵ��������DC��AC���������͡��������ý��յ��Ĳв�ϵ���ؽ�ͼ��
    -------------------------------------------------------------------------------------------------
*/

/*
    |<-----  macroblock --------->|
    +----------+-----------+------+
    | mac_type | pred_type | data |
    +----------+-----------+------+
*/
//����䷨
macroblock_layer() {

    mb_type,    //ue(v) | ae(v) ָ����ǰ�������͡�H.264�涨����ͬ��Ƭ��������ֵĺ������Ҳ��ͬ

    if ( mb_type == I_PCM ) {
        while ( !byte_aligned() ) {
            pcm_alignment_zero_bit, //f(1) 0
        }

        for( i = 0; i < 256 * ChromaFormatFactor; i++) {
            pcm_byte[ i ],  //u(8)  ǰ 256 pcm_byte[i] ��ֵ ������������ ��ֵ
                                // ( 256 * ( ChromaFormatFactor - 1 ) ) / 2 �� pcm_byte[i] �� ֵ �� �� Cb �� �� �� ֵ
                                // �� �� һ ��( 256 * ( ChromaFormatFactor - 1 ) ) / 2 �� pcm_byte[i]��ֵ���� Cr ������ֵ
        }
    } else {
        if( MbPartPredMode( mb_type, 0 ) != Intra_4x4 &&
            MbPartPredMode( mb_type, 0 ) != Intra_16x16 &&
            NumMbPart( mb_type ) == 4 ) {

            sub_mb_pred( mb_type ),
        } else {
            mb_pred( mb_type ),
        }

        if( MbPartPredMode( mb_type, 0 ) != Intra_16x16 ) {

            // ����䷨Ԫ��ͬʱ������һ����������ȡ�ɫ�ȷ����� CBP�����Ե�һ�������ȷֱ��������������� CBP ��ֵ�����У�����ɫ�ȷ����� CBP ����ͬ�ġ�
            // ���� CodedBlockPatternLuma �����ȷ����� CBP������ CodedBlocPatternChroma ��ɫ�ȷ����� CBP�����ڷ� Intra_16x16 �ĺ�����ͣ�
            // CodedBlockPatternLuma = coded_block_pattern % 16
            // CodedBlockPatternChroma = coded_block_pattern / 16
            // ����Intra_16x16������ͣ�CodedBlockPatternLuma ��CodedBlockPatternChroma ��ֵ�����ɱ��䷨Ԫ�ظ���������ͨ�� mb_type �õ�

            // 
            // CodedBlockPatternLuma����һ��16λ�ı���������ֻ�������λ�ж��塣���ڷ� Intra_16x16 �ĺ�鲻��������DCϵ���������������ָֻ�����ֱ��뷽����
            // �в�ȫ�������ȫ�������롣���������λ���ش����λ��ʼ��ÿһλ��Ӧһ���Ӻ�飬��λ����1ʱ������Ӧ�Ӻ��в�ϵ�������ͣ�
            // ��λ����0ʱ������Ӧ�Ӻ��в�ȫ���������ͣ�����������Щ�в�ϵ����Ϊ0��

            //
            // CodedBlockPatternChroma����ֵΪ0��1��2ʱ�ж���
            
            coded_block_pattern,    //me(v) | ae(v) �� CBP��ָ���Ⱥ�ɫ�ȷ����ĸ�С��Ĳв�ı��뷽������ν���뷽�������¼��� :
                // a)  ���вв���� DC��AC��������
                // b)  ֻ�� DC ϵ������
                // c)  ���вв���� DC��AC����������
        }

        if( CodedBlockPatternLuma > 0 || CodedBlockPatternChroma > 0 ||
            MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {

            mb_qp_delta,    //se(v) | ae(v) �ں����е�����������ƫ��ֵ��mb_qp_delta ֵ�ķ�Χ�� [-26, +25]
                                // ������������ͼ���������Ƭͷ��������������ģ��������ڽ�����������������¹�ʽ�õ� :
                                // QPy = ( QPy,prev + mb_qp_delta + 52 ) % 52 
                                // QPy,prev �ǵ�ǰ��鰴�ս���˳���ǰһ�������������������ǿ��Կ�����mb_qp_delta ��ָʾ��ƫ����ǰ���������֮���ƫ�ơ�
                                // ������Ƭ�е�һ������ QPy,prev ���� 7-16 ʽ����
                                // QPy,prev = 26 + pic_init_qp_minus26 + slice_qp_delta
            residual(),
        }
    }
}


//����Ԥ��䷨
mb_pred( mb_type ) {
    if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ||
        MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {

        if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ) {
            for( luma4x4BlkIdx=0; luma4x4BlkIdx<16; luma4x4BlkIdx++ ) {

                // ֡��Ԥ���ģʽҲ����ҪԤ���
                // ����ָ��֡��Ԥ��ʱ�����ȷ�����Ԥ��ģʽ��Ԥ��ֵ�Ƿ������ʵԤ��ģʽ������ǣ��Ͳ��������ٴ�Ԥ��ģʽ��
                // ������ǣ����� rem_intra4x4_pred_mode ָ����ʵԤ��ģʽ
                prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ],  //u(1) | ae(v)

                if( !prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] ) {
                    
                    rem_intra4x4_pred_mode[ luma4x4BlkIdx ],    //u(3) | ae(v) 
                }
            }
        }

        /*
            intra_chroma_pred_mode  Ԥ��ģʽ
            0                       DC
            1                       Horizontal
            2                       Vertical
            3                       Plane
            */

        intra_chroma_pred_mode, //ue(v) | ae(v) ��֡��Ԥ��ʱָ��ɫ�ȵ�Ԥ��ģʽ
    } else if ( MbPartPredMode( mb_type, 0 ) != Direct ) {

        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++) {
            if( ( num_ref_idx_l0_active_minus1 > 0 ||
                    mb_field_decoding_flag ) && 
                    MbPartPredMode( mb_type, mbPartIdx ) != Pred_L1 ){


                // �òο�֡���� L0 ����Ԥ�⣬��ǰ��Ԥ��ʱ���ο�ͼ���ڲο�֡�����е���š� ���� mbPartIdx �Ǻ����������
                // ��� ��ǰ����Ƿǳ���� , ��  ref_idx_l0[ mbPartIdx ]  ֵ�ķ�Χ�� [0, num_ref_idx_l0_active_minus1]
                // ���������ǰ����ǳ����, (�������ͼ���ǳ�����ͼ����֡������Ӧʱ��ǰ��鴦�ڳ�����ĺ���),
                // ref_idx_l0[ mbPartIdx ] ֵ�ķ�Χ�� [0, 2 * num_ref_idx_l0_active_minus1 + 1], ��ǰ��������ʱ�ο�֡���е�֡������ɳ����ʲο����г��ȼӱ���
                ref_idx_l0[ mbPartIdx ],    //te(v) | ae(v)
            }
        }

        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++) {
            if( ( num_ref_idx_l1_active_minus1 > 0 | |
                    mb_field_decoding_flag ) &&
                    MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 ) {

                ref_idx_l1[ mbPartIdx ],    //te(v) | ae(v)
            }
        }

        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++) {
            if( MbPartPredMode ( mb_type, mbPartIdx ) != Pred_L1 ) {
                for( compIdx = 0; compIdx < 2; compIdx++ ) {
                    mvd_l0[ mbPartIdx ][0][ compIdx ],  //se(v) | ae(v) �˶�ʸ����Ԥ��ֵ��ʵ��ֵ֮��ĲmbPartIdx �Ǻ���������š�
                            // CompIdx = 0 ʱˮƽ�˶�ʸ��; CompIdx = 1 ��ֱ�˶�ʸ��
                }
            }
        }

        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++) {
            if( MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 ) {
                for( compIdx = 0; compIdx < 2; compIdx++ ) {
                    mvd_l1[ mbPartIdx ][0][ compIdx ],  //se(v) | ae(v)
                }
            }
        }
    }
}


//�Ӻ��Ԥ��䷨
sub_mb_pred( mb_type ) {

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        sub_mb_type[ mbPartIdx ],   //ue(v) | ae(v) ָ���Ӻ���Ԥ�����ͣ��ڲ�ͬ�ĺ������������䷨Ԫ�ص����岻һ��
    }

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        if( ( num_ref_idx_l0_active_minus1 > 0 | |
                mb_field_decoding_flag ) &&
                mb_type != P_8x8ref0 &&
                sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
                SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 ) {

            ref_idx_l0[ mbPartIdx ],    //te(v) | ae(v)
        }
    }

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        if( (num_ref_idx_l1_active_minus1 > 0 | | mb_field_decoding_flag ) &&
            sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 ) {

            ref_idx_l1[ mbPartIdx ],    //te(v) | ae(v)
        }
    }

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 ) {

            for( subMbPartIdx = 0;subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );subMbPartIdx++) {
                for( compIdx = 0; compIdx < 2; compIdx++ ) {
                    mvd_l0[ mbPartIdx ][ subMbPartIdx ][ compIdx ],     //se(v) | ae(v)
                }
            }
        }
    }

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 && SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 ) {
            for( subMbPartIdx = 0;subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );subMbPartIdx++) {
                for( compIdx = 0; compIdx < 2; compIdx++ ) {
                    mvd_l1[ mbPartIdx ][ subMbPartIdx ][ compIdx ],     //se(v) | ae(v)
                }
            }
        }
    }
}


//�в�䷨
residual() {

    if ( !entropy_coding_mode_flag ) {
        residual_block = residual_block_cavlc,
    } else {
        residual_block = residual_block_cabac,
    }

    if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {
        residual_block( Intra16x16DCLevel, 16 )
    }

    for( i8x8 = 0; i8x8 < 4; i8x8++ ) {
        for( i4x4 = 0; i4x4 < 4; i4x4++ ) {
            if( CodedBlockPatternLuma & ( 1 << i8x8 ) ) {
                if( MbPartPredMode( mb_type, 0 ) = = Intra_16x16 ) 
                    residual_block( Intra16x16ACLevel[ i8x8 * 4 + i4x4 ], 15 )
                else 
                    residual_block( LumaLevel[ i8x8 * 4 + i4x4 ], 16 )
            } else {
                if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {
                    for( i = 0; i < 15; i++ ) {
                        Intra16x16ACLevel[ i8x8 * 4 + i4x4 ][ i ] = 0,
                    }
                } else {
                    for( i = 0; i < 16; i++ ) {
                        LumaLevel[ i8x8 * 4 + i4x4 ][ i ] = 0,
                    }
                }
            }
        }
    }

    for( iCbCr = 0; iCbCr < 2; iCbCr++ ) {
        if( CodedBlockPatternChroma & 3 )
            residual_block( ChromaDCLevel[ iCbCr ], 4 ),
        else {
            for( i = 0; i < 4; i++ ) {
                ChromaDCLevel[ iCbCr ][ i ] = 0,
            }
        }
    }

    for( iCbCr = 0; iCbCr < 2; iCbCr++ ) {
        for( i4x4 = 0; i4x4 < 4; i4x4++ ) {
            if( CodedBlockPatternChroma & 2 )
                residual_block( ChromaACLevel[ iCbCr ][ i4x4 ], 15 )
            else {
                for( i = 0; i < 15; i++ ) {
                    ChromaACLevel[ iCbCr ][ i4x4 ][ i ] = 0,
                }
            }
        }
    }
}

//CAVLC �в�䷨
residual_block_cavlc( coeffLevel, maxNumCoeff ) {

    for( i = 0; i < maxNumCoeff; i++ ) {
        coeffLevel[ i ] = 0,
    }

    coeff_token,    //ce(v) ָ���˷���ϵ���ĸ�������βϵ���ĸ���

    if( TotalCoeff( coeff_token ) > 0 ) {
        if( TotalCoeff( coeff_token ) > 10 && TrailingOnes( coeff_token ) < 3 )
            suffixLength = 1
        else 
            suffixLength = 0

        for( i = 0; i < TotalCoeff( coeff_token ); i++ ) {
            if( i < TrailingOnes( coeff_token ) ) {
                trailing_ones_sign_flag,    //u(1) ��βϵ���ķ���
                        // ���trailing_ones_sign_flag = 0, ��Ӧ����βϵ����+1
                        // ����trailing_ones_sign_flag =1����Ӧ����βϵ����-1
                level[ i ] = 1 �C 2 * trailing_ones_sign_flag,
            } else {
                level_prefix,   //ce(v) ����ϵ��ֵ��ǰ׺�ͺ�׺
                levelCode = ( level_prefix << suffixLength ),

                if( suffixLength > 0 | | level_prefix >= 14 ) {
                    level_suffix,   //u(v) ����ϵ��ֵ��ǰ׺�ͺ�׺
                    levelCode += level_suffix,
                }

                if( level_prefix == 15 && suffixLength == 0 ) {
                    levelCode += 15
                }

                if( i == TrailingOnes( coeff_token ) && TrailingOnes( coeff_token ) < 3 ) {
                    levelCode += 2 
                }

                if( levelCode % 2 == 0 )
                    level[ i ] = ( levelCode + 2 ) >> 1
                else
                    level[ i ] = ( �ClevelCode �C 1 ) >> 1

                if( suffixLength == 0 ) 
                    suffixLength = 1

                if( Abs( level[ i ] ) > ( 3 << ( suffixLength �C 1 ) ) && suffixLength < 6 )
                    suffixLength++
            }
        }
    }

    if( TotalCoeff( coeff_token ) < maxNumCoeff ) {
        total_zeros,    //ce(v) ϵ���� 0 ���ܸ���
        zerosLeft = total_zeros
    } else {
        zerosLeft = 0
    }

    for( i = 0; i < TotalCoeff( coeff_token ) �C 1; i++ ) {
        if( zerosLeft > 0 ) {
            run_before, //ce(v) �ڷ���ϵ��֮ǰ������ĸ���
            run[ i ] = run_before,
        } else {
            run[ i ] = 0,
        }

        zerosLeft = zerosLeft �C run[ i ],
    }

    run[ TotalCoeff( coeff_token ) �C 1 ] = zerosLeft,

    coeffNum = -1,

    for( i = TotalCoeff( coeff_token ) �C 1; i >= 0; i-- ) {
        coeffNum += run[ i ] + 1,
        coeffLevel[ coeffNum ] = level[ i ],
    }
}

// CABAC �в�䷨
residual_block_cabac( coeffLevel, maxNumCoeff ) {
    coded_block_flag,   //ae(v) ָ����ǰ���Ƿ��������ϵ��
            // ��� coded_block_flag= 0, ����鲻��������ϵ����
            // ��� coded_block_flag = 1��������������ϵ����

    if ( coded_block_flag ) {
        numCoeff = maxNumCoeff,
        i = 0,

        do {

            significant_coeff_flag[ i ],    //ae(v) ָ����λ��Ϊ i ���ı任ϵ���Ƿ�Ϊ��
                    // ��� significant_coeff_flag[ i ] = 0, ��λ��Ϊ i ���ı任ϵ��Ϊ�㡣
                    // ����significant_coeff_flag[ i ] =1, ��λ��Ϊ i ���ı任ϵ����Ϊ�㡣

            if ( significant_coeff_flag[ i ] ) {
                last_significant_coeff_flag[ i ],   //ae(v) ��ʾ��ǰλ�� i ���ı任ϵ���Ƿ�Ϊ�������һ������ϵ��
                        // ��� last_significant_coeff_flag[ i ] =1, �����������ϵ����Ϊ��
                        // ����, �����������ϵ���л��������ķ���ϵ��.

                if ( last_significant_coeff_flag[ i ] ) {
                    numCoeff = i + 1,

                    for( j = numCoeff; j < maxNumCoeff; j++ )
                        coeffLevel[ j ] = 0
                }
            }

            i++, 
        } while ( i < numCoeff-1 );

        coeff_abs_level_minus1[ numCoeff-1 ],       //ae(v) ϵ���ľ���ֵ�� 1��
        coeff_sign_flag[ numCoeff-1 ],              //ae(v) ϵ���ķ���λ��
                // coeff_sign_flag = 0, ������
                // coeff_sign_flag = 1, ������

        coeffLevel[ numCoeff-1 ] = ( coeff_abs_level_minus1[ numCoeff-1]+1)*(1-2* coeff_sign_flag[numCoeff-1] ),

        for( i = numCoeff-2; i >= 0; i-- ) {
            if( significant_coeff_flag[ i ] ) {
                coeff_abs_level_minus1[ i ],        //ae(v)
                coeff_sign_flag[ i ],               //ae(v)

                coeffLevel[ i ] = ( coeff_abs_level_minus1[ i ] + 1 ) *( 1 �C 2 * coeff_sign_flag[ i ] ),
            } else 
                coeffLevel[ i ] = 0
        }
    } else {
        for( i = 0; i < maxNumCoeff; i++ ) {
            coeffLevel[ i ] = 0,
        }
    }
}

/*
	h264_mp4toannexb   -----------------  aac_adtstoasc
	
	h264�����ַ�װ:
		1) һ����annexbģʽ����ͳģʽ����startcode��SPS��PPS����ES��
		2) һ����mp4ģʽ��һ��mp4 mkv���У�û��startcode��SPS��PPS�Լ�������Ϣ����װ��container�У�ÿһ��frameǰ�������frame�ĳ���

	��ffmpeg����h264_mp4toannexb_filter������ת��:
		1) ע��filter, avcbsfc = av_bitstream_filter_init("h264_mp4toannexb");
		2) ת��bitstream
			av_bitstream_filter_filter(AVBitStreamFilterContext *bsfc,
                               AVCodecContext *avctx, const char *args,
                     uint8_t **poutbuf, int *poutbuf_size, const uint8_t *buf, int buf_size, int keyframe)
*/


// RTO��Retransmission TimeOut���ش���ʱʱ��
// RTT��Round Trip Time������ʱ��
// RTT ����������ɣ���·�Ĵ���ʱ�䣨propagation delay����ĩ��ϵͳ�Ĵ���ʱ�䡢·���������е��ŶӺʹ���ʱ�䣨queuing delay����
// ���У�ǰ�������ֵ�ֵ����һ��TCP������Թ̶���·���������е��ŶӺʹ���ʱ���������������ӵ���̶ȵı仯���仯��
// ����RTT�ı仯��һ���̶��Ϸ�Ӧ�������ӵ���̶ȡ�

//======================================================
//  ��ʱ�ش�
//======================================================
/*
    ���Ͷ�����ڷ������ݰ���T1��ʱ��һ��RTO֮��δ�յ�������ݰ���ACK��Ϣ����ô���;��ش�������ݰ���
*/

//======================================================
//  �����ش�
//======================================================
/*
    ���ն��ڷ���ACK��ʱ��Я���Լ���ʧ���ĵ���Ϣ���������Ͷ˽��յ�ACK��Ϣʱ���ݶ����������б����ش���
*/

//======================================================
//  FEC ѡ���ش� FEC��Forward Error Correction��ǰ�����
//======================================================
/*
    �ڷ��ͷ����ͱ��ĵ�ʱ�򣬻����FEC��ʽ�Ѽ������Ľ���FEC���飬ͨ��XOR�ķ�ʽ�õ����ɸ��������Ȼ��һ�������ն�
    ������ն˷��ֶ�������ͨ��FEC�����㷨��ԭ���Ͳ����Ͷ������ش�
    ��������ڰ��ǲ��ܽ���FEC�ָ��ģ��������뷢�Ͷ�����ԭʼ�����ݰ���
*/



//======================================================
//  ����ӵ�������㷨
//======================================================
/*
    һ�� ��������slow start��
        ��������·�ոս����󣬲�����һ��ʼ��cwnd���õĺܴ�����������ɴ����ش�������ӵ��������ڿ�ʼ��cwnd = 1,
        Ȼ�����ͨ�Ź��̵Ķ�����������cwnd����Ӧ��ǰ������״̬��ֱ���ﵽ��������������ֵ(ssthresh),�������£�
        1) ��ʼ������cwnd = 1,����ʼ�������� 
        2) �յ�������ACK,�Ὣcwnd ��1 
        3) ���Ͷ�һ��RTT����δ�����ж����ش����ͻὫcwnd= cwnd * 2. 
        4) ��cwnd >= ssthresh���������ش�ʱ���������������� ӵ������״̬��
        
    ���� ӵ������
        ��ͨ������ ���������� ���п��ܻ�δ�����紫���ٶȵ����ߣ����ʱ����Ҫ��һ��ͨ��һ�������ĵ��ڹ������������䡣
        һ����һ��RTT�����δ���ֶ��������ǽ�cwnd = cwnd + 1��һ�����ֶ����ͳ�ʱ�ش����ͽ��� ӵ������״̬��
        
    ���� ӵ������
        ӵ��������TCP����ʵ�ֺܱ�����������������ش���ֱ�ӽ�cwnd = cwnd / 2��Ȼ����� ���ٻָ�״̬��
        
    �ġ� ���ٻָ�
        ���ٻָ���ͨ��ȷ�϶���ֻ�����ڴ���һ��λ�õİ�����ȷ���Ƿ���п��ٻָ�����ͼ6��������
        ���ֻ��104�����˶�ʧ����105,106���յ��˵ģ���ôACK���ǻὫack��base = 103,
        �������3���յ�baseΪ103��ACK,�ͽ��п��ٻָ���Ҳ���������ش�104����������յ��µ�ACK��base > 103,��cwnd = cwnd + 1,������ ӵ������״̬��
*/

#endif
