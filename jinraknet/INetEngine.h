#ifndef	__INETENGINE_H_
#define __INETENGINE_H_

#include "prefix.h"
#include "macro.h"
#include "jinformatint.h"

#if !defined(XNETENGINE_STATICLIB)
#  if defined(XNETENGINE_LIBRARY)
#    define XNETENGINE_SHARED Q_DECL_EXPORT
#  else
#    define XNETENGINE_SHARED Q_DECL_IMPORT
#  endif
#else
#  define XNETENGINE_SHARED
#endif


#if !defined(s_addr) && defined(Q_OS_WIN)
typedef struct in_addr {
        union {
                struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { unsigned short s_w1,s_w2; } S_un_w;
                unsigned int S_addr;
        } S_un;
}IN_ADDR;
#define s_addr  S_un.S_addr /* can be used for most tcp & ip code */
#define s_host  S_un.S_un_b.s_b2    // host on imp
#define s_net   S_un.S_un_b.s_b1    // network
#define s_imp   S_un.S_un_w.s_w2    // imp
#define s_impno S_un.S_un_b.s_b4    // imp #
#define s_lh    S_un.S_un_b.s_b3    // logical host
#elif !defined(Q_OS_WIN)
#include <netinet/in.h>
#endif

class ISinkForNetEngine;

#define NETWORKID_RESERVER	100		//小于这个数的NetID认为不是正常iD
#define PACKET_DELIVER_HEAD_MAX	100		//上层可以使用的head类型,这个定义要大于0小于(对RakNet的包装中最大支持0xFF-ID_USER_PACKET_ENUM[129]).
#define PACKET_DELIVER_TYPE_MAX	0xFF	//底层类型最大值

#define NETID_TARGET_ALL (NETWORKID_RESERVER-1)
#define NETID_INVALID		0
#define NETID_SELF			1	//用于区别是否来自loopback

/// 以下的消息定义是要从网络层提交给上层的.他独立于网络引擎.
/// see also 线程NetEngineReceiver
enum NetEnginePacketType
{
    TYPE_APPLICATION_DATA=0,   ///< 上层数据
    TYPE_OUT_CONNECTION_DATA,///< 收到的是广播数据
    TYPE_LOOPBACK_DATA=0x30,      ///< 这个消息是本地产生的
    TYPE_CONNECTION_ACCEPT=50,  ///< 连接请求被接受
    TYPE_CONNECTION_INCOMING,///< 新的连入连接
    TYPE_CONNECTION_FAILED,  ///< 连接请求失败
    TYPE_CONNECTION_CLOSE,   ///< 连接被关闭
    TYPE_CONNECTION_LOST,    ///< 连接断开
    TYPE_ALREADY_CONNECTED,
    TYPE_NETENGINE_REPORT,   ///< 发现模块的报告

    TYPE_COORDINATOR_JOIN_SUCCESS=200,
    TYPE_COORDINATOR_ALREADY_JOIN,
    TYPE_COORDINATOR_JOIN_FAILED
    //
    //ID_NET_PROXY_ESTABLISH,///< 代理转发通路打通
    //ID_NET_ALREADY_CONNECT,	///< 表明已经连接到了.回调中的地址是connect时的地址(实际发送不是这个地址)
    //ID_NET_UDPPROXY_SJOIN_OK=20,  ///< 代理服务加入协调者成功
    //ID_NET_UDPPROXY_SJOIN_FAIL,///< 加入失败
    //ID_NET_UPPERREPORT_UPNP=50,///< 向上层传递的其他信息 format: |NetMessageID|--dwIP--|-port-|
#ifdef _DEBUG
    ,TYPE_NOT_DEFINED=0xFF	   ///< 编写的时候没有考虑到的
#endif

};

enum NetConnExMethod
{
    CONN_METHOD_LAN_DISCO,			///< 局域发现,尝试直连
    CONN_METHOD_PASSIVE,		///< 反向,被动连接
    CONN_METHOD_PUNCHTHROUGH,   ///< 打孔方式
    CONN_METHOD_PROXY		///< 代理转发的方式
    //CONN_METHOD_UPNP			///< 连接到对方UPNP映射出来的端口
};
/// 这个枚举用来描述包被发送的时候投递的优先级.
enum SendPriority ///< see also RakNet/PacketPriority.h  {enum PacketPriority}
{
    SEND_IMMEDIATE,	///< 最高优先级,数据被立即发送.
    SEND_QUICK,		///< 高优先级
    SEND_NORMAL,	///< 普通优先级
    SEND_RELAXED,	///< 低优先级
    NUMBER_OF_SENDS///< \internal
};

