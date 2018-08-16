
#ifndef _H_264_H_
#define _H_264_H_

/*

    帧内预测模式全搜索算法

    < Step 1-3, 计算 单个 4x4块 的 9中模式 的Cost, 选取最小 Cost_4x4
    < Step 5-7, 计算 16x16块 的单个模式的 Cost_16x16

    Step l：对当前 4X4亮度块 选择 一种模式 进行预测，计算 原始块 和 预测块 的 残差值;

    Step 2：根据 SAD函数 或 RDO函数 计算该模式的代价值 Cost_4x4;

    Step 3: 对当前 4X4块 重复步骤 1～2, 直到 9种 预测模式 都计算完, 选择 最小 Cost_4x4;

    Step 4: 对当前宏块内的 16个 4X4块 重复步骤 l～3, 然后对所有 Cost_4x4 求和;

    Step 5: 对 16X16宏块 选择 一种模式 进行预测, 将 残差宏块 的每个 4×4块 都进行 Hadamard变换;

    Step 6: 从变换后的 16X16宏块 中分别提取 16个 4X4块 DC系数的1／4 形成另一个 4X4块, 再对 DC系数块 进行 Hadamard变换;

    Step 7：对所有 Hadamard变换系数的 绝对值 求和, 取 和的l／2 作为 代价函数值 Cost_16x16;

    Step 8: 重复以上步骤的 5～7, 直到该 16X16宏块 的 4种预测模式 都计算完, 选择最小的代价函数值 Cost_16x16;

    Step 9：比较以上步骤4,8, 的代价函数值

                                   i=16
                if ( Cost_16x16 >=  ∑Cost_4x4_i + 24入(QP) )
                                   i=1
                {
                    当前宏块选取使 4X4代价值最小的 预测模式 进行编码;
                }
                else
                {
                    选取使16×16代价值最小的预测模式进行编码;
                }

    Step 10：对于8×8色度块, 则是将 4种 预测模式 中的 每一种 色度 预测模式 重复一次以上所有步骤, 即 8×8色度块 的预测 是在最外层循环;


    由此可见，一个宏块内的模式组合数为:

        M_8 x ( M_4 x 16 + M_16 ),  M_8 = 4, M_4 = 9, M_16 = 4;

    即要计算 592个 不同的 代价函数 以得到 最优 色度模式 和 最优 亮度宏块模式 的组合。
                
*/


////////  帧内预测快速算法
/*

    空间相邻块相关的帧内预测

    
    一、 对于纹理简单、平坦的图像块适合按照 16×16 大小进行预测，
         对于纹理复杂、细节丰富的图像块则更适合按照 4×4 大小进行预测。
         
    二、 判断图像的细节是否丰富，即是计算图像中像素灰度值与灰度均值的偏离程度，我们取均方差来表示这个度量值。

                  1  i=16 j=16         _
            V = ---- ∑   ∑  (x_i,j - x)^2
                 16  i=1  j=1


            因为 DC模式不具有方向性，所以在选择候选模式时还应考虑到 DC模式。
            
                     1  i=4 j=4          _
            V_dc = ---- ∑  ∑  (x_i,j - x)^2
                     4  i=1 j=1

    三、 流程图

        1) 计算当前 16x16 块的 V 值

        if ( V <= TV1 ) //4 选 1
        {
            16x16 模式预测,  ===> 16x16 的最佳预测模式即为 当前块的预测模式
        } 
        else if ( TV1 < V <= TV2 ) // 13 选 1
        {
            16x16 模式预测, 4x4 模式预测,  ===> 比较 4x4, 16x16 预测的最佳模式的 RDCose 值, 较小者为 当前块的预测模式
        } 
        else if ( V > TV2 ) //9 选 1
        {
            4x4 模式预测,   ===> 4x4 的最佳预测模式即为 当前块的预测模式
        }


    四、 块间预测 流程图

        1) 根据当前块的相邻块的模式 ModeA 和 ModeB 计算当前块最可能的预测模式 MPM (ModeA, ModeB)

        if ( V_dc <= Tdc )
        {
            记 DC模式 为候选预测模式, ===> 计算 MPM (ModeA, ModeB) 和 Mode_DC 的 RDCost, 取最小值的对应模式为 当前 4x4块 的预测模式
        }
        else 
        {
            计算 8种 方向掩模的 D值, 将其最小值对应的模式记为 Mode_D,  ===> 
            计算 MPM (ModeA, ModeB) 和 Mode_D 的 RDCost, 取最小值的对应模式为 当前 4x4块 的预测模式
        }
*/

/*
    一、 描述子是指从比特流提取句法元素的方法，即句法元素的解码算法
            由于 H.264 
编码的最后一步是熵编码，所以这里的描述子大多是熵编码的解码算法

    H.264定义了如下几种描述子 :
        a)  ae(v) 基于上下文自适应的二进制 算术熵编码
        b)  b(8) 读进连续的 8 个比特
        c)  ce(v) 基于上下文自适应的 可变长熵编码
        d)  f(n) 读进连续的 n 个比特
        e)  i(n)/i(v) 读进连续的若干比特，并把它们解释为有符号整数
        f)  me(v) 映射指数 Golomb 熵编码
        g)  se(v) 有符号指数 Golomb 熵编码
        h)  te(v) 截断指数 Golomb 熵编码
        i)  u(n)/u(v) 读进连续的若干比特，并将它们解释为无符号整数
        j)  ue(v)  无符号指数 Golomb 熵编码
    
    1. ue(v)：无符号整数指数哥伦布码编码的语法元素，左位在先
    2. se(v)：有符号整数指数哥伦布码编码的语法元素，左位在先
    3. u(n)：n位无符号整数
    4. 在语法表中, 如果n是'v', 其比特数由其它语法元素值确定. 解析过程由函 数 
read_bits(n) 的返回值规定, 该返回值用最高有效位在前的二进制表示
*/

/*
nal_unit_type       NAL 类型                 C
0                   未使用 
1                   不分区、非 IDR 图像的片  2, 3, 4
2                   片分区 A                 2
3                   片分区 B                 3
4                   片分区 C                 4
5                   IDR 图像中的片           2, 3
6                   补充增强信息单元（SEI）  5
7                   序列参数集               0
8                   图像参数集               1
9                   分界符                   6
10                  序列结束                 7
11                  码流结束                 8
12                  填充                     9
13..23              保留 
24..31              未使用
*/

/*
    从 SODB 到 RBSP 的生成过程：
    -- 如果 SODB 内容是空的，生成的 RBSP 也是空的
    -- 否则，RBSP 由如下的方式生成：
        1） RBSP 的第一个字节直接取自 SODB 的第 1 到 8 个比特，（RBSP 字节内的比特按照从左到右对应为从高到低的顺序排列，most significant）,以此类推，RBSP 
            其余的每个字节都直接取自 SODB 的相应比特。RBSP 的最后一个字节包含 SODB 的最后几个比特，及如下的 rbsp_trailing_bits()

        2） rbsp_trailing_bits()的第一个比特是 1,接下来填充 0，直到字节对齐。（填充 0 的目的也是为了字节对齐）

        3） 最后添加若干个 cabac_zero_word(其值等于 0x0000)
*/

NAL_Unit( int NumBytesInNALunit ) {
    char forbidden_zero_bit,                 //f(1) == 0
    char nal_ref_idc,                        //u(2) 指示当前 NAL 的优先级, 取值范围为 0-3, ,值越高,表示当前 NAL 越重要,需要优先受到保护
                                                // H.264 规定如果当前 NAL 是属于参考帧的片，或是序列参数集，或是图像参数集这些重要的数据单位时，
                                                // 本句法元素必须大于 0。但在大于 0 时具体该取何值，却没有进一步规定,通信双方可以灵活地制定策略
    char nal_unit_type,                      //u(5) nal_unit_type=5 时，表示当前 NAL 是 IDR 图像的一个片，
                                                // 在这种情况下，IDR 图像中的每个片的nal_unit_type 都应该等于 5。注意 IDR 图像不能使用片分区。

    int rbsp_byte[1024];
    int i, NumBytesInRBSP = 0, emulation_prevention_three_byte,

    for( i = 1; i < NumBytesInNALunit; i++ ) {

        //当解码器在 NAL 内部检测到有 0x000003 的序列时，将把 0x03 抛弃，恢复原始数据

        //  0x000000  --->  0x00000300
        //  0x000001  --->  0x00000301
        //  0x000002  --->  0x00000302
        //  0x000003  --->  0x00000303
        
        if( i + 2 < NumBytesInNALunit && next_bits( 24 ) == 0x000003 ) {
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8) 
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8)
            i += 2,
            emulation_prevention_three_byte, //f(8) NAL 内部为防止与起始码竞争而引入的填充字节 ,值为 0x03。
        } else 
            rbsp_byte[ NumBytesInRBSP++ ],  //b(8) RBSP 指原始字节载荷，它是 NAL 单元的数据部分的封装格式，
                                                // 封装的数据来自 SODB（原始数据比特流）。
                                                // SODB 是编码后的原始数据，SODB 经封装为 RBSP 后放入 NAL 的数据部分。
    }
}

