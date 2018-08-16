#ifndef __TCP__UDP__H__
#define __TCP__UDP__H__

/*
    TCP Handshake

    一、 SYN=1,ACK=0, seq=0
    二、 SYN=1,ACK=1, seq=0, ack=1
    三、 SYN=0,ACK=1, seq=1, ack=1


    Fin Handshake

    一、FIN=1,ACK=1, seq=50, ack=100
    二、FIN=0,ACK=1, seq=100, ack=51
    三、FIN=1,ACK=1, seq=100, ack=51 ( 二、三组合为一次ack, FIN=1,ACK=1, seq=100, ack=51 )
    四、FIN=0,ACK=1, seq=51, ack=101
*/

/*
    TCP 数据段格式 (20字节)

    +------------------------------------------------+------------------------------------------------+
    |                源端口号 (16bit)                |              目的端口号 (16bit)     	      |    //4byte
    +------------------------------------------------+------------------------------------------------+
    |                                            顺序号 (32bit)                                       |    //8byte
    +-------------------------------------------------------------------------------------------------+
    |                                            确认号 (32bit)                 	                  |    //12byte
    +------------------|-----------------|-|-|-|-|-|-|------------------------------------------------+
    |                  |                 |U|A|P|R|S|F|                                                |
    |TCP_H_Length(4bit)| reserved (6bit) |R|C|S|S|Y|I|              窗口大小 (16bit)	            |    //16byte
    |                  |                 |G|K|H|T|N|N|                                                |
    +------------------|-----------------|-|-|-|-|-|-|------------------------------------------------+
    |               校验和 (16bit)                   |              紧急指针 (16bit)	            |    //20byte
    +------------------------------------------------+------------------------------------------------+
    |                                           可选项 (8bit 的倍数)                                  |
    +-------------------------------------------------------------------------------------------------+
    |                                                                                                 |
    |                                              数据                                               |
    |                                                                                                 |
    +-------------------------------------------------------------------------------------------------+


    1. 源端口和目的端口
        1) 各占2字节, 端口号加上IP地址, 共同构成socket. 
        2) 互相通信的进程使用一对socket. 包括协议、源IP、源端口、目的IP、目的端口，这五个元素唯一确定一个TCP连接

    2. 顺序号
        1) 占4字节, 是TCP段所发送的数据部分第一个字节的序号.
        2) 在TCP传送的数据流中, 每一个字节都有一个序号. 
        3) 建立连接时, 发送方将初始序号（Initial Sequence Number, ISN）填写到第一个发送的TCP段序号中.

    3. 确认号
        1) 占4字节, 是期望收到对方下次发送的数据的第一个字节的序号, 也就是期望收到的下一个TCP段的首部中的序号. 
        2) 等于已经成功收到的TCP段的最后一个字节序号加1. 
        3) 确认号在 ACK标志 为1时有意义, 除了主动发起连接的第一个 TCP段 不设置 ACK标志 外, 其后发送的 TCP段 都会设置 ACK标志.

    4. TCP_H_Length: 数据偏移
        1) 占4比特, 表示数据开始的地方离 TCP段 的 起始处 有多远. 实际上就是 TCP段 首部的长度. 
        2) 由于首部长度不固定, 因此数据偏移字段是必要的. 
        3) 数据偏移以 (32bit) 为长度单位, 因此TCP首部的最大长度是60(15*4)个字节。

    5. 保留

    6. 控制位
        一共6个，占6比特，设置为1时有效。按顺序依次为：URG、ACK、PSH、RST、SYN、FIN。
        1) URG 紧急位, 为1时, 首部中的紧急指针有效
        2) ACK 确认位, 为1时, 首部中的确认号有效
        3) PSH   推位, 为1时, 要求把数据尽快交给应用程序
        4) RST 复位位, 为1时, 复位连接, 一般在出错或关闭连接时使用
        5) SYN 同步位, 在建立连接时使用, 
                当SYN=1而ACK=0时, 表明这是一个连接请求报文段. 
                对方若同意建立连接, 在发回的报文段中使SYN=1和ACK=1
        6) FIN 结束位, 为1时, 表示发送方完成了数据发送

    7. 窗口大小
        占2字节, 表示报文段发送方期望接收的字节数. 可接收的序号范围是从接收方的确认号开始到确认号加上窗口大小之间的数据.
        对方无需确认,可以发送的最大字节数

    8. 校验和
        校验和 包含了 伪首部、TCP首部和数据, 校验和 是TCP强制要求的, 由发送方计算, 接收方验证.
        伪首部,又称为伪包头(Pseudo Header): 是指在 TCP 的分段或 UDP 的数据报格式中, 
            在数据报首部前面增加
                源IP地址(32bit)+目的IP地址(32bit)+0值(8bit)+IP分组的协议字段(8bit)+TCP或UDP数据报的总长度(16bit) == 12字节, 
            所构成的扩展首部结构. 
            此伪首部是一个临时的结构, 它既不向上也不向下传递, 仅仅只是为了保证可以校验套接字的正确性

    9. 紧急指针
        1) URG标志 为1时,紧急指针有效, 表示数据需要优先处理. 
        2) 紧急指针指出在 TCP段 中的紧急数据的最后一个字节的序号, 使接收方可以知道紧急数据共有多长.

    10. 选项
        最常用的选项是最大段大小 (Maximum Segment Size，MSS), 向对方通知本机可以接收的最大TCP段长度. MSS选项只在建立连接的请求中发送
*/