enum PackStyle  ///< see also RakNet/PacketPriority.h  {enum PacketReliability}
{
    PACK_UDPLIKE,	///< 像UDP一样工作,(额外:丢弃重复包)
    PACK_STREAM0,   ///< 定义他是为了和PacketReliability一致
    PACK_DATAGRAM,	///< 数据将最终到达,顺序不定
    PACK_TCPLIKE,	///< 数据最终都将按次序到达
    PACK_REALSTREAM///< 实时流,丢弃迟到的数据.
};
typedef int64_t _nInt64;
typedef uint64_t NetID;
//typedef int FileID;
typedef uint16_t NetPort;  //网络序的网络端口
typedef uint16_t HostPort;//本机序的网络端口
typedef struct _ConnID
{
    _ConnID()
    {
        fakeserial = 0;
        this->u.id64 = 0;  //invalid
    }
    _ConnID(NetID id)//拷贝方便.其实不是按NetID存的
    {
        this->u.id64 = id;
    }
    _ConnID(unsigned short serial,unsigned int dw)
    {	//本地构造id
        this->u.d.valid = 1;
        this->u.d.fake = 1;
        this->u.d.netport = serial?serial:(fakeserial++);
        this->u.d.dwip = dw;
    }
    _ConnID(const struct sockaddr_in &in)
    {
        this->Set(in);
    }
    void Set(const struct sockaddr_in &in)
    {
        this->u.d.valid=1;
        this->u.d.fake=0;
        this->u.d.netport = in.sin_port;
        this->u.d.dwip = in.sin_addr.s_addr;
    }
    void Set(unsigned int dwIP,NetPort port)
    {
        this->u.d.valid=1;
        this->u.d.fake=0;
        this->u.d.netport = port;
        this->u.d.dwip = dwIP;
    }
    union{
        struct _desc{
            uint8_t valid;	//是否有效
            uint8_t fake;	//是否本地臆造,臆造的情况下,netport为序号,dwip为NetID的小4字节
            NetPort netport; //网络序
            uint32_t dwip;
        } d;
        NetID id64;
    }u;
    operator bool() const { return this->u.d.valid>0 && this->u.id64; }
    //operator bool() { return this->u.d.valid>0; }
    operator NetID() { return this->u.id64; }
    bool operator < (const _ConnID &right) const{return this->u.id64 < right.u.id64;}
    bool operator == (const _ConnID &right) const{return this->u.id64 == right.u.id64;}
    bool operator == (int t) const{return this->u.id64 == (NetID)t;}
    bool operator > (const _ConnID &right) const{return this->u.id64 > right.u.id64;}
    static unsigned short fakeserial;
}ConnID,*PConnID;


struct NetEngineConnection  //连接接受/失败/丢失时的数据包
{
    unsigned char PacketType0;
    ConnID id;
};

#pragma pack(push,1)

/// 抽离出来的,与RakNet无关的包定义
/// 从网络层收到的包都将是这个格式
struct NetPacket
{
    /// \note 这里是一个字节,enum是4字节的.所以要注意转换到(char)
    /// \note 在发送的时候这一位要预留给网络层
    unsigned char PacketType;
    char DataFirstByte;/// \note 数据的第一个字节,还有更多的字节可以通过复制DataFirstByte的指针累加访问
};

struct NetEngineMsg
{
    unsigned char PacketType0;
    unsigned short MainID;
    unsigned short AsstID;
    unsigned char data[1];
};

struct NetEnginePackMsg	//消息中包装了消息
{
    unsigned char PacketType0;
    unsigned char PacketType1;
    unsigned short MainID;
    unsigned short AsstID;
    unsigned char data[1];
};
#pragma pack(pop)