/*
    Exp-Golomb 编码技术

    Exp-Golomb 码是 Yuji Itoh 在[9]中提出的 UVLC Universal Variable Length Coding
    编码方案在阶为 2 时的特例 它有固定的编码结构 如下:

    [0, . ,0][1][INFO...]
    |<- M ->|   |<- M ->|

        1) 它是由 ( M bit ) 前导的 0,
        2) 1 比特的 1,
        3) ( M bit) 的信息位构成,

    其中 M 由公式 4.4.1-1 确定:

    M    = [LOG 2 (code_num + 1)] (向下取整)
    INFO = code_num + 1 - 2^M

        1) code_num 也即所要编的符号值
        2) 符号 0 没有前导和 INFO 直接编码为 1

    下表列出了 Exp-Golomb 码表的前 8 个结构

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

    //指明所用 profile、level。
    profile_idc,    //u(8) 标识当前 H.264 码流的 profile
                        //66 -- baseline    profile
                        //77 -- main        profile
                        //88 -- extended    profile

    //注意: 当 constraint_set0_flag,constraint_set1_flag,constraint_set2_flag 中的两个以上等于 1 时，A.2中的所有制约条件都要被遵从
    constraint_set0_flag,   //u(1) 等于 1 时表示必须遵从附录 A.2.1 所指明的所有制约条件。等于 0 时表示不必遵从所有条件
    constraint_set1_flag,   //u(1) 等于 1 时表示必须遵从附录 A.2.2 所指明的所有制约条件。等于 0 时表示不必遵从所有条件
    constraint_set2_flag,   //u(1) 等于 1 时表示必须遵从附录 A.2.3 所指明的所有制约条件。等于 0 时表示不必遵从所有条件
    
    reserved_zero_5bits,    //u(5) 在目前的标准中本句法元素必须等于 0，其他的值保留做将来用，解码器应该忽略本句法元素的值
    
    //指明所用 profile、level。
    level_idc,  //u(8) 标识当前码流的 Level, 编码的Level定义了某种条件下的最大视频分辨率、最大视频帧率等参数，码流所遵从的 level由 level_idc 指定

    //注意：当编码器需要产生新的序列参数集时，应该使用新的 seq_parameter_set_id,即使用新的序列参数集，而不是去改变原来的参数集中的内容
    seq_parameter_set_id,   //ue(v) 指明本序列参数集的 id 号，这个 id 号将被 picture 参数集引用，本句法元素的值应该在[0，31]


    //值得注意的是 frame_num 是循环计数的，即当它到达 MaxFrameNum 后又从 0 重新开始新一轮的计数。
    //解码器必须要有机制检测这种循环，不然会引起类似千年虫的问题，在图像的顺序上造成混乱。在第八章会详细讲述 H.264 检测这种循环的机制
    log2_max_frame_num_minus4,  //ue(v) 这个句法元素主要是为读取另一个句法元素 frame_num 服务的，frame_num 是最重要的句法元素之一，
                                    // 它标识所属图像的解码顺序。可以在句法表看到，fram-num 的解码函数是 ue（v），函数中的 v 在这里指定：
                                    // v = log2_max_frame_num_minus4 + 4
                                    //
                                    // 从另一个角度看，这个句法元素同时也指明了 frame_num 的所能达到的最大值 :
                                    // MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 )
                                    // 变量 MaxFrameNum 表示 frame_num 的最大值，在后文中可以看到，在解码过程中它也是一个非常重要的变量
    
    pic_order_cnt_type, //ue(v) 指明了 poc (picture order count) 的编码方法，poc 标识图像的播放顺序。
                            // 由于H.264 使用了 B 帧预测，使得图像的解码顺序并不一定等于播放顺序，但它们之间存在一定的映射关系。
                            // poc 可以由 frame-num 通过映射关系计算得来，也可以索性由编码器显式地传送。
                            // H.264 中一共定义了三种 poc 的编码方法，这个句法元素就是用来通知解码器该用哪种方法来计算 poc。
                            // 而以下的几个句法元素是分别在各种方法中用到的数据。

                            // 在如下的视频序列中本句法元素不应该等于 2:
                            // - 一个非参考帧的接入单元后面紧跟着一个非参考图像(指参考帧或参考场)的接入单元
                            // - 两个分别包含互补非参考场对的接入单元后面紧跟着一个非参考图像的接入单元
                            // - 一个非参考场的接入单元后面紧跟着另外一个非参考场,并且这两个场不能构成一个互补场对

    if ( pic_order_cnt_type == 0 ) {
        log2_max_pic_order_cnt_lsb_minus4,  //ue(v) 指明了变量 MaxPicOrderCntLsb 的值:
                                                // MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )
                                                // 该变量在 pic_order_cnt_type = 0 时使用
    } else if ( pic_order_cnt_type == 1 ) {
        delta_pic_order_always_zero_flag,   //u(1) 等于 1 时,句法元素 delta_pic_order_cnt[0]和 delta_pic_order_cnt[1]不在片头出现,并且它们的值默认为 0; 
                                                // 本句法元素等于 0 时,上述的两个句法元素将在片头出现
                                                
        offset_for_non_ref_pic, //se(v) 被用来计算非参考帧或场的 picture order count (在 8.2.1),本句法元素的值应该在[-2^31 , 2^31 – 1]
        
        offset_for_top_to_bottom_field, //se(v) 被用来计算帧的底场的 picture order count (在 8.2.1), 本句法元素的值应该在[-2^31 , 2^31 – 1]
        
        num_ref_frames_in_pic_order_cnt_cycle,  //ue(v) 被用来解码 picture order count (在 8.2.1),本句法元素的值应该在[0,255]

        for ( i=0; i<num_ref_frames_in_pic_order_cnt_cycle; ++i ) {
            offset_for_ref_frame[i],    //se(v) picture order count type=1 时用，用于解码 POC，
                                            // 本句法元素对循环 num_ref_frames_in_pic_order_cycle 中的每一个元素指定一个偏移
        }
    }
    num_ref_frames, //ue(v) 指定参考帧队列可能达到的最大长度，解码器依照这个句法元素的值开辟存储区，这个存储区用于存放已解码的参考帧，
                        // H.264 规定最多可用 16 个参考帧，本句法元素的值最大为 16。值得注意的是这个长度以帧为单位，如果在场模式下，应该相应地扩展一倍
            
    gaps_in_frame_num_value_allowed_flag,   //u(1) 这个句法元素等于 1 时，表示允许句法元素 frame_num 可以不连续。
                                                // 当传输信道堵塞严重时，编码器来不及将编码后的图像全部发出，这时允许丢弃若干帧图像。
                                                // 在正常情况下每一帧图像都有依次连续的 frame_num 值，
                                                // 解码器检查到如果 frame_num 不连续，便能确定有图像被编码器丢弃。
                                                // 这时，解码器必须启动错误掩藏的机制来近似地恢复这些图像，因为这些图像有可能被后续图像用作参考帧
                                                //
                                                // 当这个句法元素等于 0 时，表不允许 frame_num 不连续，即编码器在任何情况下都不能丢弃图像。
                                                // 这时，H.264 允许解码器可以不去检查 frame_num 的连续性以减少计算量。
                                                // 这种情况下如果依然发生 frame_num 不连续，表示在传输中发生丢包，
                                                // 解码器会通过其他机制检测到丢包的发生，然后启动错误掩藏的恢复图像
            
    pic_width_in_mbs_minus1,    //ue(v) 本句法元素加 1 后指明图像宽度，以宏块为单位：
                                    // PicWidthInMbs = pic_width_in_mbs_minus1 + 1
                                    // 通过这个句法元素解码器可以计算得到 亮度分量 以像素为单位的图像宽度：
                                    // PicWidthInSamplesL = PicWidthInMbs * 16
                                    // 从而也可以得到 色度分量 以像素为单位的图像宽度：
                                    // PicWidthInSamplesC = PicWidthInMbs * 8
                                    // 以上变量 PicWidthInSamplesL、PicWidthInSamplesC 分别表示图像的亮度、色度分量以像素为单位的宽
                                    //
                                    // H.264 将图像的大小在序列参数集中定义，意味着可以在通信过程中随着序列参数集动态地改变图像的大小，
                                    // 在后文中可以看到，甚至可以将传送的图像剪裁后输出
                                                                
    pic_height_in_map_units_minus1, //ue(v) 本句法元素加 1 后指明图像高度：
                                        // PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1
                                        // PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits
                                        // 图像的高度的计算要比宽度的计算复杂，因为一个图像可以是帧也可以是场，
                                        // 从这个句法元素可以在帧模式和场模式下分别计算出出亮度、色度的高。
                                        // 值得注意的是，这里以 map_unit 为单位，map_unit的含义由后文叙述
                                                                
    frame_mbs_only_flag,    //u(1) 本句法元素等于 0 时表示本序列中所有图像的编码模式都是帧，没有其他编码模式存在；
                                // 本句法元素等于 1 时 ，表示本序列中图像的编码模式可能是帧，也可能是场或帧场自适应，某个图像具体是哪一种要由其他句法元素决定。
                                // 结合 map_unit 的含义，这里给出上一个句法元素pic_height_in_map_units_minus1的进一步解析步骤：
                                // 当 frame_mbs_only_flag == 1，pic_height_in_map_units_minus1 指的是一个 picture 中 帧 的高度；
                                // 当 frame_mbs_only_flag == 0，pic_height_in_map_units_minus1 指的是一个 picture 中 场 的高度，
                                // 所以可以得到如下以宏块为单位的图像高度：
                                // FrameHeightInMbs = ( 2 – frame_mbs_only_flag ) * PicHeightInMapUnits
                                // PictureHeightInMbs= ( 2 – frame_mbs_only_flag ) * PicHeightInMapUnits
    
    if( !frame_mbs_only_flag ) {
        mb_adaptive_frame_field_flag,   //u(1)  指明本序列是否属于帧场自适应模式。
                                            // mb_adaptive_frame_field_flag == 1 
                                            // 时表明在本序列中的图像如果不是 场模式 就是 帧场自适应模式，
                                            // mb_adaptive_frame_field_flag == 0 
                                            // 时表示本序列中的图像如果不是 场模式 就是 帧模式
                                            // 列举了一个序列中可能出现的编码模式：
                                            // a. 全部是帧，对应于 frame_mbs_only_flag =1  的情况
                                            // b. 帧和场共存。frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =0
                                            // c. 帧场自适应和场共存。frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =1
                                            // 值得注意的是，帧和帧场自适应不能共存在一个序列中
    }

    direct_8x8_inference_flag,  //u(1) 用于指明 B 片的直接和 skip 模式下运动矢量的预测方法。
    
    frame_cropping_flag,    //u(1) 用于指明解码器是否要将图像裁剪后输出，
                                // 如果是的话，后面紧跟着的四个句法元素分别指出左右、上下裁剪的宽度。

    if ( frame_cropping_flag ) {
        frame_crop_left_offset,                 //ue(v)
        frame_crop_right_offset,                //ue(v)
        frame_crop_top_offset,                  //ue(v)
        frame_crop_bottom_offset,               //ue(v)
    }

    vui_parameters_present_flag,    //u(1) 指明 vui 子结构是否出现在码流中，vui 的码流结构在附录中指明，用以表征视频格式等额外信息。

    if ( vui_parameters_present_flag ) {
        vui_parameters(),                       //
    }

    rbsp_trailing_bits(),                       //
}



    //PPS
Pic_Parameter_Set_rbsp () {

    pic_parameter_set_id,  //ue(v) 用以指定本参数集的序号，该序号在各片的片头被引用
                                // slice 引用 PPS 的方式就是在 Slice header 中保存 PPS 的 id 值。该值的取值范围为[0,255]。
                                                        
    seq_parameter_set_id,  //ue(v) 指明本图像参数集所引用的序列参数集的序号, 该值的取值范围为[0,31]。
                                                        
    entropy_coding_mode_flag,  //u(1) 指明熵编码的选择，本句法元素为０时，表示熵编码使用 CAVLC，本句法元素为１时表示熵编码使用 CABAC
                                    // 对于部分语法元素，在不同的编码配置下，选择的熵编码方式不同。
                                    // 例如在一个宏块语法载中，宏块类型 m b_type 的语法元素描述符为“ue(v) | ae(v)”，
                                    // 在 baseline profile 等设置下采用指数哥伦布编码，
                                    // 在 main profile 等设置下采用 CABAC 编码。
                                    // 标识位 entropy_coding_mode_flag 的作用就是控制这种算法选择。
                                    // 当该值为 0 时，选择左边的算法，通常为指数哥伦布编码或者 CAVLC；
                                    // 当该值为 1 时，选择右边的算法，通常为 CABAC。
                                                        
    pic_order_present_flag,    //u(1) POC 的三种计算方法在片层还各需要用一些句法元素作为参数，
                                    // pic_order_present_flag == 1 时 表示在片头会有句法元素指明这些参数；
                                    // pic_order_present_flag == 0 时 表示片头不会给出这些参数，这些参数使用默认值
                                                        
    num_slice_groups_minus1,   //ue(v) 本句法元素加１后指明图像中片组的个数。
                                    // Ｈ.264 中没有专门的句法元素用于指明是否使用片组模式，
                                    // num_slice_groups_minus1 == 0,（即只有一个片组），表示不使用片组模式，后面也不会跟有用于计算片组映射的句法元素

    if ( num_slice_groups_minus1 > 0 ) {
        slice_group_map_type,  //ue(v) 当 num_slice_group_minus1 大于０，既使用片组模式时，本句法元素出现在码流中，用以指明片组分割类型。
                                    // map_units 的定义：
                                    // 当 frame_mbs_only_flag == 1 时，map_units 指的就是宏块
                                    // 当 frame_mbs_only_falg == 0 时,
                                    //  帧场自适应模式时，map_units 指的是宏块对
                                    //  场模式时，map_units 指的是宏块
                                    //  帧模式时，map_units 指的是与宏块对相类似的，上下两个连续宏块的组合体

        if ( slice_group_map_type == 0 ) {
            for( iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                run_length_minus1[ iGroup ],    //ue(v) 用以指明当片组类型等于０时，每个片组连续的 map_units 个数
            }
        } else if ( slice_group_map_type == 2 ) {
            for( iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ ) {
                top_left[ iGroup ],             //ue(v) 用以指明当片组类型等于２时，矩形区域的左上及右下位置。
                bottom_right[ iGroup ],         //ue(v)
            }
        } else if ( slice_group_map_type == 3 ||
                    slice_group_map_type == 4 ||
                    slice_group_map_type == 5 ) {
            slice_group_change_direction_flag,  //u(1) 当片组类型等于３、４、５时，本句法元素与下一个句法元素一起指明确切的片组分割方法
            slice_group_change_rate_minus1,     //ue(v) 用以指明变量 SliceGroupChangeRAte
        } else if ( slice_group_map_type == 6 ) {
            pic_size_in_map_units_minus1,       //ue(v) 在片组类型等于６时，用以指明图像以 map_units 为单位的大小

            for( i = 0; i <= pic_size_in_map_units_minus1; i++ ) {
                slice_group_id[ i ],            //u(v) 在片组类型等于６时，用以指明某个 map_units 属于哪个片组。
            }
        }
    }

    // 加 1 后, 指明目前参考帧队列的长度，即有多少个参考帧（包括短期和长期）。值得注意的是，当目前解码图像是场模式下，参考帧队列的长度应该是本句法元素再乘以2，
    // 因为场模式下各帧必须被分解以场对形式存在。（这里所说的场模式包括图像的场及帧场自适应下的处于场模式的宏块对） 本句法元素的值有可能在片头被重载。
    // 读者可能还记得在序列参数集中有句法元素 num_ref_frames 也是跟参考帧队列有关，它们的区别是 :
    // num_ref_frames 指明 参考帧队列 的 最大值, 解码器用它的值来分配内存空间;
    // num_ref_idx_l0_active_minus1 指明在这个队列中当前实际的、已存在的参考帧数目，这从它的名字“active”中也可以看出来。
    // 这个句法元素是 H.264 中最重要的句法元素之一，在第章我们可以看到，编码器要通知解码器某个运动矢量所指向的是哪个参考图像时, 
    // 并不是直接传送该图像的编号，而是传送该图像在参考帧队列中的序号。这个序号并不是在码流中传送的，而是编码器和解码器同步地、用相同的方法将参考图像放入队列，
    // 从而获得一个序号。这个队列在每解一个图像，甚至是每个片后都会动态地更新。维护参考帧队列是编解码器十分重要的工作，而本句法元素是维护参考帧队列的重要依据。
    // 参考帧队列的复杂的维护机制是 H.264 重要也是很有特色的组成部分
    num_ref_idx_l0_active_minus1,   //ue(v) 
    
    num_ref_idx_l1_active_minus1,   //ue(v) 与上一个句法元素的语义一致，只是本句法元素用于 list１，而上一句法元素用于 list 0

    weighted_pred_flag, //u(1) 用以指明是否允许 P 和 SP 片的加权预测，如果允许，在片头会出现用以计算加权预测的句法元素
    
    weighted_bipred_idc,    //u(2) 用以指明是否允许 B 片的加权预测，
                                // weighted_bipred_idc == 0 时表示使用 默认 加权预测模式,
                                // weighted_bipred_idc == 1 时表示使用 显式 加权预测模式,
                                // weighted_bipred_idc == 2 时表示使用 隐式 加权预测模式

    pic_init_qp_minus26,    //se(v) 加 26 后用以指明亮度分量的量化参数的初始值.
                                //在 H.264 中，量化参数分三个级别给出：图像参数集、片头、宏块。在图像参数集给出的是一个初始值。
                                
    pic_init_qs_minus26,    //se(v) 与上一个句法元素语义一致，只是用于 SP 和 SI
    
    chroma_qp_index_offset, //se(v) 色度分量的量化参数是根据亮度分量的量化参数计算出来的，本句法元素用以指明计算时用到的参数 取值范围为[-12,12]。

    //编码器可以通过句法元素显式地控制去块滤波的强度，本句法元素指明是在片头是否会有句法元素传递这个控制信息。
    deblocking_filter_control_present_flag,     //u(1) 标识位，用于表示 Slice header 中是否存在用于去块滤波器控制的信息。
                                                    // 当该标志位为 1 时，slice header 中包含去块滤波相应的信息；
                                                    // 当该标识位为 0 时，slice header 中没有相应的信息。解码器将独立地计算出滤波强度
                                                        
    // 在 P 和 B 片中，帧内编码的宏块的邻近宏块可能是采用的帧间编码。
    // 当本句法元素等于 1 时，表示帧内编码的宏块不能用帧间编码的宏块的像素作为自己的预测，即帧内编码的宏块只能用邻近帧内编码的宏块的像素作为自己的预测;
    // 而本句法元素等于 0 时，表示不存在这种限制
    constrained_intra_pred_flag,    //u(1) 若该标识为 1，表示 I 宏块在进行帧内预测时只能使用来自 I 和 SI 类型宏块的信息；
                                        //若该标识位 0，表示 I 宏块可以使用来自 Inter 类型宏块的信息。
                                                     
    redundant_pic_cnt_present_flag, //u(1) 标识位，用于表示 Slice header 中是否存在 redundant_pic_cnt 语法元素。
                                        // 当该标志位为 1 时，slice header 中包含 redundant_pic_cnt；
                                        // 当该标识位为 0 时，slice header 中没有相应的信息。

    rbsp_trailing_bits(),                       //
}


//片层句法(不分区)
slice_layer_without_partitioning_rbsp () {
    slice_header(),
    slice_data(),                   // all categories of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//片层 A 分区句法
slice_data_partition_A_layer_rbsp () {
    slice_header(),
    slice_id,                       //ue(v)
    slice_data(),                   // only category 2 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(), 
}

//片层 B 分区句法
slice_data_partition_B_layer_rbsp () {
    slice_id,                       //ue(v)

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,          //ue(v)
    }

    slice_data(),                   //  only category 3 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//片层 C 分区句法
slice_data_partition_C_layer_rbsp () {
    slice_id,                       //ue(v)

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,          //ue(v)
    }

    slice_data(),                   // only category 4 parts of slice_data( ) syntax
    rbsp_slice_trailing_bits(),
}

//拖尾（trailing bits）句法
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
    
    first_mb_in_slice,  // ue(v) 片中的第一个宏块的地址, 片通过这个句法元素来标定它自己的地址.
                            // 要注意的是在帧场自适应模式下，宏块都是成对出现，这时本句法元素表示的是第几个宏块对，对应的第一个宏块的真实地址应该是:
                            // 2 * first_mb_in_slice
                            
    slice_type, // ue(v) 指明片的类型，具体语义见表7.21
    
    pic_parameter_set_id,   // ue(v) 图像参数集的索引号. 范围 0 到 255

    // H.264 对 frame_num 的值作了如下规定: 当参数集中的句法元素 gaps_in_frame_num_value_allowed_flag 不为 1 时，
    // 每个图像的 frame_num 值是它前一个参考帧的 frame_num 值增加 1, 这句话包含有两层意思:
    // 1) 当 gaps_in_frame_num_value_allowed_flag == 0，即 frame_num 连续的情况下，每个图像的frame_num 由前一个参考帧图像对应的值加 1，着重点是“前一个参考帧”
    // 2) 当 gaps_in_frame_num_value_allowed_flag == 1，前文已经提到，这时若网络阻塞，编码器可以将编码后的若干图像丢弃，而不用另行通知解码器。
    //  在这种情况下，解码器必须有机制将缺失的 frame_num 及所对应的图像填补，否则后续图像若将运动矢量指向缺失的图像将会产生解码错误
    frame_num,  // u(v) 每个参考帧都有一个依次连续的 frame_num 作为它们的标识,这指明了各图像的解码顺序。
                    // 但事实上我们在表 中可以看到，frame_num 的出现没有 if 语句限定条件，这表明非参考帧的片头也会出现 frame_num。
                    // 只是当该个图像是参考帧时，它所携带的这个句法元素在解码时才有意义

    if ( !frame_mbs_only_flag ) {

        /*
                if ( frame_mbs_only_flag == 1 ) {
                    帧编码
                } else {
                    if ( mb_adaptive_frame_field_flag == 1 ) {
                        if ( field_pic_flag == 1 ) {
                            场编码
                        } else {
                            帧场自适应
                        }
                    } else {
                        if ( field_pic_flag == 1 ) {
                            场编码
                        } else {
                            帧编码
                        }
                    }
                }
            */
            
        //在序列参数集中我们已经能够计算出图像的高和宽的大小，但曾强调那里的高是指的该序列中图像的帧的高度，而一个实际的图像可能是帧也可能是场，
        //对于图像的实际高度，应进一步作如下处理 :
        // PicHeightInMbs = FrameHeightInMbs / ( 1 + field_pic_flag ) 
        // 从而我们可以得到在解码器端所用到的其他与图像大小有关的变量：
        // PicHeightInSamplesL = PicHeightInMbs * 16 
        // PicHeightInSamplesC = PicHeightInMbs * 8 
        // PicSizeInMbs = PicWidthInMbs * PicHeightInMbs

        // 前文已提到，frame_num 是参考帧的标识，但是在解码器中，并不是直接引用的 frame_num 值，而是由 frame_num 进一步计算出来的变量 PicNum,
        // 在第八章会详细讲述由 frame_num 映射到PicNum 的算法。这里介绍在该算法中用到的两个变量 :
        //
        // MaxPicNum :
        // 表征 PicNum 的最大值，PicNum 和 frame_num 一样，也是嵌在循环中，当达到这个最大值时，PicNum 将从 0 开始重新计数
        // 如果field_pic_flag= 0, MaxPicNum = MaxFrameNum.
        // 否则，MaxPicNum =2*MaxFrameNum.
        //
        // CurrPicNum :
        // 当前图像的 PicNum 值，在计算 PicNum 的过程中，当前图像的 PicNum 值是由 frame_num 直接算出
        //（在第八章中会看到，在解某个图像时，要将已经解码的各参考帧的 PicNum 重新计算一遍，新的值参考当前图像的 PicNum 得来）
        // 如果field_pic_flag= 0， CurrPicNum = frame_num.
        // 否则, CurrPicNum= 2 * frame_num + 1.

        // Frame_num 是对 帧 编号的，也就是说如果在场模式下，同属一个场对的顶场和底场两个图像的 frame_num 的值是相同的。
        // 在帧或帧场自适应模式下，就直接将图像的 frame_num 赋给 PicNum，而在场模式下，将2 * frame_num 和 2 * frame_num + 1 两个值分别赋给两个场。
        // 2 * frame_num + 1 这个值永远被赋给当前场，解码到当前场对的下一个场时, 
        // 刚才被赋为 2 * frame_num + 1 的场的 PicNum 值被重新计算为 2 * frame_num ，而将 2 * frame_num + 1 赋给新的当前场
        field_pic_flag, // u(1) 这是在片层标识图像编码模式的唯一一个句法元素。所谓的编码模式是指的帧编码、场编码、帧场自适应编码。
                            // 当这个句法元素取值为 1 时 属于场编码； 0 时为非场编码
                            // 序列参数集中的句法元素 frame_mbs_only_flag 和 mb_adaptive_frame_field_flag 再加上本句法元素共同决定图像的编码模式
        
        if ( field_pic_flag ) {
            bottom_field_flag,  // u(1) 等于 1 时表示当前图像是属于底场；等于 0 时表示当前图像是属于顶场
        }
    }

    if ( nal_unit_type == 5 ) {
        idr_pic_id, // ue(v) 不同的 IDR 图像有不同的 idr_pic_id 值。值得注意的是，IDR 图像有不等价于 I 图像，
                        // 只有在作为 IDR 图像的 I 帧才有这个句法元素，在场模式下，IDR 帧的两个场有相同的 idr_pic_id 值。
                        // idr_pic_id 的取值范围是 [0，65535]，和 frame_num 类似，当它的值超出这个范围时，它会以循环的方式重新开始计数
    }

    if ( pic_order_cnt_type == 0 ) {
        pic_order_cnt_lsb,  //u(v) 在 POC 的第一种算法中本句法元素来计算 POC 值，在 POC 的第一种算法中是显式地传递 POC 的值，
                                // 而其他两种算法是通过 frame_num 来映射 POC 的值。注意这个句法元素的读取函数是 u(v),这个 v 的来自是图像参数集: 
                                // v=log2_max_pic_order_cnt_lsb_minus4 + 4

        if ( pic_order_present_flag && !field_pic_flag ) {
            delta_pic_order_cnt_bottom, //se(v) 如果是在场模式下，场对中的两个场都各自被构造为一个图像，
                                            // 它们有各自的 POC 算法来分别计算两个场的 POC 值，也就是一个场对拥有一对 POC 值；
                                            // 而在是帧模式或是帧场自适应模式下，一个图像只能根据片头的句法元素计算出一个 POC 值。
                                            // 根据 H.264 的规定，在序列中有可能出现场的情况，即 frame_mbs_only_flag 不为 1 时，
                                            // 每个帧或帧场自适应的图像在解码完后必须分解为两个场，以供后续图像中的场作为参考图像。
                                            // 所以当 frame_mb_only_flag 不为 1时，帧或帧场自适应中包含的两个场也必须有各自的 POC 值。
                                            // 在第八章中我们会看到，通过本句法元素，可以在已经解开的帧或帧场自适应图像的 POC 基础上新映射一个 POC 值，
                                            // 并把它赋给底场。当然，象句法表指出的那样，这个句法元素只用在 POC 的第一个算法中
        }
    }

    if ( pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag ) {

        delta_pic_order_cnt[ 0 ],   //se(v) 用于帧编码方式下的 底场 和 场编码方式的 场

        if ( pic_order_present_flag && !field_pic_flag ) {
            delta_pic_order_cnt[ 1 ],   //se(v) 用于帧编码方式下的 顶场
        }
    }

    if ( redundant_pic_cnt_present_flag ) {
        redundant_pic_cnt,  //ue(v) 冗余片的 id 号
    }

    if ( slice_type == B ) {
        direct_spatial_mv_pred_flag,    //u(1) 指出在 B图像 的 直接预测 的模式下，用时间预测还是用空间预测。1：空间预测；0：时间预测
    }

    if ( slice_type == P || slice_type == SP || slice_type == B ) {
        num_ref_idx_active_override_flag,//u(1) 在图像参数集 中 我们看到已经出现句法元素 
                                           // num_ref_idx_l0_active_minus1 和 num_ref_idx_l1_active_minus1 指定当前参考帧队列中实际可用的参考帧的数目。
                                           // 在片头可以重载这对句法元素，以给某特定图像更大的灵活度。这个句法元素就是指明片头是否会重载，
                                           // 如果该句法元素等于 1，下面会出现新的 num_ref_idx_l0_active_minus1 和 num_ref_idx_l1_active_minus1 值

        if ( num_ref_idx_active_override_flag ) {
            num_ref_idx_l0_active_minus1,       //ue(v) 重载的 num_ref_idx_l0_active_minus1

            if ( slice_type == B ) {
                num_ref_idx_l1_active_minus1,   //ue(v) 重载的 num_ref_idx_l1_active_minus1
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
            cabac_init_idc, //ue(v) 给出 cabac 初始化时表格的选择，范围 0 到 2
        }

        slice_qp_delta, //se(v) 指出在用于当前片的所有宏块的量化参数的初始值 QPy
                            // SliceQPy = 26 + pic_init_qp_minus26 + slice_qp_delta 
                            // QPy 的范围是 [0, 51]
                            // 我们前文已经提到，H.264 中量化参数是分图像参数集、片头、宏块头三层给出的，前两层各自给出一个偏移值，这个句法元素就是片层的偏移

        if ( slice_type == SP || slice_type == SI ) {
            if ( slice_type == SP ) {
                sp_for_switch_flag, //u(1) 指出SP帧中的p宏块的解码方式是否是switching模式，在第八章有详细的说明
            }

            slice_qs_delta, //se(v) 与 slice_qp_delta 的与语义相似，用在 SI 和 SP 中的
                                // QSy = 26 + pic_init_qs_minus26 + slice_qs_delta
                                // QSy 值的范围是 [0, 51]
        }

        if ( deblocking_filter_control_present_flag ) {
            disable_deblocking_filter_idc,  //ue(v) H.264指定了一套算法可以在解码器端独立地计算图像中各边界的滤波强度进行滤波。
                                                // 除了解码器独立计算之外，编码器也可以传递句法元素来干涉滤波强度，
                                                // 当这个句法元素指定了在块的边界是否要用滤波，同时指明那个块的边界不用块滤波，在第八章会详细讲述

            if ( disable_deblocking_filter_idc != 1 ) {
                slice_alpha_c0_offset_div2, //se(v) 给出用于增强 α 和 t_C0 的偏移值
                                                // FilterOffsetA = slice_alpha_c0_offset_div2 << 1
                                                // 取值的范围是 [-6, +6]
                                                
                slice_beta_offset_div2, //se(v) 给出用于增强 β 和 t_C0 的偏移值
                                            // FilterOffsetB = slice_beta_offset_div2 << 1
                                            // 取值的范围是 [-6, +6]
            }
        }

        if ( num_slice_groups_minus1 > 0 && slice_group_map_type >= 3 && slice_group_map_type <= 5 ) {
            slice_group_change_cycle,   //u(v) 当片组的类型是 3, 4, 5，由句法元素可获得片组中 映射单元的数目:
                                            // MapUnitsInSliceGroup0 = Min(  slice_group_change_cycle  *  SliceGroupChangeRate,PicSizeInMapUnits )
                                            // slice_group_change_cycle 由 Ceil( Log2( PicSizeInMapUnits ÷ SliceGroupChangeRate + 1 ) )位比特表示
                                            // slice_group_change_cycle 值的范围是 0 到 Ceil( PicSizeInMapUnits÷SliceGroupChangeRate )
        }
    }
}