/*
    UDP 数据报格式 (8字节)

    +------------------+------------------+
    | 源端口号 (16bit) | 目的端口号(16bit)|     //4byte
    +------------------+------------------+
    |   报文长度       |       校验和     |     //8byte
    +-------------------------------------+
    |                                     |
    |                 数据                |
    |                                     |
    +-------------------------------------+

    1. 源端口号: (16bit) 发送方端口号
    2. 目的端口: (16bit) 接收方端口号
    3. 报文长度: (16bit) UDP 用户数据报的总长度, 以字节为单位.
    4. 校验和:   (16bit) 检测 UDP 用户数据报在传输中是否有错, 有错就丢弃.
    5. 数据
        UDP 的数据部分如果不为偶数需要用 0 填补, 就是说, 如果数据长度为奇数, 数据长度加 "1"
*/


/*
    IP 数据包格式 (20字节)

    +-------------+--------------+---------------------------+------------------------------------------------------+
    |Version(4bit)|H_Length(4bit)| Type_of_Server(TOS)(8bit) |                  Total_Length   (16bit)              |    //4byte
    +-------------+--------------+---------------------------+----------+-------------------------------------------+
    |                     重组标识(16bit)                    |标志(3bit)|                片偏移(13bit)              |    //8byte
    +--------------------------------------------------------+----------+-------------------------------------------+
    |    生存时间(TTL)(8bit)     |     上层协议标识(8bit)    |                  头部校验和  (16bit)                 |    //12byte
    +--------------------------------------------------------+------------------------------------------------------+
    |                                           源 IP 地址 (32bit)                                                  |    //16byte
    +---------------------------------------------------------------------------------------------------------------+
    |                                         目的 IP 地址 (32bit)                                                  |    //20byte
    +---------------------------------------------------------------------------------------------------------------+
    |                                                     选项                                                      |
    +---------------------------------------------------------------------------------------------------------------+
    |                                                                                                               |
    |                                                     数据                                                      |
    |                                                                                                               |
    +---------------------------------------------------------------------------------------------------------------+

    1. Version: (4bit) 版本号
    2. H_Length: (4bit) 头长度, 它表示数据包头部包括多少个32位长整型，也就是多少个4字节的数据。无选项则为5
    3. Type_of_Server: (8bit) 服务类型

         0  1  2  3  4  5  6  7
        +--+--+--+--+--+--+--+--+
        | 优先权 | D| T| R| 保留|
        +--+--+--+--+--+--+--+--+

        1) 优先权, 取值(0--7), 数值越大优先权越高, 网络中路由器可以使用优先权进行拥塞控制, 如当网络发生拥塞时可以根据数据报的优先权来决定数据报的取舍.
        2) D (Delay), 短延时位, 取值(0/1)
            D == '1', 数据报请求以短延迟信道传输
            D == '0', 正常延时
        3) T (Throughput), 高吞吐量位,  取值(0/1)
            T == '1', 数据报请求以高吞吐量信道传输
            T == '0', 普通
        4) R (Reliability), 高可靠位, 取值(0/1)
            R == '1', 数据报请求以高可靠性信道传输
            R == '0', 普通

    4. Total_Length: (16bit) IP数据包总长度, 整个IP数据报的长度 (报头区+数据区), 以字节为单位
    
    5. 重组标识: 发送主机赋予的标识，以便接收方进行分片重组。
        1) 同一个分片的 标志值(ID值) 相同，不同的分片的 标识值(ID值) 不同。
        2) 每发送一个数据包他的值也逐渐递增
        3) 即使ID相同，如果目标地址、源地址或协议不同的话，也会被认为不同的分片。
        
    6. 标志3位:
               0    1   2
            +----+----+----+
            |保留|不分|更多|
            +----+----+----+

            0) 保留段位: 0 --> 未使用. 现在必须为0
            1) 不分段位: 0 --> 允许数据报分段, 1 --> 数据报不能分段
            2) 更多段位: 0 --> 数据包后面没有包，该包为最后的包,  1 --> 数据包后面有更多的包

    7. 段偏移量: 与更多段位组合, 帮助接收方组合分段的报文, 以字节为单位.
        1) 由13比特组成, 用来标识被分片的每一个分段相对于原始数据的位置. 第一个分片对应的值为0.
        2) 片偏移以8个字节为偏移单位. 也就是说, 每个分片的长度一定是8字节(64位)的整数倍.
    
    8. TTL (time to live): (8bit) 它指定了数据报可以在网络中传输的最长时间. 实际应用中把生存时间字段设置成了数据报可以经过的最大路由器数
            1) TTL的初始值由源主机设置(通常为32、64、128或256), 
            2) 一旦经过一个处理它的路由器,它的值就减1.
            3) 当该字段为0时,数据报就丢弃,并发送ICMP报文通知源主机.
                因此可以防止进入一个循环回路时,数据报无休止地传输下去。

    9. 上层协议标识: 表明使用该包裹的上层协议，如TCP=6，ICMP=1，UDP=17等。
            -----------+---------+-----------------
            十进制编码 |   协议  |     说明
            -----------+---------+-----------------
                0      |    无   |  保留
            -----------+---------+-----------------
                1      |  ICMP   |  网际控制报文协议
            -----------+---------+-----------------
                2      |  IGMP   |  网际组管理协议
            -----------+---------+-----------------
                3      |  GGP    |  网关-网关协议
            -----------+---------+-----------------
                4      |  无     |  未分配
            -----------+---------+-----------------
                5      |  ST     |  流
            -----------+---------+-----------------
                6      |  TCP    |  传输控制协议
            -----------+---------+-----------------
                8      |  EGB    |  外部网关协议
            -----------+---------+-----------------
                9      |  IGP    |  内部网关协议
            -----------+---------+-----------------
                11     |  NVP    |  网络声音协议
            -----------+---------+-----------------
                17     |  UDP    |  用户数据报协议
            -----------+---------+-----------------

    10. 头部校验和: (16bit)
        原理：发送端首先将检验和字段置0，然后对头部中每16位二进制数进行反码求和的运算，并将结果存在校验和字段中。 
        
由于接收方在计算过程中包含了发送方放在头部的校验和，因此，如果头部在传输过程中没有发生任何差错，那么接收方计算的结果应该是全1。

    11. 源IP地址: (32bit), 发送端IP地址
    12. 目的IP地址: (32bit), 目的端IP地址
*/