//Disco_Echo1表示需要回复,Disco_Quit2表示我退出了,Disco_Reply0则是普通回应,
//Disco_ExtendTool表示探测来自工具程序,不要将其记录和用于白板联络 该类型不Reply
enum DiscoType{Disco_Reply=0,Disco_Echo=1,Disco_Quit=2,Disco_ExtendTool=33,Disco_ETReply=34/*,Disco_Probe=3*/};
//class ISinkForNetEngine;

class INetEngine
{
   public:
       _STATIC_FACTORY_DECLARATIONS(INetEngine)

       /// 初始化网络,传入用于接收数据的接口,成为一个类似SOCKET的对象
       /// \param[in] receive 由使用者提供的接收函数.NetEngine在得到数据后调用它.
       /// \return 返回0表示失败,否则表示该NetEngine的64位ID号.
       virtual NetID Init(ISinkForNetEngine* iNet)=0;

       /// \param[in] maximumIncomingConnections 最大的连入连接数
       virtual bool SetMaxIncomingConnections(unsigned short maximumIncomingConnections=200)=0;

       /// \param[in] timeoutTime 连接超时时限.不建议修改,不指定则为底层默认值(不是此处的默认参数)
       virtual bool SetTimeOutTime(unsigned int timeoutTime=20000)=0;

       ///  设定"节点发现"相关的参数，必须在Bind之前，
       /// \param[in] DiscoverLsnPort 用于节点发现的监听端口,同时兼具广播接收功能.0则不启用
       /// \param[in] DiscoverType  发现类型，云白板使用Disco_Echo。扩展程序使用Disco_ExtendTool。 不同的类型使记录不至于混淆
       /// \return 返回设置是否有效
       /// \note 必须在Bind之前调用 否则返回false
       virtual bool SetDiscoverDetail(HostPort DiscoverLsnPort=0,DiscoType DiscoverType=Disco_Echo,bool bDiscoReport=false) =0;

       /// 绑定地址和端口.如果只作为客户端可以忽略这个调用,自动绑定到INADDR_ANY,0
       /// 而如果打算作为服务器,则需要显式的调用它
       /// \param[in,out] port 绑定的端口.0表示不关心让系统自动分配,返回绑定的端口
       /// \param[in] hostAddress 绑定的地址.NULL或者空字符串表示INADDR_ANY
       /// \param[in] maxmumConnections
       /// \param[in] maximumIncomingConnections
       /// \return 返回0表示绑定失败失败 否则是绑定后的端口
       /// \note 必须先Init,反复bind将会关闭上一次的bind重新绑定
       /// \note 可以为程序指定相同的DiscoverLsnPort,他们被当做是一种应用的多个客户端,应用节点发现和免端口广播发送.
       virtual unsigned short Bind(HostPort port, const char *hostAddress,unsigned short maxmumConnections=255,unsigned short maximumIncomingConnections=127)=0;
       virtual bool MultiBind(char **hostAddressWithPort,int addressCount,unsigned short maxmumConnections=255,unsigned short maximumIncomingConnections=127)=0;

       /// 再激活发现模块.(实际使用中有因为种种原因没有得到广播消息的情况,"再激活"表示主动发起一次广播以期得到必要的回应)
       /// \param[in] 1表示需要回复,2表示我退出了,0则是普通回应
       /// \param[in] 如果指定addr 发送定向探测.更新内部发现信息. 用于子网间发现
       /// \return 如果Bind未指定DiscoverLsnPort则返回false
       virtual bool ReactiveDisco(DiscoType type=Disco_Echo,const char* addr=NULL)=0;//如果指定了addr,port则使用非局域广播探测

       /// 返回对id的了解
       /// \return 返回0表示未知;1表示已知;2表示已连接
       /// \param[in] id 网络id
       /// \param[out] addr 如果返回非0,返回地址
       virtual int HasKnowledge(NetID id,const char*& addr)=0;

       /// 连接到指定的目标,一个NetEngine可以连接到很多目标,使用者可能需要维护这些目标
       /// \param[in] target 目标地址,可以是域名和ip
       /// \param[in] port 目标端口
       /// \param[in] addrStr 目标地址的串,包含端口,使用'|'隔开.
       /// \return 返回ConnID,当连接成功进行中时候,返回ConnID用于在连接成功/失败时区分到底是谁成功或者失败. *new feature*
       virtual ConnID Connect(const char* target,unsigned short port)=0;
       virtual ConnID Connect(const char* addrStr)=0;
       /// \return 返回DWORD形式的ip地址表示请求已经投递成功 这个返回值配合端口来确定是哪个目标的连接请求返回了.
       //virtual unsigned int Connect(const char* target,unsigned short port)=0;
       //virtual unsigned int Connect(const char* addrStr)=0;
       //virtual NetID GetNetIDByConnID(const ConnID &id);