/*
    每一个使用 帧间预测 的图像都会引用前面已解码的图像作为 参考帧。如前文所述，编码器给每个 参考帧 都会分配一个唯一性的标识，即句法元素 frame_num。
    但是，当编码器要指定当前图像的 参考图像 时，并不是直接指定该图像的 frame_num 值，而是使用通过下面步骤最终得出的 ref_id 号：

    frame_num  --计算-->  PicNum  --排序-->  ref_id

    从 frame_num 到变换到变量 PicNum 主要是考虑到场模式的需要，当序列中允许出现场时，每个非场的图像（帧或 帧场自适应）解码后必须分解为一个 场对，
    从而需要为它们分解后的 两个场 各自指定一个标识; 进一步从 PicNum 到 ref_id 是为了能节省码流，因为 PicNum 的值通常都比较大，而在帧间预测时, 
    需要为每个运动矢量都指明相对应的参考帧的标识，如果这个标识选用 PicNum 开销就会比较大，所以 H.264 又将 PicNum 映射为一个更小的变量 ref_id。
    在编码器和解码器都同步地维护一个 参考帧队列, 每解码一个片就将该队列刷新一次, 把各图像按照特定的规则进行排序, 排序后各图像在队列中的序号就是该图像的 ref_id 值, 
    在下文中我们可以看到在宏块层表示参考图像的标识就是 ref_id, 在第八章我们会详细讲述队列的初始化、排序等维护算法。
    本节和下节介绍的是在维护队列时两个重要操作：重排序(reordering) 和 标记(marking) 所用到的句法元素

    说明：句法元素的后缀名带有 L0 指的是第一个参数列表；句法元素的后缀名带有 L1 指的是第二个参数列表（用在 B 帧预测中）
*/