/*
    MTU (Maximum Transmission Unit) 最大传输单元


    一、应用层  (dns)
    +--------------------+
    |        Data        |
    +--------------------+

    二、传输层 (TCP:数据段 Data Segment, UDP:数据报 Datagram) (icmp)
        1. 添加TCP/UDP头
    +-----------------------------------+
    |         IP_Data(1480byte)         |
    +--------------+--------------------+
    |TCP/UDP Header|        Data        |
    +--------------+--------------------+

    三、网络层 (数据包) (ip, arp)
        1. 添加IP头 (20byte) (20byte + 1480byte = 1500byte) 
    +------------------------------------+
    |      IP数据包(MTU)(1500byte)       |   ==> IP数据包
    +-----------------+------------------+
    |IP_Header(20byte)| IP_Data(1480byte)|
    +-----------------+------------------+

    四、数据链路层 (数据帧)  [MTU ==> (46, 1500)]
        1. 添加帧头,帧尾 (7byte + 1byte + 6byte + 6byte + 2byte + 4byte = 26byte)  (1500byte + 26byte = 1526byte)
        2. 网络抓包获取的数据是去掉帧头(前同步码和帧开始定界符部分)和帧尾的数据(6 + 6 + 2 + 1500 = 1514byte)
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
    |前同步码 (7byte)|帧开始定界符(1byte)|目的MAC地址(6byte)|源MAC地址(6byte)|  上层协议(2byte) |IP数据包(MTU)(1500byte)|CRC(4byte)|
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+

        //上一层协议类型，如0x0800代表上一层是IP协议，0x0806为arp
    
    五、物理层
        1. 传输byte流
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
    |前同步码 (7byte)|帧开始定界符(1byte)|目的MAC地址(6byte)|源MAC地址(6byte)|  上层协议(2byte) |IP数据包(MTU)(1500byte)|CRC(4byte)|
    +----------------+-------------------+------------------+----------------+------------------+-----------------------+----------+
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