       /// 连接到指定的目标,他与Connect的区别在于需要一个协助者,连接可能通过打孔,逆向连接代理转发等方式完成(取决于协助者)
       /// \param[in] tarID 目标的ID.通常是由服务器直接提供,我们本身并没有连接到它
       /// \param[in] method 连接方法.CONN_METHOD_PASSIVE,	CONN_METHOD_PUNCHTHROUGH, CONN_METHOD_PROXY, CONN_METHOD_UPNP
       /// \param[in] aidID 协助者的ID.对于CONN_METHOD_PASSIVE,aid负责通知tarID发起到我们的连接;对于CONN_METHOD_PUNCHTHROUGH,aid是一个打孔服务器,节点需要预先已经连接到它上面;对于CONN_METHOD_PROXY是coordinator服务的节点
       /// \return 返回ConnID,当连接成功进行中时候,返回ConnID用于在连接成功/失败时区分到底是谁成功或者失败
       virtual ConnID ConnectEx(NetConnExMethod method,NetID aidID,NetID tarID)=0;

       /// 发送数据给一个已经连接到的目标
       /// \param[in] packet 要发送的数据,第一字节预留给Engine填写
       /// \param[in] DataLenInPack 发送数据的长度,只是NetPacket中数据的长度,不包括第一字节.
       /// \param[in] priority 发送的优先级,通常是SEND_QUICK
       /// \param[in] packStyle 发送方式.通常使用PACK_TCPLIKE
       /// \param[in] orderingChannel 数据是可靠有序的,因此丢包时需要等待重传.而不同的channel可以将此过程隔离互不影响.
       /// \param[in] targetID 发送的目标,如果boardcast为true,则是排除不发的目标
       /// \param[in] boardcast true是为发送到所有已经连接的目标.且targetID为排除不发的目标.
       /// \param[in] head 这个参数影响网络层使用不同的标记头来发包,在OnNetMsg收到ID_NET_APPLICATION_DATA消息时可以通过head区分消息用途.取值1~NET_DELIVER_HEAD_MAX，0留给非连接数据. head不能填0 填0会自动变成MAX
       /// \return 返回true表示请求已经投递成功
       virtual bool Send(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast=false,int head=1)=0;

       /// 关闭到目标的连接,如果是连入连接,就踢掉连接.
       /// \param[in] targetID 目标的ID,使用TARGET_ALL关闭所有的连接
       /// \return 返回true表示请求已经投递成功
       virtual bool Close(NetID targetID,char orderingChannel=0)=0;

       /// \param[in] target 目标的ip或者域名
       /// \param[in] port 目标端口
       /// \param[in] addrStr 目标地址的串,包含端口,使用'|'隔开.
       /// \param[in] packet 发送的数据,
       /// \param[in] DataLenInPack 数据长度,和其他函数调用一致,剔除第一字节后的长度
       /// \return 返回true表示发送请求已投递
       /// \note 如果target是NetID则必须HasKnowledge==1才行 挑选更合适的端口发送
       virtual bool SendTo(const char* target,unsigned short port,NetPacket *packet, const int DataLenInPack)=0;
       virtual bool SendTo(const char* addrStr,NetPacket *packet, const int DataLenInPack)=0;
       //virtual bool SendTo(NetID target,NetPacket *packet, const int DataLenInPack)=0;

       /// \param[in] netpack 附加const
       /// \param[in] len 正常的长度
       /// \note 不加任何修饰的发送,不使用rak的协议.
       virtual bool SendToRaw(const char* target,unsigned short port,const char *netpack, int len)=0;