/*
    重排序操作

    ---------------------------------------------------------------------------------------------------
    reordering_of_pic_nums_idc          操作
    ---------------------------------------------------------------------------------------------------
            0                           短期参考帧重排序，abs_diff_pic_num_minus1 会出现在码流中，从当
                                        前图像的 PicNum 减去 (abs_diff_pic_num_minus1 + 1) 后指明需要重
                                        排序的图像。
    ---------------------------------------------------------------------------------------------------
            1                           短期参考帧重排序，abs_diff_pic_num_minus1 会出现在码流中，从当
                                        前图像的 PicNum 加上 (abs_diff_pic_num_minus1 + 1) 后指明需要重
                                        排序的图像。
    ---------------------------------------------------------------------------------------------------
            2                           长期参考帧重排序，long_term_pic_num 会出现在码流中，指明需要重
                                        排序的图像。
    ---------------------------------------------------------------------------------------------------
            3                           结束循环，退出重排序操作。
    ---------------------------------------------------------------------------------------------------
*/

//参考帧队列重排序（reordering）句法
ref_pic_list_reordering(){

    if ( slice_type != I && slice_type != SI ) {
        ref_pic_list_reordering_flag_l0,    //u(1) 指明是否进行重排序操作,这个句法元素等于 1 时表明紧跟着会有一系列句法元素用于参考帧队列的重排序

        if ( ref_pic_list_reordering_flag_l0 ) {
            do {
                reordering_of_pic_nums_idc, //ue(v) 指明执行哪种重排序操作,具体语义见表 7.22

                if ( reordering_of_pic_nums_idc == 0 || reordering_of_pic_nums_idc == 1) {
                    
                    abs_diff_pic_num_minus1,    //ue(v) 短期参考帧重排序, PicNum - ( abs_diff_pic_num_minus1 + 1) 指明需要重排序的图像
                } else if ( reordering_of_pic_nums_idc == 2) {

                    long_term_pic_num,  //ue(v) 长期参考帧重排序, 指明需要重排序的图像
                }
            } while ( reordering_of_pic_nums_idc != 3 );
        }
    }

    if ( slice_type == B ) {
        ref_pic_list_reordering_flag_l1,    //u(1) 参考帧队列 L1

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


//加权预测句法
pred_weight_table() {

    luma_log2_weight_denom, //ue(v) 给出参考帧列表中参考图像所有亮度的加权系数，是个初始值, 值的范围是 [0, 7]
    
    chroma_log2_weight_denom,   //ue(v) 给出参考帧列表中参考图像所有色度的加权系数，是个初始值, 是个初始值, 值的范围是 [0, 7]
    
    for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ ) {
        luma_weight_l0_flag,    //u(1) 等于 1 时，指的是在参考序列 0 中的亮度的加权系数存在；
                                    // 等于 0 时，在参考序列 0 中的亮度的加权系数不存在

        if ( luma_weight_l0_flag ) {
            luma_weight_l0[ i ],    //se(v) 用参考序列 0 预测亮度值时，所用的加权系数。
                                        // 如果 luma_weight_l0_flag ==  0, luma_weight_l0[ i ] = 2^luma_log2_weight_denom
                                        
            luma_offset_l0[ i ],    //se(v) 用参考序列 0 预测亮度值时，所用的加权系数的额外的偏移。luma_offset_l0[i] 值的范围[-128, 127],
                                        // 如果 luma_weight_l0_flag is = 0, luma_offset_l0[ i ] = 0 
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
    前文介绍的重排序（reordering）操作是对参考帧队列重新排序，
    而标记（marking）操作负责将参考图像 移入 或 移出 参考帧队列
*/

/*
    -----------------------------------------------------------------------------------------------------------
    adaptive_ref_pic_marking_mode_flag                  标记（marking ）模式
    -----------------------------------------------------------------------------------------------------------
        0                                       先入先出（FIFO）：使用滑动窗的机制，先入先出，在这种模式
                                                下没有办法对长期参考帧进行操作。
    -----------------------------------------------------------------------------------------------------------
        1                                       自适应标记（marking）：后续码流中会有一系列句法元素显式指
                                                明操作的步骤。自适应是指编码器可根据情况随机灵活地作出决策。
    -----------------------------------------------------------------------------------------------------------
*/

/*
    --------------------------------------------------------------------------------------------------
    memory_management_control_operation                 标记（marking ）操作
    --------------------------------------------------------------------------------------------------
        0                                       结束循环，退出标记（marding）操作。
    --------------------------------------------------------------------------------------------------
        1                                       将一个短期参考图像标记为非参考图像，也
                                                即将一个短期参考图像移出参考帧队列。
    --------------------------------------------------------------------------------------------------
        2                                       将一个长期参考图像标记为非参考图像，也
                                                即将一个长期参考图像移出参考帧队列。
    --------------------------------------------------------------------------------------------------
        3                                       将一个短期参考图像转为长期参考图像。
    --------------------------------------------------------------------------------------------------
        4                                       指明长期参考帧的最大数目。
    --------------------------------------------------------------------------------------------------
        5                                       清空参考帧队列，将所有参考图像移出参考
                                                帧队列，并禁用长期参考机制
    --------------------------------------------------------------------------------------------------
        6                                       将当前图像存为一个长期参考帧。
    --------------------------------------------------------------------------------------------------
*/

//参考帧队列标记(marking)句法
dec_ref_pic_marking() {

    if ( nal_unit_type == 5 ) {
        no_output_of_prior_pics_flag,   //u(1) 指明是否要将前面已解码的图像全部输出
        
        long_term_reference_flag,   //u(1) 这个句法元素指明是否使用长期参考这个机制。
                                        // 如果取值为 1，表明使用长期参考，并且每个 IDR 图像被解码后自动成为长期参考帧, 
                                        // 否则（取值为 0），IDR 图像被解码后自动成为短期参考帧
    } else {
        adaptive_ref_pic_marking_mode_flag, //u(1) 指明标记（marking）操作的模式

        if ( adaptive_ref_pic_marking_mode_flag ) {

            do {
                memory_management_control_operation,    //ue(v) 在自适应标记（marking）模式中，指明本次操作的具体内容，具体语义见表 7.24

                if ( memory_management_control_operation == 1 || memory_management_control_operation == 3) {
                    difference_of_pic_nums_minus1,  //ue(v) 这个句法元素可以计算得到需要操作的图像在短期参考队列中的序号。
                                                        // 参考帧队列中必须存在这个图像。
                }

                if ( memory_management_control_operation == 2 ) {
                    long_term_pic_num,  //ue(v) 从此句法元素得到所要操作的长期参考图像的序号
                }

                if ( memory_management_control_operation == 3 || memory_management_control_operation == 6 ) {
                    long_term_frame_idx,    //ue(v) 分配一个长期参考帧的序号给一个图像
                }

                if ( memory_management_control_operation == 4 ) {
                    max_long_term_frame_idx_plus1,  //ue(v) 指明长期参考队列的最大数目, 取值范围 [0, num_ref_frames]
                }
            } while ( memory_management_control_operation != 0 );
        }
    }
}

//片层数据句法
slice_data() {

    if ( entropy_coding_mode_flag ) {

        while ( !byte_aligned() ) {
            cabac_alignment_one_bit,    //f(1) 当熵编码模式是 CABAC 时,此时要求数据字节对齐,即数据从下一个字节的第一个比特开始,
                                            // 如果还没有字节对齐将出现若干个 cabac_alignment_one_bit 作为填充
        }
    }

    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag ),
    moreDataFlag = 1,
    prevMbSkipped = 0,

    do {
        if ( slice_type != I && slice_type != SI ) {

            if ( !entropy_coding_mode_flag ) {

                mb_skip_run,    //ue(v) 当图像采用帧间预测编码时，H.264 允许在图像平坦的区域使用“跳跃”块，“跳跃”块 本身不携带任何数据，
                                    // 解码器通过周围已重建的宏块的数据来恢复“跳跃”块.我们可以看到,当熵编码为 CAVLC 或 CABAC 时,"跳跃"块的表示方法不同.
                                    // 当 entropy_coding_mode_flag == 1，即熵编码为 CABAC 时, 每个"跳 跃"块 都会有句法元素 mb_skip_flag 指明,
                                    // 而 entropy_coding_mode_flag == 0，即熵编码为 CAVLC 时，用一种行程的方法给出紧连着的“跳跃”块的数目，
                                    // 即句法元素 mb_skip_run. mb_skip_run 值的范围 [0, PicSizeInMbs – CurrMbAddr]

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
                mb_field_decoding_flag, //u(1) | ae(v) 在帧场自适应图像中，指明当前宏块所属的宏块对是帧模式还是场模式。
                                            // 0 -- 帧模式; 1 -- 场模式。如果一个宏块对的两个宏块句法结构中都没有出现这个句法元素，
                                            // 即它们都是“跳跃”块时，本句法元素由以下决定 :
                // 如果这个宏块对与相邻的、左边的宏块对属于同一个片时，这个宏块对的 mb_field_decoding_flag 的值等于左边的宏块对的 mb_field_decoding_flag 的值。
                // 否则, 这个宏块对 的 mb_field_decoding_flag 的值等于上边同属于一个片的 宏块对 的 mb_field_decoding_flag 的值
                // 如果这个 宏块对 既没有相邻的、上边同属于一个片的宏块对,也没有相邻的、左边同属于一个片的宏块对，这个宏块对的 mb_field_decoding_flag 的值等于0，即帧模式
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
                end_of_slice_flag,  //ae(v) 指明是否到了片的结尾
                moreDataFlag = !end_of_slice_flag,
            }
        }

        CurrMbAddr = NextMbAddress( CurrMbAddr ),

    } while ( moreDataFlag );
}

/*
    --------------------------------------------
    片类型                  允许出现的宏块种类
    --------------------------------------------
    I (slice)               I 宏块
    --------------------------------------------
    P (slice)               P 宏块、 I 宏块
    --------------------------------------------
    B (slice)               B 宏块、 I 宏块
    --------------------------------------------
    SI (slice)              SI 宏块、 I 宏块
    --------------------------------------------
    SP (slice)              P 宏块、 I 宏块
    --------------------------------------------
*/

/*
    在帧间预测模式下，宏块可以有七种运动矢量的划分方法。
    在帧内预测模式下，可以是帧内 16x16 预测，这时可以宏块有四种预测方法，即四种类型, 
                    也可以是 4x4 预测，这时每个 4x4 块可以有九种预测方法，整个宏块共有 144 种类型。
    mb_type 并不能描述以上所有有关宏块类型的信息。
    事实上可以体会到，mb_tye 是出现在宏块层的第一个句法元素，它描述跟整个宏块有关的基本的类型信息
*/

/*
    -------------------------------------------------------------------------------------------------
    CodedBlockPatternChroma                     定义
    -------------------------------------------------------------------------------------------------
            0                   所有残差都不被传送，解码器把所有残差系数赋为0。
    -------------------------------------------------------------------------------------------------
            1                   只有DC系数被传送，解码器把所有AC系数赋为0。
    -------------------------------------------------------------------------------------------------
            2                   所有残差系数（包括DC、AC）都被传送。解码器用接收到的残差系数重建图像
    -------------------------------------------------------------------------------------------------
*/

/*
    |<-----  macroblock --------->|
    +----------+-----------+------+
    | mac_type | pred_type | data |
    +----------+-----------+------+
*/
//宏块层句法
macroblock_layer() {

    mb_type,    //ue(v) | ae(v) 指明当前宏块的类型。H.264规定，不同的片中允许出现的宏块类型也不同

    if ( mb_type == I_PCM ) {
        while ( !byte_aligned() ) {
            pcm_alignment_zero_bit, //f(1) 0
        }

        for( i = 0; i < 256 * ChromaFormatFactor; i++) {
            pcm_byte[ i ],  //u(8)  前 256 pcm_byte[i] 的值 代表亮度像素 的值
                                // ( 256 * ( ChromaFormatFactor - 1 ) ) / 2 个 pcm_byte[i] 的 值 代 表 Cb 分 量 的 值
                                // 最 后 一 个( 256 * ( ChromaFormatFactor - 1 ) ) / 2 个 pcm_byte[i]的值代表 Cr 分量的值
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

            // 这个句法元素同时隐含了一个宏块中亮度、色度分量的 CBP，所以第一步必须先分别解算出各分量各自 CBP 的值。其中，两个色度分量的 CBP 是相同的。
            // 变量 CodedBlockPatternLuma 是亮度分量的 CBP，变量 CodedBlocPatternChroma 是色度分量的 CBP：对于非 Intra_16x16 的宏块类型：
            // CodedBlockPatternLuma = coded_block_pattern % 16
            // CodedBlockPatternChroma = coded_block_pattern / 16
            // 对于Intra_16x16宏块类型，CodedBlockPatternLuma 和CodedBlockPatternChroma 的值不是由本句法元素给出，而是通过 mb_type 得到

            // 
            // CodedBlockPatternLuma：是一个16位的变量，其中只有最低四位有定义。由于非 Intra_16x16 的宏块不单独编码DC系数，所以这个变量只指明两种编码方案：
            // 残差全部编码或全部不编码。变量的最低位比特从最低位开始，每一位对应一个子宏块，该位等于1时表明对应子宏块残差系数被传送；
            // 该位等于0时表明对应子宏块残差全部不被传送，解码器把这些残差系数赋为0。

            //
            // CodedBlockPatternChroma：当值为0、1、2时有定义
            
            coded_block_pattern,    //me(v) | ae(v) 即 CBP，指亮度和色度分量的各小块的残差的编码方案，所谓编码方案有以下几种 :
                // a)  所有残差（包括 DC、AC）都编码
                // b)  只对 DC 系数编码
                // c)  所有残差（包括 DC、AC）都不编码
        }

        if( CodedBlockPatternLuma > 0 || CodedBlockPatternChroma > 0 ||
            MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {

            mb_qp_delta,    //se(v) | ae(v) 在宏块层中的量化参数的偏移值。mb_qp_delta 值的范围是 [-26, +25]
                                // 量化参数是在图像参数集、片头、宏块分三层给出的，最终用于解码的量化参数由以下公式得到 :
                                // QPy = ( QPy,prev + mb_qp_delta + 52 ) % 52 
                                // QPy,prev 是当前宏块按照解码顺序的前一个宏块的量化参数，我们可以看到，mb_qp_delta 所指示的偏移是前后两个宏块之间的偏移。
                                // 而对于片中第一个宏块的 QPy,prev 是由 7-16 式给出
                                // QPy,prev = 26 + pic_init_qp_minus26 + slice_qp_delta
            residual(),
        }
    }
}


//宏块层预测句法
mb_pred( mb_type ) {
    if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ||
        MbPartPredMode( mb_type, 0 ) == Intra_16x16 ) {

        if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ) {
            for( luma4x4BlkIdx=0; luma4x4BlkIdx<16; luma4x4BlkIdx++ ) {

                // 帧内预测的模式也是需要预测的
                // 用来指明帧内预测时，亮度分量的预测模式的预测值是否就是真实预测模式，如果是，就不需另外再传预测模式。
                // 如果不是，就由 rem_intra4x4_pred_mode 指定真实预测模式
                prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ],  //u(1) | ae(v)

                if( !prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] ) {
                    
                    rem_intra4x4_pred_mode[ luma4x4BlkIdx ],    //u(3) | ae(v) 
                }
            }
        }

        /*
            intra_chroma_pred_mode  预测模式
            0                       DC
            1                       Horizontal
            2                       Vertical
            3                       Plane
            */

        intra_chroma_pred_mode, //ue(v) | ae(v) 在帧内预测时指定色度的预测模式
    } else if ( MbPartPredMode( mb_type, 0 ) != Direct ) {

        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++) {
            if( ( num_ref_idx_l0_active_minus1 > 0 ||
                    mb_field_decoding_flag ) && 
                    MbPartPredMode( mb_type, mbPartIdx ) != Pred_L1 ){


                // 用参考帧队列 L0 进行预测，即前向预测时，参考图像在参考帧队列中的序号。 其中 mbPartIdx 是宏块分区的序号
                // 如果 当前宏块是非场宏块 , 则  ref_idx_l0[ mbPartIdx ]  值的范围是 [0, num_ref_idx_l0_active_minus1]
                // 否则，如果当前宏块是场宏块, (宏块所在图像是场，当图像是帧场自适应时当前宏块处于场编码的宏块对),
                // ref_idx_l0[ mbPartIdx ] 值的范围是 [0, 2 * num_ref_idx_l0_active_minus1 + 1], 如前所述，此时参考帧队列的帧都将拆成场，故参考队列长度加倍。
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
                    mvd_l0[ mbPartIdx ][0][ compIdx ],  //se(v) | ae(v) 运动矢量的预测值和实际值之间的差。mbPartIdx 是宏块分区的序号。
                            // CompIdx = 0 时水平运动矢量; CompIdx = 1 垂直运动矢量
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


//子宏块预测句法
sub_mb_pred( mb_type ) {

    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ ) {
        sub_mb_type[ mbPartIdx ],   //ue(v) | ae(v) 指明子宏块的预测类型，在不同的宏块类型中这个句法元素的语义不一样
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


//残差句法
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

//CAVLC 残差句法
residual_block_cavlc( coeffLevel, maxNumCoeff ) {

    for( i = 0; i < maxNumCoeff; i++ ) {
        coeffLevel[ i ] = 0,
    }

    coeff_token,    //ce(v) 指明了非零系数的个数，拖尾系数的个数

    if( TotalCoeff( coeff_token ) > 0 ) {
        if( TotalCoeff( coeff_token ) > 10 && TrailingOnes( coeff_token ) < 3 )
            suffixLength = 1
        else 
            suffixLength = 0

        for( i = 0; i < TotalCoeff( coeff_token ); i++ ) {
            if( i < TrailingOnes( coeff_token ) ) {
                trailing_ones_sign_flag,    //u(1) 拖尾系数的符号
                        // 如果trailing_ones_sign_flag = 0, 相应的拖尾系数是+1
                        // 否则，trailing_ones_sign_flag =1，相应的拖尾系数是-1
                level[ i ] = 1 – 2 * trailing_ones_sign_flag,
            } else {
                level_prefix,   //ce(v) 非零系数值的前缀和后缀
                levelCode = ( level_prefix << suffixLength ),

                if( suffixLength > 0 | | level_prefix >= 14 ) {
                    level_suffix,   //u(v) 非零系数值的前缀和后缀
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
                    level[ i ] = ( –levelCode – 1 ) >> 1

                if( suffixLength == 0 ) 
                    suffixLength = 1

                if( Abs( level[ i ] ) > ( 3 << ( suffixLength – 1 ) ) && suffixLength < 6 )
                    suffixLength++
            }
        }
    }

    if( TotalCoeff( coeff_token ) < maxNumCoeff ) {
        total_zeros,    //ce(v) 系数中 0 的总个数
        zerosLeft = total_zeros
    } else {
        zerosLeft = 0
    }

    for( i = 0; i < TotalCoeff( coeff_token ) – 1; i++ ) {
        if( zerosLeft > 0 ) {
            run_before, //ce(v) 在非零系数之前连续零的个数
            run[ i ] = run_before,
        } else {
            run[ i ] = 0,
        }

        zerosLeft = zerosLeft – run[ i ],
    }

    run[ TotalCoeff( coeff_token ) – 1 ] = zerosLeft,

    coeffNum = -1,

    for( i = TotalCoeff( coeff_token ) – 1; i >= 0; i-- ) {
        coeffNum += run[ i ] + 1,
        coeffLevel[ coeffNum ] = level[ i ],
    }
}

// CABAC 残差句法
residual_block_cabac( coeffLevel, maxNumCoeff ) {
    coded_block_flag,   //ae(v) 指出当前块是否包含非零系数
            // 如果 coded_block_flag= 0, 这个块不包含非零系数。
            // 如果 coded_block_flag = 1，这个块包含非零系数。

    if ( coded_block_flag ) {
        numCoeff = maxNumCoeff,
        i = 0,

        do {

            significant_coeff_flag[ i ],    //ae(v) 指出在位置为 i 处的变换系数是否为零
                    // 如果 significant_coeff_flag[ i ] = 0, 在位置为 i 处的变换系数为零。
                    // 否则，significant_coeff_flag[ i ] =1, 在位置为 i 处的变换系数不为零。

            if ( significant_coeff_flag[ i ] ) {
                last_significant_coeff_flag[ i ],   //ae(v) 表示当前位置 i 处的变换系数是否为块中最后一个非零系数
                        // 如果 last_significant_coeff_flag[ i ] =1, 这个块中随后的系数都为零
                        // 否则, 这个块中随后的系数中还有其它的非零系数.

                if ( last_significant_coeff_flag[ i ] ) {
                    numCoeff = i + 1,

                    for( j = numCoeff; j < maxNumCoeff; j++ )
                        coeffLevel[ j ] = 0
                }
            }

            i++, 
        } while ( i < numCoeff-1 );

        coeff_abs_level_minus1[ numCoeff-1 ],       //ae(v) 系数的绝对值减 1。
        coeff_sign_flag[ numCoeff-1 ],              //ae(v) 系数的符号位。
                // coeff_sign_flag = 0, 正数。
                // coeff_sign_flag = 1, 负数。

        coeffLevel[ numCoeff-1 ] = ( coeff_abs_level_minus1[ numCoeff-1]+1)*(1-2* coeff_sign_flag[numCoeff-1] ),

        for( i = numCoeff-2; i >= 0; i-- ) {
            if( significant_coeff_flag[ i ] ) {
                coeff_abs_level_minus1[ i ],        //ae(v)
                coeff_sign_flag[ i ],               //ae(v)

                coeffLevel[ i ] = ( coeff_abs_level_minus1[ i ] + 1 ) *( 1 – 2 * coeff_sign_flag[ i ] ),
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
	
	h264有两种封装:
		1) 一种是annexb模式，传统模式，有startcode，SPS和PPS是在ES中
		2) 一种是mp4模式，一般mp4 mkv会有，没有startcode，SPS和PPS以及其它信息被封装在container中，每一个frame前面是这个frame的长度

	在ffmpeg中用h264_mp4toannexb_filter可以做转换:
		1) 注册filter, avcbsfc = av_bitstream_filter_init("h264_mp4toannexb");
		2) 转换bitstream
			av_bitstream_filter_filter(AVBitStreamFilterContext *bsfc,
                               AVCodecContext *avctx, const char *args,
                     uint8_t **poutbuf, int *poutbuf_size, const uint8_t *buf, int buf_size, int keyframe)
*/


// RTO（Retransmission TimeOut）重传超时时间
// RTT（Round Trip Time）往返时间
// RTT 由三部分组成：链路的传播时间（propagation delay）、末端系统的处理时间、路由器缓存中的排队和处理时间（queuing delay）。
// 其中，前两个部分的值对于一个TCP连接相对固定，路由器缓存中的排队和处理时间会随着整个网络拥塞程度的变化而变化。
// 所以RTT的变化在一定程度上反应了网络的拥塞程度。

//======================================================
//  超时重传
//======================================================
/*
    发送端如果在发出数据包（T1）时刻一个RTO之后还未收到这个数据包的ACK消息，那么发送就重传这个数据包。
*/

//======================================================
//  请求重传
//======================================================
/*
    接收端在发送ACK的时候携带自己丢失报文的信息反馈，发送端接收到ACK信息时根据丢包反馈进行报文重传。
*/

//======================================================
//  FEC 选择重传 FEC（Forward Error Correction）前向纠错
//======================================================
/*
    在发送方发送报文的时候，会根据FEC方式把几个报文进行FEC分组，通过XOR的方式得到若干个冗余包，然后一起发往接收端
    如果接收端发现丢包但能通过FEC分组算法还原，就不向发送端请求重传
    如果分组内包是不能进行FEC恢复的，就请求想发送端请求原始的数据包。
*/



//======================================================
//  经典拥塞控制算法
//======================================================
/*
    一、 慢启动（slow start）
        当连接链路刚刚建立后，不可能一开始将cwnd设置的很大，这样容易造成大量重传，经典拥塞里面会在开始将cwnd = 1,
        然后根据通信过程的丢包来逐步扩大cwnd来适应当前的网络状态，直到达到慢启动的门限阈值(ssthresh),步骤如下：
        1) 初始化设置cwnd = 1,并开始传输数据 
        2) 收到回馈的ACK,会将cwnd 加1 
        3) 发送端一个RTT后且未发现有丢包重传，就会将cwnd= cwnd * 2. 
        4) 当cwnd >= ssthresh或发生丢包重传时慢启动结束，进入 拥塞避免状态。
        
    二、 拥塞避免
        当通信连接 慢启动结束 后，有可能还未到网络传输速度的上线，这个时候需要进一步通过一个缓慢的调节过程来进行适配。
        一般是一个RTT后如果未发现丢包，就是将cwnd = cwnd + 1。一但发现丢包和超时重传，就进入 拥塞处理状态。
        
    三、 拥塞处理
        拥塞处理在TCP里面实现很暴力，如果发生丢包重传，直接将cwnd = cwnd / 2，然后进入 快速恢复状态。
        
    四、 快速恢复
        快速恢复是通过确认丢包只发生在窗口一个位置的包上来确定是否进行快速恢复，如图6中描述，
        如果只是104发生了丢失，而105,106是收到了的，那么ACK总是会将ack的base = 103,
        如果连续3次收到base为103的ACK,就进行快速恢复，也就是立即重传104，而后如果收到新的ACK且base > 103,将cwnd = cwnd + 1,并进入 拥塞避免状态。
*/

#endif
