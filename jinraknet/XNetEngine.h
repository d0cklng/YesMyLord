#ifndef XNETENGINE_H
#define XNETENGINE_H

#include "prefix.h"
#include "INetEngine.h"
#include "macro.h"
#include "NetEngineDefs.h"
#include "../module/jindnsparser.h"

//#include "RakThread.h"
#include "INetEngine.h"
#include "NetEngineDefs.h"
#include "ISinkForNetEngine.h"
#include "RakNet/NativeTypes.h"
#include "RakNet/RakNetTypes.h"
#include "RakNet/RakPeerInterface.h"
#include "RakNet/MessageIdentifiers.h"
#include "RakNet/DS_Map.h"  //NetID到GUID的映射
#include "RakNet/DS_Queue.h"
#include "RakNet/DS_QueueLinkedList.h"
#include "RakNet/UDPProxyClient.h"
#include "RakNet/UDPProxyServer.h"
#include "RakNet/UDPProxyCoordinator.h"
#include "RakNet/NatPunchthroughServer.h"
#include "RakNet/NatPunchthroughClient.h"
using namespace RakNet;

//class ISinkForNetEngine;

#define ID_USER_PROXY_ESTABLISH		(ID_USER_PACKET_ENUM+2)
#define ID_USER_ALREADY_CONNECTED	(ID_USER_PACKET_ENUM+3)		//已经连上 可以当做ID_CONNECTION_REQUEST_ACCEPTED处理
#define ID_USER_PREFETCH_ADDRESS		(ID_USER_PACKET_ENUM+7)		//用于向coordinator询问目标guid的systemaddress
#define ID_USER_PACKET_SPECIFIC			(ID_USER_PACKET_ENUM+8)		//用于发送Loopback的时候,如果设置raknetmessageid为这个,则返回的时候保留packet第一字节id不变
//#define ID_USER_PACKET_LOOPBACK		(ID_USER_PACKET_ENUM+9)		//受head的使用占用,不要大于(0xff-head_max)
#define ID_USER_NETENGINE_DISCOVER  (ID_USER_PACKET_ENUM+9)     //通过advertise使用,也不会占用接收处的PacketType0
#define ID_USER_ADVERTISESYSTEM		(ID_USER_PACKET_ENUM+10)	//自封的advertise包,用来收发超长包,但是不会出现在PacketType0里,可以和上面的重复
#define MAX_OFFLINE_DATA_LENGTH 388		//受raknet定义和SendToEx插入数据量的影响 最后394+5等于399 因为接收时的判断是"<"而不是"<="所以不能是400
#define MAX_OFFLINE_SEGMENTS	10   //↑ d.01.06 修改394=>388 不让它那么接近临界
#define MAX_OFFLINE_TIMEOUT	3998	//大包的组包时限

enum ConnInfoStatus
{
    ConnI_Connecting,
    ConnI_ConnectingEx,
    ConnI_ConnectingMulti,	  //域名解析后,发现连接目标和之前的直连目标雷同.合并处理
    ConnI_ConnOnAdvance, //在高级方式下的连接(代理/打孔)
    ConnI_DomainResolve //正在进行的是域名解析
    //ConnI_Lost,  以下属于连接好的
    //ConnI_Incoming,
    //ConnI_Connected,
};

#define FLAG_DISCO_DIRECT 1  //peerFlag有此标志 表示Disco是定向发现的.否则是广播发现的
struct PeerInfo	//节点信息,[已经连接]
{
    RakNetGUID guid;
    SystemAddress addr;
    HostPort bdPort;	//PeerInfoMap不处理这个参数
    unsigned int peerFlag;
    //ConnID cid;  //连接信息id
};
struct ConnInfo //连接追踪信息
{
    ConnID directConn; //可能proxy方式directConn和主键不同
    NetID targetID;
    ConnInfoStatus status;
    void *parseHandle;
};
struct ProxyTunnel
{
    SystemAddress proxyAddress;
    _nInt64 tick; //expire start at this timetick.0 not expire
};
#pragma pack(push,1)
struct NetMsgProxyEstablish
{
    MessageID type;
    unsigned char positive;	//仿打孔完成时候的返回. (打孔成功时,主动方1被动方0
    SystemAddress targetAddr; //Proxy目标的地址 (目标连到coordinator使用的地址)
};

struct IPsInfo
{
    unsigned int adapt_interface;
    unsigned int ip;
    unsigned int mask;
    char mask_len; //掩码长度
};
struct ScanDiscoInfo	  //用于分批扫描,发送探测包
{
    unsigned short totalGroupCount; //infoGroup中有多少index
    unsigned short nextHandleIndex; //infoGroup中下一个要处理的IPsInfo
    int HandledInCurIdx; //当前index中处理了多少个 因为一个Group要处理的地址可能很多 分批次
    IPsInfo* infoGroup;
};
//struct NetMsgAddressPrefetch
//{
//	MessageID type;
//	char GetOrReturn; //可能值'g'et 或 'r'eturn
//	union{
//		RakNetGUID guid;
//		SystemAddress addr;
//	};
//};
#pragma pack(pop)