       /// 发送（构造）数据给自己
       /// \param[in] from 接收的时候认为的来源
       /// \param[in] packet 发送的数据,
       /// \param[in] DataLenInPack 数据长度,和其他函数调用一致,剔除第一字节后的长度
       /// \param[in] head 和Send中的意义相同,使用不同的底层包头 但是可以使用0,会成为无连接数据
       /// \return 返回false表示head有误.数据始终会递交,如果from已经连接,返回TYPE_APPLICATION_DATA,否则是TYPE_OUT_CONNECTION_DATA
       virtual bool SendToLoopback(NetID from,NetPacket *packet, const int DataLenInPack,int head=PACKET_DELIVER_HEAD_MAX)=0;
       //virtual bool SendToLoopback(NetID from,NetPacket *packet, const int DataLenInPack,unsigned char rakNetMessageID = ID_USER_LOOPBACK)=0;
       //virtual bool SendToLoopback(NetPacket *packet, const int DataLenInPack,NetID from,SystemAddress *pAddr=NULL,unsigned char rakNetMessageID = ID_USER_LOOPBACK)=0;

       /// 广播信息到局域网
       /// \param[in] port 要发送的端口
       /// \param[in] packet 发送的数据,
       /// \param[in] DataLenInPack 数据长度,和其他函数调用一致,剔除第一字节后的长度
       /// \return 返回true表示发送请求已投递
       /// \note 免端口的广播发送必须在Bind时指定DiscoverLsnPort并成功完成绑定.否则返回false
       virtual bool Broadcast(NetPacket *packet, const int DataLenInPack)=0;
       virtual bool Broadcast(unsigned short port,NetPacket *packet, const int DataLenInPack)=0;

       /// 开启UPNP支持(阻塞版本)
       /// \param[in] innerPort 映射到咱内部的端口
       /// \param[out] outerAddress 传入一个缓冲区用于返回出口ip.空间必须够存放ip地址!建议长度64
       /// \param[in,out] outerPort 传入一个期望开通的外部接口.返回最终开通的端口.0表示使用默认
       /// \param[in] trys 尝试的次数
       /// \return 返回false表示开通不成功或者UPNP功能不被支持
       /// \note outerAddress必须是调用者提供的合法缓冲区,长度应该足够容纳ip地址.以\0结尾.
       /// \note 这个函数需要路由器支持和反馈,是个阻塞且比较耗时的过程.
       //virtual bool UPNPSupport(const unsigned short innerPort,char* outerAddress,unsigned short &outerPort,int trys=3)=0;

       /// 开启UPNP支持(异步通过ID_NET_UPPERREPORT_UPNP返回上层)
       /// \param[in] pIDOnLAN LAN中的活动节点.如果发现eport被这类节点占用不可覆盖
       /// \param[in] innerPort 映射到咱内部的端口
       /// \param[in] outerPort 传入一个期望开通的外部接口.返回最终开通的端口.0表示使用默认
       /// \param[in] trys 尝试的次数
       //-----//virtual bool UPNPSupportAsync(void *pIDOnLAN,const unsigned short innerPort,unsigned short outerPort,int trys=3)=0;

       /// 获得upnp支持的结果
       /// \param[out] dwIP 传出upnp的出口ip,如果有的话
       /// \param[out] port 传出upnp的外部端口
       /// \return 有结果的话返回true,否则0. 有结果后 只要不再次发起upnp都会返回true
       /// \note 如果调用这个函数的时候,upnp结果处于就绪未汇报状态,则不会通过消息重复汇报
       //virtual unsigned short GetUPNPResult(unsigned int &dwIP)=0;
       //-----//virtual bool GetUPNPResult(unsigned int &dwIP,unsigned short &port)=0;

       /// 获取到目标的延时
       /// \param[in] target 目标的NetID
       /// \return 返回延时ms毫秒
       virtual unsigned int GetNetLatency(NetID target)=0;

       /// 加载成为打孔服务器.
       /// \param[in] trueToAttach true的时候加载,反之卸载
       /// \note PunchthroughServer和PunchthroughClient不能共存
       virtual void AttachAsPunchthroughServer(bool trueToAttach=true)=0;

       /// 加载成为打孔客户端.继而ConnectEx当中CONN_METHOD_PUNCHTHROUGH的OpenNAT才能成功
       /// \param[in] trueToAttach true的时候加载,反之卸载
       /// \note PunchthroughServer和PunchthroughClient不能共存
       virtual void AttachAsPunchthroughClient(bool trueToAttach=true)=0;

       /// 加载成为udp代理协调者.
       /// \param[in] trueToAttach true的时候加载,反之卸载
       virtual void AttachAsUDPProxyCoordinator(bool trueToAttach=true)=0;

       /// 加载成为udp代理服务.然后调用JoinCoordinator等着服务别人
       /// \param[in] maxForwardEntries 最多转发通道数
       /// \param[in] trueToAttach true的时候加载,反之卸载
       /// \return proxyserver应该是个独立部分,使用独立地址和端口.否则数据将不知是forward还是给服务本身
       virtual void AttachAsUDPProxyServer(unsigned short maxForwardEntries,bool trueToAttach=true)=0;

       /// 加载成为udp代理客户端.
       /// \param[in] trueToAttach true的时候加载,反之卸载
       virtual void AttachAsUDPProxyClient(bool trueToAttach=true)=0;

       /// 加入协调者等待调度(只有UDPProxyServer角色需要)
       /// \param[in] coo 必须是一个已经连接的目标
       /// \return true表示请求已提交.在消息接收当中获知成功与否
       virtual bool JoinCoordinator(NetID coo)=0;

       /// 检收数据,通过ISinkForXXX返回数据
       /// \param[in] timetick 传入时间tick值，这样Engine中不用自行获取tick
       /// \param[in] isRecvAll 该参数设置为true则函数在该节点网络数据全部处理后才返回,且函数总是返回false .否则提交一包数据后即返回
       /// \return 返回true表示有必要立即继续调用该函数来取数据.
       virtual bool ProcPacket(_nInt64 timetick, bool isProcAll=true)=0;

       /// 获得相关id的地址.如果没有对应关系则返回false
       /// \param[in] id 要查找的id
       /// \param[out] addr 返回该id的地址,是一个sockadd_in
       /// \return 找到则返回true且填充addr
       //virtual bool GetAddrOf(NetID id,pSOCKADDRIN addr)=0;

       /// 打开收包过滤,主要在复用raknet的时候,把非raknet包挑出来,每个包来都会触发回调OnDatagram.
       /// 注意:这个函数只允许来一次,多次的话会相互覆盖使早设的不起作用.
       virtual void OpenDatagramFilter() = 0;

       /// 直连探测.用于有上下级子网关系的局域网
       /// \param[in] addr 探测的目标地址
       /// \param[in] knownID 待探测的目标是否已经知道NetID,如果知道,提供该参数
       /// \note 函数得到回应的话将导致探测信息被更新. 影响HasKnownledge
       //virtual void ProbeDirect(unsigned int dwIP,NetID knownID)=0;

       /// 判断是否和目标有连接
       /// \param[in] tar 目标
       /// \return true 表示有连接
       virtual bool isConnectedTo(NetID tar)=0;

       /// 判断到目标是否直达是否经过NAT.目标必须是个外网
       /// \param[in] tar 目标,必须是已经连接到的目标
       /// \return true表示没有经过NAT,false表示没有经过NAT或者未连接
       virtual bool isDirectTo(NetID tar)=0;

       /// 返回自己的id
       /// \return 自己的id
       virtual NetID GetMyNetID()=0;

       /// 返回自己绑的端口
       /// \return 自己的UDP端口
       virtual HostPort GetBindPort()=0;

       /// 关闭.停止网络线程
       /// \param[in] blocktime 给予时间发送退出的通告
       virtual void Shutdown(unsigned int blocktime=0)=0;

       /// 释放
       virtual void ManualFree(void* toFree)=0;

};

extern "C"
{
    XNETENGINE_SHARED INetEngine* CreateNetEngine();

    XNETENGINE_SHARED void DestroyNetEngine(INetEngine* instance);
}

#if defined(HIDDEN_DYNAMIC_LOAD_NETENGINE)	//在导出dll中并不包含这些内容
#include "ImplictLibraryLoad.hpp"
//#ifdef _DEBUG
//DYNAMIC_DLL_IMPORT(NetEngine,NetEngine,D_NetEngine.dll);
//#else
ImplictLibraryLoad(NetEngine,NetEngine,XNetEngine);
//#endif
#endif

#endif//__INETENGINE_H_