class XNetEngine: public INetEngine ,public UDPProxyClientResultHandler ,public UDPProxyServerResultHandler
#ifdef PUNCHTHROUGH_DEBUG
    ,public NatPunchthroughServerDebugInterface_Printf,public NatPunchthroughDebugInterface_Printf
#endif
{
public:
    XNetEngine(void);
    ~XNetEngine(void);
    //void Destroy();

    NetID Init(ISinkForNetEngine* iNet);
    bool SetTimeOutTime(unsigned int timeoutTime=20000);
    bool SetMaxIncomingConnections(unsigned short maximumIncomingConnections=200);
    bool SetDiscoverDetail(HostPort DiscoverLsnPort=0,DiscoType DiscoverType=Disco_Echo,bool bDiscoReport=false);
    HostPort Bind(HostPort port, const char *bindAddress,unsigned short maxmumConnections=255,unsigned short maximumIncomingConnections=127);
    bool MultiBind(char **hostAddressWithPort,int addressCount,unsigned short maxmumConnections=255,unsigned short maximumIncomingConnections=127);
    ConnID Connect(const char* target,unsigned short port);
    ConnID Connect(const char* addrStr);
    ConnID ConnectEx(NetConnExMethod method,NetID aidID,NetID tarID);
    bool Send(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast,int head);
    bool Close(NetID targetID,char orderingChannel=0);
    bool SendToRaw(const char* target,unsigned short port,const char *netpack, int len);
    bool SendTo(const char* target,unsigned short port,NetPacket *packet, const int DataLenInPack);
    bool SendTo(const char* addrStr,NetPacket *packet, const int DataLenInPack);
    bool Broadcast(NetPacket *packet, const int DataLenInPack);
    bool Broadcast(unsigned short port,NetPacket *packet, const int DataLenInPack);
    bool SendToLoopback(NetID from,NetPacket *packet, const int DataLenInPack,int head=PACKET_DELIVER_HEAD_MAX); //自己发给自己,如果from存在,则是TYPE_APPLICATION_DATA
    bool ProcPacket(_nInt64 timetick, bool isProcAll=true);  //返回true表示建议再ProcPacket一次.当isProcAll==true时总返回false
    void ManualFree(void* toFree);
    NetID GetMyNetID();
    HostPort GetBindPort();
    unsigned int GetNetLatency(NetID target);
    void Shutdown(unsigned int blocktime=0);//关闭网络,通过Bind重新开始工作
    //void ProbeDirect(unsigned int dwIP,NetID knownID=0);	//探测节点
    bool isDirectTo(NetID tar);
    bool isConnectedTo(NetID tar);
    int HasKnowledge(NetID id,const char*& addr);

    void AttachAsPunchthroughServer(bool trueToAttach=true);
    void AttachAsPunchthroughClient(bool trueToAttach=true);
    void AttachAsUDPProxyCoordinator(bool trueToAttach=true);
    void AttachAsUDPProxyServer(unsigned short maxForwardEntries,bool trueToAttach=true);
    void AttachAsUDPProxyClient(bool trueToAttach=true);
    bool JoinCoordinator(NetID coo);
protected:
    ScanDiscoInfo sDiscoInfo;
    bool _Bind(char **hostAddressWithPort,int addressCount,unsigned short maxmumConnections,unsigned short maximumIncomingConnections);
    bool ReactiveDisco(DiscoType type=Disco_Echo,const char* addr=NULL); //如果指定了addr,port则使用非局域广播探测
    void SpreadProbes();
    //[in/out]info 提供缓冲区被函数填充  [in]maxcount 指定info最多可以填充几个 [in]指定是否输出文件		   return 得到的info数量
    int _GetAllMyIPs(IPsInfo* info,int maxcount);  //outfile为NULL不输出文件	,为“”输出到%TMP%\ips.conf 否则输出到指定文件
    NetID _Init(RakPeerInterface** iface);
    bool _Broadcast(unsigned short port,NetPacket *packet, const int DataLenInPack,MessageID PackType=ID_ADVERTISE_SYSTEM);
    //ConnID _ConnectEx(NetConnExMethod method,NetID aidID,NetID tarID,ConnInfoStatus status=ConnI_ConnectingEx);
    ConnID ConnectToDomain(const char* domain,HostPort port);
    void DeInit(); //反初始化,最高层次的
    void OnAdvertiseSystem(RakNet::Packet *packet,bool bHelperPacket);//接收处理超长通告包
    bool SendPacketInternal(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast);//强力发送,packetType写什么发什么
    bool SendToEx(const char* target,unsigned short port,NetPacket *packet, const int DataLenInPack,MessageID PackType=ID_ADVERTISE_SYSTEM);
    bool _ProcPacket(bool isProcAll=true);
    static bool OnDatagramHandler(RNS2RecvStruct* recvStruct);
    void OpenDatagramFilter();
    bool _SendToLoopback(const RakNetGUID &from,NetPacket *packet, const int DataLenInPack,const SystemAddress &sa,MessageID rakNetMessageID);
    void OperatPeerInfo(const RakNetGUID &guid,const SystemAddress &address,bool remove=false);
    //void OperatConnInfo(const ConnID &keyid,const ConnID &did,NetID targetID,bool remove=false);
    ConnID _GetConnID(const SystemAddress &sa,const RakNetGUID &guid,bool pop,unsigned char multiUseMsg=0);
    ConnID _GetConnID__DomainResolve(uint32_t id,bool pop);
    void* ResolveDomain(const char* domain,HostPort port,uint32_t genid); //发起异步解析工作.返回异步工作的唯一id
    static void JinOnDnsParsed(const char* domain, JinNetAddr addr, void* user, uint64_t user64);
    void OnDnsParsed(unsigned int dwip,HostPort port, uint32_t rrid);
    //void CheckResolveResult();
    void _DeallocatePacket(Packet* packet,bool bHelperPacket);
    //NetID PopProxyPrefetch(const NetID &aidID,int &idx); //从ProxyPreFetchMap 返回targetID.因为一个aid可能服务多个target,所以idx的目的是方便遍历

    //UDPProxyClientResultHandler
    void OnForwardingSuccess(const char *proxyIPAddress, unsigned short proxyPort, SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    void OnForwardingNotification(const char *proxyIPAddress, unsigned short proxyPort, SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    void OnNoServersOnline(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    void OnRecipientNotConnected(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    void OnAllServersBusy(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    //void OnForwardingInProgress(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);
    void OnForwardingInProgress(const char *proxyIPAddress, unsigned short proxyPort, SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin);


    //UDPProxyServerResultHandler
    void OnLoginSuccess(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin);
    void OnAlreadyLoggedIn(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin);
    void OnNoPasswordSet(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin);
    void OnWrongPassword(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin);
private:
    NetID mNetID;
    SystemAddress mSystemAddress;
    RakPeerInterface* mRakPeer;
    RakPeerInterface* mRakPeerHelper;
    ISinkForNetEngine* mSinkForNetEngine;
    HostPort bdListenPort; //开启discover时接收广播的端口,约定好了根据指定端口使用两个广播接收端口之一.所以发送的时候要向两个端口各发一份
    HostPort bdListenPortAnother;//另一个接收端口,我们不用别人可能在用.不Bind的时候DiscoverLsnPort为0则这两个变量必然是0
    DiscoType mDiscoverType;
    HostPort mDiscoverPortSet;
    bool mbDiscoReport;
    bool mIsStartup;
    _nInt64 mProcTime; //本来用GetTickCount. 现在用变量从_ProcPacket函数直接获取并记下,做不精确的tick用.移除xGetTickCount依赖

    DataStructures::Map<NetID,PeerInfo*> DiscoInfoMap; //发现模块的信息
    DataStructures::Map<NetID,PeerInfo*> PeerInfoMap; //已经连接的节点的信息
    DataStructures::Map<ConnID,ConnInfo*> ConnectingInfoMap;//连接追踪管理信息. 连接完成后则消失
    DataStructures::Map<NetID,ProxyTunnel*> ProxyTunnelMap; // 代理通道保存起来，避免already in progress | http://www.jenkinssoftware.com/forum/index.php?topic=4777.0
    //DataStructures::Map<NetID,NetID> ProxyPreFetchMap; //代理连接目标前获取目标的地址.key保存目标NetID,value保存aidID
    //DataStructures::Map<NetID,NetID> ProxyPreFetchMap;.......Disco信息
    //超长包收发用
    struct _PackCheck	{	_nInt64 timetick;		uint64_t serial;	};
    DataStructures::QueueLinkedList<_PackCheck*> LongPackCacheCheckQueue; //检查超时丢弃用
    DataStructures::Map<NetID,RakNet::Packet **> LongPackCache;
    void CheckLongPack(_nInt64 tick=0,bool bHelperPacket=true);

    NatPunchthroughServer natPunchthroughServer;
    NatPunchthroughClient natPunchthroughClient;
    UDPProxyCoordinator udpProxyCoordinator;
    UDPProxyClient udpProxyClient;
    UDPProxyServer udpProxyServer;

    char stcBuff[1024]; //简易用途的短buff
};

#endif // XNETENGINE_H
