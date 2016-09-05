
#include "XNetEngine.h"
#include "oMisc.hpp"
#include "RakNet/RakThread.h"
#include "../base/jinlogger.h"
#include "../base/jinassert.h"
#include "../module/jindnsparser.h"

//using namespace oMisc;
#define MaxProbesPerUpdate 300  //每次update（目前是1秒）最多发送多少探测
#define MinSubNetMaskLen 23  //最小的子网掩码长度. 如果更小的话,扫描点会太多

//char rbit1[256] = {	//小于0且小于-32使得计算得到的掩码长度会小于0	//char rbit1[256] = {		x表示非法掩码段
char rbit1[256] = {\
    0,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,			//0,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    1,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,			//1,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    2,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,			//2,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    -33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,		//x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    3,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,-33,			//3,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,
    4,-33,-33,-33,-33,-33,-33,-33,5,-33,-33,-33,6,-33,7,8};		   //4,x,x,x,x,x,x,x,5,x,x,x,6,x,7,8}
unsigned char bit1cL[9] =  //左起x个1得到的数字
{0,128,192,224,240,248,252,254,255};
int bit1cR[17] =  //此错误，不是从右起，是从左起
{0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535};
char __GetMaskBits(unsigned int dwMask)		 //dwMask 网络字节序·大端
{
    if(rbit1[dwMask&0xff]!=8) return rbit1[dwMask&0xff];
    dwMask>>=8;
    if(rbit1[dwMask&0xff]!=8) return rbit1[dwMask&0xff]+8;
    dwMask>>=8;
    if(rbit1[dwMask&0xff]!=8) return rbit1[dwMask&0xff]+16;
    dwMask>>=8;
    return rbit1[dwMask&0xff]+24;
}

unsigned int __GetDwMask(char bitcount)
{
    unsigned int dwMask=0;
    if(bitcount%8)
    {
        dwMask = bit1cL[bitcount%8];
        bitcount = bitcount/8*8;
    }
    while(bitcount/8)
    {
        dwMask <<= 8;
        dwMask &= 0xFF;
        bitcount-=8;
    }
    return dwMask;
}

_STATIC_FACTORY_DEFINITIONS(INetEngine,XNetEngine)
//#if defined(_DEBUG) || defined(_JDLog)
//XSafeCloseFile logFile("Log_NetEngine.txt");
//#endif
//XSafeCloseFile errFile("Err_NetEngine.txt");
XNETENGINE_SHARED INetEngine* CreateNetEngine()
{
    return INetEngine::GetInstance();
}

XNETENGINE_SHARED void DestroyNetEngine(INetEngine* instance)
{
    INetEngine::DestroyInstance(instance);
}

unsigned short _ConnID::fakeserial = 1;

struct ResloveThreadParam
{
    char* domain;
    HostPort port;
    XNetEngine *eng;
    //int id;
};

//static int ResolveRecordID = 0;


void* XNetEngine::ResolveDomain(const char* domain,HostPort port,uint32_t genid) //发起异步解析工作,port仅用于传递,与解析域名无关.返回分配的id
{
    JinNetAddr nexp;
    uint64_t user64 = (((uint64_t)genid)<<32) + port;
    void* parseHandle = JinDNS::Get()->parse(domain,Av4,JinOnDnsParsed,this,user64,nexp);
    if(nexp.isValid())
    {
        JDLog("unexpect direct parse succ.%s",domain);
        //this->JinOnDnsParsed(domain,nexp,pParam,0);
        JAssert2(false,"unexpect direct parse succ.");
        return NULL;
    }
    else
    {
        return parseHandle ;
    }

}

void XNetEngine::JinOnDnsParsed(const char *domain, JinNetAddr addr, void *user, uint64_t user64)
{
    XNetEngine *eng = (XNetEngine*)user;
    eng->OnDnsParsed(addr.rawIP(),(HostPort)user64&0xFFFF,(uint32_t)(user64>>32));
}

void XNetEngine::OnDnsParsed(unsigned int dwip, HostPort port, uint32_t rrid)
{
    ConnID cidAtStart = _GetConnID__DomainResolve(rrid,false); //此处不pop原始cid,避免调用者得不到id匹配	  此处得到当初分给这个域名连接的构造ConID
    if(cidAtStart)
    {
        bool bConnMulti=false;
        ConnID cidAdv;
        ConnInfo *pCInfoStart=NULL,*pCInfoDirect=NULL;
        JDLog("update domain conn conninfo %s",_inet_ntoa(dwip));
        pCInfoStart = ConnectingInfoMap.Get(cidAtStart);//_GetConnID__DomainResolve得到的合法cidAtStart,在ConnectingInfoMap一定有
        if(dwip)
        {
            cidAdv.Set(dwip,htons(port));	 //测试一下如果连接的话新cid会是多少 ,需要保证和下面this->Conect返回的是一样的
            if(ConnectingInfoMap.Has(cidAdv)) //如果解析出来的地址,之前已经在连接中了,而且还没出结果呢. @d.11.12 之前没考虑这个问题
            {
                bConnMulti = true;
                pCInfoDirect = ConnectingInfoMap.Get(cidAdv);
                if(pCInfoDirect->status!=ConnI_Connecting)
                {
                    JELog("domain connecting to a exist Unconventional coneection.域名连接指向一个正在进行的非普通连接.");
                    cidAdv = 0; //置0导致下面失败离开
                    //TODO
                }
                else	   //这种情况下不用连接 直接产生新Conn status
                {
                    JDLog("Conn domain match exist connection.");
                    //pCInfoDirect->status = ConnI_ConnectingMulti;
                    //pCInfoDirect->targetID = rrid;
                    //pCInfoDirect->directConn = cidAtStart;
                    //到此不用再Conect了.并用之前的就得
                    pCInfoStart->directConn=cidAdv;
                }
            }
            else
            {
                cidAdv = this->Connect(_inet_ntoa(dwip),port);
                pCInfoStart->directConn=cidAdv; //最初的cid连接信息里的directconn直到新生connid.嗯..Set(dwip,_htons(port));
            }
        }
        if(!cidAdv)
        {
            JDLog("domain connect %s directly failed",_inet_ntoa(dwip));
            NetEngineConnection *pnec = (NetEngineConnection*)stcBuff;
            pnec->PacketType0 = TYPE_CONNECTION_FAILED; //被_SendToLoopback直接上提
            pnec->id = cidAtStart;
            this->_SendToLoopback(UNASSIGNED_RAKNET_GUID,(NetPacket*)pnec,sizeof(NetEngineConnection)-1,UNASSIGNED_SYSTEM_ADDRESS,ID_USER_PACKET_SPECIFIC);
            ConnectingInfoMap.Delete(cidAtStart);
            delete pCInfoStart;
        }
        else
        {
            if(!pCInfoDirect)pCInfoDirect = ConnectingInfoMap.Get(cidAdv);//Connect得到的合法cidAdv 一定在ConnectingInfoMap有
            pCInfoDirect->targetID = rrid;
            pCInfoDirect->directConn = cidAtStart; //新生连接信息的directConn等于到最初那个 这样好找哇
            if(!bConnMulti)pCInfoDirect->status = ConnI_DomainResolve;
            else pCInfoDirect->status = ConnI_ConnectingMulti;
        }
    }
}

XNetEngine::XNetEngine(void)
{
    mNetID = NETID_INVALID;
    mRakPeer = NULL;
    mRakPeerHelper = NULL;
    mSinkForNetEngine = NULL;

    mIsStartup = false;
    bdListenPort = 0;
    bdListenPortAnother=0;
    mDiscoverType = Disco_Echo;
    mbDiscoReport = false;
    mDiscoverPortSet = 0;
    mProcTime = 0;

    memset(&sDiscoInfo,0,sizeof(ScanDiscoInfo));
}

XNetEngine *gNetEngineOpenFilter = 0;
XNetEngine::~XNetEngine(void)
{
    if(gNetEngineOpenFilter == this){
        mRakPeer->SetIncomingDatagramEventHandler(NULL);
        gNetEngineOpenFilter = NULL;
    }

    ReactiveDisco(Disco_Quit); //表示自己离开
    if(sDiscoInfo.infoGroup)delete [] sDiscoInfo.infoGroup;
    DeInit();
    JinDNS::Free();
}



NetID XNetEngine::Init(ISinkForNetEngine* iNet)
{
    RakAssert(iNet);
    if(mRakPeer)DeInit();
    mSinkForNetEngine = iNet;
    JinDNS::Create();
    mNetID = _Init(&mRakPeer);
    JDLog("? My NetID %llu",mNetID);
    return mNetID;
}
NetID XNetEngine::_Init(RakPeerInterface** iface)
{
    RakAssert(iface);
    *iface = RakPeerInterface::GetInstance();
    if(*iface && (*iface)->GetMyGUID().g<NETWORKID_RESERVER)
    {
        JELog("Warning : NETWORKID_RESERVER");
        RakPeerInterface::DestroyInstance(*iface);
        return _Init(iface);
    }
    if(*iface)
    {	return (*iface)->GetMyGUID().g;	}
    else return 0;

}
NetID XNetEngine::GetMyNetID()
{
    return mNetID;
}

HostPort XNetEngine::GetBindPort()
{
    return mSystemAddress.GetPort();
}

unsigned int XNetEngine::GetNetLatency(NetID target)
{
    int late=-1;
    if(PeerInfoMap.Has(target))
    {
        late = mRakPeer->GetLastPing( PeerInfoMap.Get(target)->guid );
    }
    return (unsigned int)late; //late可能返回-1,直接转成dword
}

#ifdef QT_VERSION
int XNetEngine::_GetAllMyIPs(IPsInfo* info,int maxcount)
{
    int ipcount=0,ifcount=0;
    QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
    foreach(const QNetworkInterface &interface,list)
    {
        QList<QNetworkAddressEntry> listAddr = interface.addressEntries();
        foreach(const QNetworkAddressEntry &addr,listAddr)
        {
            if(ipcount>=maxcount)break;
            if(addr.ip().protocol()==QAbstractSocket::IPv4Protocol)
            {
                info[ipcount].adapt_interface = ifcount;
                //qDebug()<<addr.ip()<<addr.netmask();
                info[ipcount].ip = 	qToBigEndian(addr.ip().toIPv4Address()); //QT保存成本地字节序(可大可小).只有用qToBigEndian才能保证移植正确
                info[ipcount].mask = qToBigEndian(addr.netmask().toIPv4Address());
                info[ipcount].mask_len = __GetMaskBits(info[ipcount].mask);
            }
            ipcount++;
        }
        //listAddr.clear();
        ifcount ++;
    }
    //list.clear();
    return ipcount;
}
#else
#ifdef Q_OS_WIN
int XNetEngine::_GetAllMyIPs(IPsInfo* info,int maxcount)
{
#pragma message("not implement _GetAllMyIPs")
    return 0;
}
#else
int XNetEngine::_GetAllMyIPs(IPsInfo* info,int maxcount)
{
#pragma message("not implement _GetAllMyIPs")
    return 0;
}
#endif
#endif
// void XNetEngine::ProbeDirect(unsigned int dwIP,NetID knownID)
// {
// 	const char *addr_rtn=NULL;
// 	if(!bdListenPort) return;
// 	if(knownID && HasKnowledge(knownID,addr_rtn))return;
// 	ReactiveDisco(1,_inet_ntoa(dwIP));
// }

bool XNetEngine::isConnectedTo(NetID tar)
{
    bool ret;
    //PeerInfoMutex.Lock();
    ret = PeerInfoMap.Has(tar);
    //PeerInfoMutex.Unlock();
    return ret;
}
int XNetEngine::HasKnowledge(NetID id,const char*& addr)
{
    PeerInfo* p=NULL;
    int ret=0;
    if(PeerInfoMap.Has(id))
    {
        ret = 2;
        p = PeerInfoMap.Get(id);
    }
    else if(DiscoInfoMap.Has(id))
    {
        ret = 1;
        p = DiscoInfoMap.Get(id);
    }
    //else	  //如果需要将无广播联络的非连接联络id加入knowledge，那么可以从raknet层获取信息
    //{		   //如果云白板后起，extendtool不会回复探测包 当tool来非连接包时，只有raknet层了解地址
    //	SystemAddress sa = mRakPeer->GetSystemAddressFromGuid(RakNetGUID(id));
    //	if(sa != UNASSIGNED_SYSTEM_ADDRESS)
    //	{
    //		addr = sa.ToString();
    //		return 3;
    //	}
    //}
    if(p)
    {
        addr = p->addr.ToString();
    }
    return ret;
}
bool XNetEngine::isDirectTo(NetID tar)
{ // localIP和对方看到的你的ip相同则说明你是直连不过NAT的
    int i ;
    SystemAddress sys=UNASSIGNED_SYSTEM_ADDRESS;
    //PeerInfoMutex.Lock();
    if(PeerInfoMap.Has(tar))
    {
        sys = mRakPeer->GetExternalID(PeerInfoMap.Get(tar)->addr);
    }
    //PeerInfoMutex.Unlock();
    if(sys == UNASSIGNED_SYSTEM_ADDRESS)
        return false;
    i = mRakPeer->GetNumberOfAddresses();
    for(int j=0;j<i;j++)
    {
        if(0==strcmp(mRakPeer->GetLocalIP(j),sys.ToString(false)))
            return true;
    }
    return false;
}
void XNetEngine::DeInit()
{
    CheckLongPack(mProcTime+MAX_OFFLINE_TIMEOUT+1000); //必然全部超时
    if(mIsStartup)this->Shutdown(100);
    if(mRakPeer)RakPeerInterface::DestroyInstance(mRakPeer);
    mRakPeer = NULL;
    mNetID = NETID_INVALID;
    JDLog("Deinit rakpeer.");
}
void XNetEngine::Shutdown(unsigned int blocktime)
{
    this->Close(NETID_TARGET_ALL);
    while(ProxyTunnelMap.Size())
    {
        delete ProxyTunnelMap[0];
        ProxyTunnelMap.RemoveAtIndex(0);
    }
    while(DiscoInfoMap.Size())
    {
        if(DiscoInfoMap[0]->peerFlag & FLAG_DISCO_DIRECT)
        {
            ReactiveDisco(Disco_Quit,_inet_ntoa(DiscoInfoMap[0]->addr.address.addr4.sin_addr));
        }
        delete DiscoInfoMap[0];
        DiscoInfoMap.RemoveAtIndex(0);
    }
    while(PeerInfoMap.Size())
    {
        mRakPeer->CloseConnection(PeerInfoMap[0]->guid,true,1,IMMEDIATE_PRIORITY);
        delete PeerInfoMap[0];
        PeerInfoMap.RemoveAtIndex(0);
    }
    while(ConnectingInfoMap.Size())
    {
        ConnInfo* info = ConnectingInfoMap[0];
        if(info->parseHandle) JinDNS::Get()->cancel(info->parseHandle);
        delete info;
        ConnectingInfoMap.RemoveAtIndex(0);
    }
    if(bdListenPort && mRakPeerHelper)
    {
        mRakPeerHelper->Shutdown(0);
        RakPeerInterface::DestroyInstance(mRakPeerHelper);
        mRakPeerHelper = NULL;
        bdListenPort=0;
        bdListenPortAnother=0;
        JDLog("Shutdown discover.");
    }
    if(mIsStartup && mRakPeer)
    {
        mRakPeer->Shutdown(blocktime);
        mIsStartup = false;
        JDLog("Shutdown rakpeer.");
    }
    JDLog("Shutdown compelete.");
}

bool XNetEngine::SetDiscoverDetail(HostPort DiscoverLsnPort,DiscoType DiscoverType,bool bDiscoReport)
{
    if(mIsStartup)return false;
    mDiscoverPortSet = DiscoverLsnPort;
    mDiscoverType = DiscoverType;
    mbDiscoReport = bDiscoReport;
    return true;
}

HostPort XNetEngine::Bind(HostPort port, const char *bindAddress,unsigned short maxmumConnections,unsigned short maximumIncomingConnections)
{
    char Address[40];
    char * addx = Address;
    char **adds = &addx;
    //char **adds  = &Address;
    Address[0]=0;

    if(bindAddress && bindAddress[0])
    {	strcpy(Address, bindAddress);		}
    if(port)
    {
        strcat(Address,"|");
        sprintf(Address,"%s%hu",Address,port);
    }

    if(!_Bind(adds,1,maxmumConnections,maximumIncomingConnections))
    { return 0;	}
    mSystemAddress = mRakPeer->GetMyBoundAddress(0);
    port = mSystemAddress.GetPort();
    mIsStartup = true;

    if(mDiscoverPortSet) //启用增强模式
    {
        SocketDescriptor sd;
        //int errStep=0;
        StartupResult sr;
        if(_Init(&mRakPeerHelper) < NETWORKID_RESERVER)return port;
        sd.hostAddress[0]=0;
        //sd.port = (sd.port&0xAAAA)%10000 + 2416;
        sd.port = mDiscoverPortSet;
        bdListenPort = sd.port;
        bdListenPortAnother = sd.port+560;
        sr = mRakPeerHelper->Startup(1,&sd,1);
        if(sr == SOCKET_PORT_ALREADY_IN_USE)
        {
            sd.port += 560;
            bdListenPortAnother = bdListenPort;
            bdListenPort = sd.port;
            sr = mRakPeerHelper->Startup(1,&sd,1);
        }
        if(sr != RAKNET_STARTED)
        {
            bdListenPortAnother = 0;
            bdListenPort = 0;
            RakPeerInterface::DestroyInstance(mRakPeerHelper);
            mRakPeerHelper = NULL;
            JELog("Can't start discover.%d",sr);
        }
        else  //发送探测包
        {
            IPsInfo *buf = (IPsInfo*)new char[10*sizeof(IPsInfo)];
            int count = _GetAllMyIPs(buf,10);
            sDiscoInfo.totalGroupCount = (unsigned short)count;
            sDiscoInfo.nextHandleIndex = 0;
            sDiscoInfo.HandledInCurIdx = 0;
            sDiscoInfo.infoGroup = 	buf;	   //记下 不立即发
            ReactiveDisco(mDiscoverType);

        }
    }
    return port;
}
bool XNetEngine::MultiBind(char **hostAddressWithPort,int addressCount,unsigned short maxmumConnections,unsigned short maximumIncomingConnections)
{
    return _Bind(hostAddressWithPort,addressCount,maxmumConnections,maximumIncomingConnections);
}
bool XNetEngine::_Bind(char **hostAddressWithPort,int addressCount,unsigned short maxmumConnections,unsigned short maximumIncomingConnections)
{
    RakAssert(mRakPeer);
    if(addressCount<=0)return false;
    char *localP=NULL;
    if(hostAddressWithPort==NULL)return false;
    char addrtmp[40];
    if(mIsStartup)
    {	this->Shutdown();	}
    SocketDescriptor *psd = new SocketDescriptor[addressCount];
    for(int i = 0;i<addressCount;i++)
    {
        if(hostAddressWithPort[i]==NULL)
        {
            psd[i].hostAddress[0]=0; //空地址等同INADDR_ANY
            psd[i].port = 0;
        }
        else
        {
            JDLog("Bind %s;",hostAddressWithPort[i]);
            strncpy(addrtmp,hostAddressWithPort[i],39);
            localP = strchr(addrtmp,'|');
            if(!localP)psd[i].port = 0;
            else
            {
                *localP++=0;
                psd[i].port = (unsigned short)atoi(localP);
            }
            strncpy(psd[i].hostAddress,addrtmp,31);
        }
    }

    if(maximumIncomingConnections)mRakPeer->SetMaximumIncomingConnections(maximumIncomingConnections);
    mRakPeer->SetIncomingPassword(CONNECTION_PASSWORD,CONNECTION_PASSWORDLENGTH);
    maxmumConnections = maxmumConnections>maximumIncomingConnections?maxmumConnections:maximumIncomingConnections;
    if(RAKNET_STARTED != mRakPeer->Startup(maxmumConnections,psd,addressCount))
    {		return false;	}
    delete [] psd;
    mIsStartup = true;
    JDLog("Bind %d with MaxIncome:%u,MaxConn:%u",addressCount,mRakPeer->GetMaximumIncomingConnections(),maxmumConnections);
    return true;
}
bool XNetEngine::ReactiveDisco(DiscoType type,const char* addr)
{
    if(!bdListenPort)return false;
    NetEnginePackMsg nepm;
    nepm.PacketType0 = ID_USER_NETENGINE_DISCOVER; //此处赋值将被Broadcast最后一个参数覆盖
    nepm.PacketType1 = (unsigned char)type; //1表示需要回复,2表示我退出了,0则是普通回应
    nepm.MainID = mSystemAddress.GetPort(); //主端口 没有用到.后续都是直接用数据包上的地址戳的
    nepm.AsstID = bdListenPort;	//discover 的端口
    if(addr)
    {
       nepm.data[0] = FLAG_DISCO_DIRECT;   //FLAG_DISCO_DIRECT 1表示定向探测,用于父子网发现和直连
       this->SendToEx(addr,bdListenPort,(NetPacket*)&nepm,sizeof(NetEnginePackMsg)-1,ID_USER_NETENGINE_DISCOVER);
       this->SendToEx(addr,bdListenPortAnother,(NetPacket*)&nepm,sizeof(NetEnginePackMsg)-1,ID_USER_NETENGINE_DISCOVER);
    }
    else
    {
        nepm.data[0] = 0; //0表示广播探测,1表示定向探测,用于父子网发现和直连
        this->_Broadcast(bdListenPort,(NetPacket*)&nepm,sizeof(NetEnginePackMsg)-1,ID_USER_NETENGINE_DISCOVER);
    }
    //this->SendTo()
    return true;
}
void XNetEngine::SpreadProbes()
{		//MaxProbesPerUpdate
    int ProbesSended = 0;
    while(sDiscoInfo.nextHandleIndex<sDiscoInfo.totalGroupCount)
    {
        IPsInfo *info = &sDiscoInfo.infoGroup[sDiscoInfo.nextHandleIndex];
        if(info->mask_len<MinSubNetMaskLen)
        {
            sDiscoInfo.HandledInCurIdx = 0;
            sDiscoInfo.nextHandleIndex++;
            continue;
        }
        int sectorNum = bit1cR[32-info->mask_len];  //这个段有多少需要探测的
        sDiscoInfo.HandledInCurIdx  = sDiscoInfo.HandledInCurIdx ? sDiscoInfo.HandledInCurIdx : 1; //0是该子网名，不能使用
        while(sDiscoInfo.HandledInCurIdx < sectorNum)  //最后那个是该子网的广播地址，不包括
        {
            unsigned int netAddrHost = ntohl(info->ip & info->mask);  //转成主机序的地址，小端
            netAddrHost |= sDiscoInfo.HandledInCurIdx;
            ReactiveDisco(mDiscoverType,_inet_ntoa(htonl(netAddrHost)));
            //JDLog("probe to %s",_inet_ntoa(htonl(netAddrHost)));
            ProbesSended++;
            sDiscoInfo.HandledInCurIdx++;
            if(ProbesSended>MaxProbesPerUpdate)break;
        }

        if(ProbesSended>MaxProbesPerUpdate)break;
        sDiscoInfo.nextHandleIndex++;
        sDiscoInfo.HandledInCurIdx = 0;
    }
// 	for(int i =0;i<count;i++)
// 	{
//
// 	}
}
void XNetEngine::CheckLongPack(_nInt64 tick,bool bHelperPacket)
{
    RakNet::Packet** ppp;
    //if(tick==0)tick = xGetTickCount();
    while(LongPackCacheCheckQueue.Size())//检查有的包是否作废 需要回收
    {  //这个queue是按时间顺序添入的,所以前面遇到时间未超的记录后面就不用继续查了
        _PackCheck *pc = LongPackCacheCheckQueue.Peek();
        if(tick-pc->timetick>MAX_OFFLINE_TIMEOUT)
        {
            if(LongPackCache.Has(pc->serial))	//没有的话表示包已经取走
            {
                ppp = LongPackCache.Get(pc->serial);
                for(int i=0;i<MAX_OFFLINE_SEGMENTS;i++)	//检查包是否到达完整
                {
                    if(ppp[i])
                    {	_DeallocatePacket(ppp[i],bHelperPacket); }//mRakPeer->DeallocatePacket(ppp[i]);	}
                }
                delete[] ppp;
                LongPackCache.Delete(pc->serial);
            }
            delete pc;
            LongPackCacheCheckQueue.Pop();
            continue;
        }
        break;
    }
    return ;
}
ConnID XNetEngine::Connect(const char* addrStr)
{
    char *p;
    RakAssert(mRakPeer);
    strcpy(stcBuff,addrStr);
    p = strrchr(stcBuff,'|');
    if(p)
    {
        unsigned short port;
        *p++=0;
        port=(unsigned short)atoi(p);
        return this->Connect(stcBuff,port);
    }
    return 0;
}

ConnID XNetEngine::Connect(const char* target,unsigned short port)
{
    //unsigned int dwAddr;
    bool bIsDomain = false;
    RakNet::ConnectionAttemptResult connRet;
    RakAssert(mRakPeer);
    if(target==NULL || port ==0)return 0;
    if(!mIsStartup) //如果没有startup,那么自动绑定
    {
        mRakPeer->SetMaximumIncomingConnections(DEFAULT_MAX_CONNECTIONS);
        if(!this->Bind(0,"",DEFAULT_MAX_CONNECTIONS,false))
            return 0;
    }
    ConnID cidKey;
    SystemAddress sa;
    ConnInfo *pci=NULL;

    for(int i=0;target[i];i++)
    {
        if(target[i]>57||target[i]<46)	//if发现是域名
        {
            bIsDomain = true;
            break;
        }
    }

    if(bIsDomain)
    {
        JinNetAddr addr = JinDNS::Get()->ifcache(target);
        if(addr.isValid())
        {
            target = _inet_ntoa(addr.rawIP());
        }
        else
        {
            return ConnectToDomain(target,port);
        }
    }

    sa.FromStringExplicitPort(target,port);
    cidKey.Set(sa.address.addr4);
    if(ConnectingInfoMap.Has(cidKey))
    {	pci = ConnectingInfoMap.Get(cidKey);	}
    else pci = new ConnInfo;
    pci->directConn = cidKey;
    pci->targetID = NETID_INVALID; //未知id
    pci->status = ConnI_Connecting;
    pci->parseHandle = NULL;
    ConnectingInfoMap.Set(cidKey,pci);			JDLog("ConnectingInfoMap.Set %llu",cidKey.u.id64);
    //dwAddr = sa.address.addr4.sin_addr.s_addr;
    connRet = mRakPeer->Connect(target,port,CONNECTION_PASSWORD,CONNECTION_PASSWORDLENGTH);
    if(CONNECTION_ATTEMPT_STARTED==connRet || CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS==connRet)
    {
        //return sa.address.addr4;
        //return dwAddr;
        return cidKey;
    }
    if(connRet == ALREADY_CONNECTED_TO_ENDPOINT)
    {
        MessageID type = ID_USER_ALREADY_CONNECTED;
        pci->targetID = mRakPeer->GetGuidFromSystemAddress(sa).g;
        _SendToLoopback(RakNetGUID(pci->targetID),(NetPacket*)&type,0,sa,ID_USER_ALREADY_CONNECTED);
        JDLog("ALREADY_CONNECTED_TO_ENDPOINT %s:%u ",target,port);
        //return sa.address.addr4;
        return cidKey;
    }
    return 0;
}
static uint32_t sDnsParseGenID = 0;
ConnID XNetEngine::ConnectToDomain(const char* domain,HostPort port)
{
    static unsigned short serial=1;
    RakAssert(mRakPeer);
    ConnInfo *pci=NULL;
    if(!mIsStartup) 	{	return 0;	}
    int hash_or_id = xBKDRHash(domain);
    ConnID cidKey(serial++,hash_or_id);	//第一个参数不为0,接受这里给的递增数,使得同一个域名不同端口可以区别ConID
    //if(serial==0)serial++;
    JDLog("cidkey@ConnectToDomain %llu",cidKey.u.id64);
    //PeerInfoMutex.Lock();


    if(ConnectingInfoMap.Has(cidKey))
    {
        pci = ConnectingInfoMap.Get(cidKey);
        JELog("ConnID collision2");
    }
    else pci = new ConnInfo;
    pci->targetID = ++sDnsParseGenID;
    pci->parseHandle = ResolveDomain(domain,port,(uint32_t)pci->targetID);
    pci->directConn = 0;
    pci->status = ConnI_DomainResolve; //ConnI_ConnectingEx 通过targetID能够唯一找到的.否则得用别的
    ConnectingInfoMap.Set(cidKey,pci);		JDLog("ConnectingInfoMap.Set %llu",cidKey.u.id64);
    if(pci->parseHandle==NULL || hash_or_id==0)
    {
        JELog("ConnectToDomain[%s|%hu] Directly Failed.",domain,port);
        if(pci)delete pci;
        if(cidKey)ConnectingInfoMap.Delete(cidKey);
        cidKey.u.id64=0;
    }
    return cidKey;
}
//CONNECTION_ATTEMPT_STARTED, 成功
//INVALID_PARAMETER, 失败
//CANNOT_RESOLVE_DOMAIN_NAME,失败
//ALREADY_CONNECTED_TO_ENDPOINT,  成功,特殊.代发消息？ 应该避免 算失败比较合理
//CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS,忽略, 成功
//SECURITY_INITIALIZATION_FAILED 失败

//ConnID XNetEngine::ConnectEx(NetConnExMethod method,NetID aidID,NetID tarID)
//{
//	return _ConnectEx(method,aidID,tarID);
//}

ConnID XNetEngine::ConnectEx(NetConnExMethod method,NetID aidID,NetID tarID)
{
    bool connOK=false;
    RakAssert(mRakPeer);
    ConnInfo *pci=NULL;
    if(!mIsStartup || !PeerInfoMap.Has(aidID))
    {	return 0;	}
    ConnID cidKey(0,tarID&0xFFFFFFFF);	//产生一个id
    JDLog("cidkey@connex %llu",cidKey.u.id64);
    //PeerInfoMutex.Lock();
    if(ConnectingInfoMap.Has(cidKey))
    {
        pci = ConnectingInfoMap.Get(cidKey);
        JELog("ConnID collision");
    }
    else pci = new ConnInfo;
    pci->directConn = 0;
    pci->targetID = tarID;
    pci->status = ConnI_ConnectingEx; //ConnI_ConnectingEx 通过targetID能够唯一找到的.否则得用别的
    pci->parseHandle = NULL;
    ConnectingInfoMap.Set(cidKey,pci);		JDLog("ConnectingInfoMap.Set %llu",cidKey.u.id64);

    //先判断一下目标是不是已经连好了
    RakNetGUID guid(tarID);
    if(IS_CONNECTED == mRakPeer->GetConnectionState(guid))
    {
        MessageID type = ID_USER_ALREADY_CONNECTED;
        JDLog("Already connected.directly return.");
        _SendToLoopback(RakNetGUID(tarID),(NetPacket*)&type,0,mRakPeer->GetSystemAddressFromGuid(guid),ID_USER_ALREADY_CONNECTED);
        return cidKey;
    }

    switch (method)
    {
    case CONN_METHOD_PASSIVE:
    case CONN_METHOD_PUNCHTHROUGH:
        connOK = natPunchthroughClient.OpenNAT(RakNetGUID(tarID),PeerInfoMap.Get(aidID)->addr);
        goto func_return;
    case CONN_METHOD_PROXY:
        {
            ProxyTunnel *pt=NULL;
            if(ProxyTunnelMap.Has(tarID))
            {
                pt = ProxyTunnelMap.Get(tarID);
                if(pt->tick==0 || mProcTime - pt->tick < mRakPeer->GetTimeoutTime(UNASSIGNED_SYSTEM_ADDRESS)) //表示通道没过期
                {
                    JDLog("reuse proxy tunnel.conn %llu via %s",tarID,pt->proxyAddress.ToString(true));
                    ConnectingInfoMap.Delete(cidKey);
                    return this->Connect(pt->proxyAddress.ToString(true));
                }
                else
                {	//此处的移除过期是不够的 析构时需要处理
                    delete pt;
                    ProxyTunnelMap.Delete(tarID);
                }
            }
            connOK = udpProxyClient.RequestForwarding( PeerInfoMap.Get(aidID)->addr,UNASSIGNED_SYSTEM_ADDRESS,RakNetGUID(tarID),mRakPeer->GetTimeoutTime(UNASSIGNED_SYSTEM_ADDRESS));//UDP_FORWARDER_MAXIMUM_TIMEOUT


        }
        goto func_return;
    default:
        connOK = false;
        goto func_return;
    }
func_return:
    //PeerInfoMutex.Unlock();
    if(!connOK)
    {
        JELog("ConnectEx[%d] to %llu Directly Failed.",method,tarID);
        if(pci)delete pci;
        if(cidKey)ConnectingInfoMap.Delete(cidKey);
        JDLog("ConnectingInfoMap.Delete %llu",cidKey.u.id64);
        cidKey.u.id64=0;
    }
    return cidKey;
}

bool XNetEngine::Send(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast,int head)
{
    if(head<1||head>PACKET_DELIVER_HEAD_MAX)head = PACKET_DELIVER_HEAD_MAX;
    packet->PacketType = (unsigned char)(PACKET_DELIVER_TYPE_MAX-head);
    if(orderingChannel<0)orderingChannel=-orderingChannel;   //-(-128)=1  -(-128)=-128
    orderingChannel++;	//也就是orderingchannel 0被保留
    return SendPacketInternal(packet,DataLenInPack,priority,packStyle,orderingChannel,targetID,boardcast);
}

//internal 和send的区别是直接使用packet->PacketType
bool XNetEngine::SendPacketInternal(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast)
{
    RakNetGUID guid = UNASSIGNED_RAKNET_GUID;
    bool rt=false;
    RakAssert( packet && DataLenInPack > 0 );
    RakAssert(mRakPeer);
    if(orderingChannel<0)orderingChannel=-orderingChannel;
    //if(head<1||head>PACKET_DELIVER_HEAD_MAX)head = PACKET_DELIVER_HEAD_MAX;
    if(targetID==NETID_TARGET_ALL)boardcast = true;
    if(!boardcast && (targetID==NETID_SELF || targetID==mNetID))
    {
        return this->SendToLoopback(mNetID,packet,DataLenInPack,
            (packet->PacketType>(PACKET_DELIVER_TYPE_MAX-PACKET_DELIVER_HEAD_MAX))?(PACKET_DELIVER_TYPE_MAX-packet->PacketType):0);
    }
    //PeerInfoMutex.Lock();
    if(PeerInfoMap.Has(targetID))
    {
        guid = PeerInfoMap.Get(targetID)->guid;
    }
    if(guid!=UNASSIGNED_RAKNET_GUID || boardcast)
    {
        //head%=PACKET_DELIVER_HEAD_MAX;
        //packet->PacketType = (unsigned char)(PACKET_DELIVER_TYPE_MAX-head);
        if(0!=mRakPeer->Send((char*)packet,DataLenInPack+1,(PacketPriority)priority,(PacketReliability)packStyle,orderingChannel,guid,boardcast))
        {
            rt = true;
        }
    }
    //xxx PeerInfoMutex.Unlock();

    return rt;
}


bool XNetEngine::Close(NetID targetID,char orderingChannel)
{
    //if(m_RakPeer==NULL)return false;
    RakAssert(mRakPeer);
    if(targetID == NETID_SELF)return true;//@c.12.19不需要删除对自己的映射.否则自己不能给自己发数据
    if(targetID == NETID_TARGET_ALL)
    {
        //这里使用shutdown是不对的!它会关闭监听端口
        //m_RakPeer->Shutdown(1000);
        //PeerInfoMutex.Lock();
        while(PeerInfoMap.Size())
        {
            mRakPeer->CloseConnection(PeerInfoMap[0]->guid,true,orderingChannel+1,LOW_PRIORITY); // 这里使用高优先可能抢先于数据 断开了连接
            delete PeerInfoMap[0];
            PeerInfoMap.RemoveAtIndex(0);
        }

        //OperatPeerInfo(mRakPeer->GetMyGUID(),mSystemAddress); //删完又添回去,使得自己可以已连接方式发消息给自己
        //PeerInfoMutex.Unlock();
    }
    else
    {
        //PeerInfoMutex.Lock();
        if(PeerInfoMap.Has(targetID))
        {
            PeerInfo* pi = PeerInfoMap.Get(targetID);
            mRakPeer->CloseConnection(pi->guid,true,orderingChannel+1,LOW_PRIORITY);
            delete pi;
            PeerInfoMap.Delete(targetID);
        }
        else
        {
            return false;
        }
        //PeerInfoMutex.Unlock();
    }
    return true;
}

bool XNetEngine::SendToRaw(const char *target, unsigned short port, const char *netpack, int len)
{
    return mRakPeer->SendRawUDP(target,port,netpack,len);
}
void XNetEngine::ManualFree(void* toFree)	//一定是非helper
{
    mRakPeer->DeallocatePacket((Packet*)toFree);
}
//NetID XNetEngine::PopProxyPrefetch(const NetID &aidID,int &idx)
//{
//	NetID tarID = 0;
//	int size = ProxyPreFetchMap.Size();
//	for(;idx<size;idx++)
//	{
//		if(ProxyPreFetchMap[idx] == aidID)
//		{
//			tarID = ProxyPreFetchMap.GetKeyAtIndex(idx);
//			ProxyPreFetchMap.RemoveAtIndex(idx);
//			break;
//		}
//	}
//	return tarID;
//}
bool XNetEngine::ProcPacket(_nInt64 timetick, bool isProcAll)
{
    bool ret=true;
    static _nInt64 CheckTimer=0;
    ret = _ProcPacket(isProcAll);

    mProcTime = timetick;
    //unsigned int ticktime = xGetTickCount();
    if(mProcTime-CheckTimer>1000)
    {
        CheckTimer = mProcTime;
        //this->CheckResolveResult(); //检查解析情况
        this->SpreadProbes();  //发送探测包
    }
    return ret;
}
ConnID XNetEngine::_GetConnID__DomainResolve(uint32_t id,bool pop)
{
    ConnID cid=0;
    ConnInfo* pci=NULL;
    int size;
    size = ConnectingInfoMap.Size();
    for(int i=0;i<size;i++)
    {
        if(ConnectingInfoMap[i]->targetID ==(NetID) id && ConnectingInfoMap[i]->status==ConnI_DomainResolve)
        {
            cid = ConnectingInfoMap.GetKeyAtIndex(i);
            pci = ConnectingInfoMap[i];
            goto to_rtn;
        }
    }
to_rtn:
    if(cid && pop && pci)
    {
        delete pci;
        ConnectingInfoMap.Delete(cid);
    }
    return cid;
}
ConnID XNetEngine::_GetConnID(const SystemAddress &sa,const RakNetGUID &guid,bool pop,unsigned char multiUseMsg)
{
    ConnID cid=0;
    ConnInfo* pci=NULL;
    if(sa!=UNASSIGNED_SYSTEM_ADDRESS)		//有地址必须从地址优先找
    { //从地址找ConnID
        ConnID fConn(sa.address.addr4);
        if(ConnectingInfoMap.Has(fConn)) //是个普通连接
        {
            cid = fConn;
            pci = ConnectingInfoMap.Get(fConn);
            if(pci->status == ConnI_ConnOnAdvance)	//是个附加出来的记录
            {
                if(pop)ConnectingInfoMap.Delete(fConn);//删掉当前这个'新自生成记录'
                cid = _GetConnID(UNASSIGNED_SYSTEM_ADDRESS,RakNetGUID(pci->targetID),pop,0);//pop的话也删掉起始那个记录
                if(!cid){JELog("Can't find record via ConnI_ConnOnAdvance");}
                if(pop)delete pci;
                return cid;
            }
            else if(pci->status == ConnI_DomainResolve) //找到domain解析
            {
                if(pop)ConnectingInfoMap.Delete(fConn);//删掉当前这个'新自生成记录' pci还能用
                cid = pci->directConn;  //直接用到原始connid 所以要另外自己删起始那个记录
                //cid = _GetConnID(UNASSIGNED_SYSTEM_ADDRESS,RakNetGUID(pci->targetID),pop);
                if(pop)delete pci;
                if(!cid){JELog("Can't find record via ConnI_DomainResolve");}
                else if(pop)
                {
                    pci = ConnectingInfoMap.Get(cid);
                    delete pci;
                    ConnectingInfoMap.Delete(cid);
                }
                return cid;
            }
            else if(pci->status == ConnI_ConnectingMulti)  //复合了domain和直连的情况
            {	//采取的办法是处理掉以后,向消息泵再投递一个消息,就会重做一遍了.此遍和ConnI_DomainResolve类似.因为共用无论如何不pop
                JDLog("ConnI_ConnectingMulti") ;
                cid = pci->directConn;
                if(!cid){JELog("Can't find record via ConnI_ConnectingMulti");}
                pci->status = ConnI_Connecting;
                pci->targetID = guid.g; //在普通连接中,这个ID不起作用
                pci->directConn = fConn;
                if(!multiUseMsg)JELog("no multiUseMsg at ConnectionMulti");
                else
                {
                    NetPacket nm;
                    _SendToLoopback(guid,&nm,0,sa,multiUseMsg);
                }
                return cid;
            }
            //if(guid==UNASSIGNED_RAKNET_GUID)
            //{
            //	guid.g = pci->targetID; //如果直接得到的guid是未知而ConnectingInfoMap里有targetID覆盖一下
            //}
            goto to_rtn;
        }
    }
    //从NetID遍历找ConnID
    if(guid!=UNASSIGNED_RAKNET_GUID)
    {
        int size;
        size = ConnectingInfoMap.Size();
        for(int i=0;i<size;i++)
        {
            if(ConnectingInfoMap[i]->targetID == guid.g)
            {
                cid = ConnectingInfoMap.GetKeyAtIndex(i);
                pci = ConnectingInfoMap[i];
                goto to_rtn;
            }
        }
    }
to_rtn:
    if(cid && pop && pci)
    {
        delete pci;
        ConnectingInfoMap.Delete(cid);
    }
    return cid;
}

bool XNetEngine::_ProcPacket(bool isProcAll)
{
    bool bHelperPacket=false;
    Packet *packet;
    NetEnginePackMsg* netpacket;
    NetEngineMsg* netpacket_raise; //用来提交,如果不移动这个指针,netpacket_raise和netpacket是一样操作的
    NetEngineConnection netConn;
    do
    {
        int head = 0;
        packet = mRakPeer->Receive();
        if(!packet && mRakPeerHelper)
        {
            packet = mRakPeerHelper->Receive();
            bHelperPacket = true;
        }
        if(packet)
        {
            int ret=-1;
            int len_raise = packet->length-1;
            /// 将RakNet首字节翻译成与RakNet无关的PacketType
            netpacket = (NetEnginePackMsg*)packet->data;
            netpacket_raise = (NetEngineMsg*)packet->data; //初始和netpacket一样
            //JDLog("DoReceiveBase pStat %d GetPacket~%d len %u",paceStat,netpacket->PacketType,packet->length);
            if(netpacket->PacketType0>=(PACKET_DELIVER_TYPE_MAX-PACKET_DELIVER_HEAD_MAX))
            {
                head = PACKET_DELIVER_TYPE_MAX - netpacket->PacketType0;
                netpacket->PacketType0 = TYPE_APPLICATION_DATA;
            }
            else switch ((unsigned char)netpacket->PacketType0)
            {
                //case ID_USER_PACKET_LOOPBACK:
                //	netpacket->PacketType0 = TYPE_LOOPBACK_DATA;
                //	break;
                case ID_USER_PACKET_SPECIFIC:		//这个id是由咱们sendtoloopback产生的
                    netpacket_raise = (NetEngineMsg*)&netpacket->PacketType1; //后移这个包
                    len_raise--;
                    break;
                case ID_OUT_OF_BAND_INTERNAL:		//目前这个id是由咱们sendtoloopback产生的..更底层也会产生这个,但是不会也不该泄露到我们上层
                    netpacket->PacketType0 = TYPE_OUT_CONNECTION_DATA;
                    break;
                case ID_ADVERTISE_SYSTEM:
                    //JDLog("ID_ADVERTISE_SYSTEM %llu,%llu",packet->guid.g,mNetID);
                    //JDLogHex((const unsigned char*)netpacket,len_raise);
                    netpacket_raise = (NetEngineMsg*)&netpacket->PacketType1; //后移这个包
                    len_raise--;
                    switch(netpacket_raise->PacketType0)
                    {
                    case ID_USER_NETENGINE_DISCOVER:
                        //JDLog("ID_USER_NETENGINE_DISCOVER %d,%llu,%llu",len_raise,packet->guid.g,mNetID);
                        if(len_raise>=(int)sizeof(NetEnginePackMsg)-1 && packet->guid.g!=mNetID)
                        {
                            //bool bNewDiscoInfo=false; //当新建了DiscoInfo时置true
                            PeerInfo *p=NULL;
                            NetEnginePackMsg* pnepm = (NetEnginePackMsg*)netpacket_raise;

                            switch(pnepm->PacketType1)
                            {
                            case Disco_Reply:
                            case Disco_Echo:
                            case Disco_ETReply:
                            case Disco_ExtendTool:	 //no break;  是考虑可能对云白板客户端的干扰 ，现在考虑他们netid不同 不会干扰
                                break; //考虑如果不加入DiscoInfo 拓扑层调用hasknowleadge得不到id对应的地址，无法发送非连接数据
                            case Disco_Quit:
                                if(DiscoInfoMap.Has(packet->guid.g))DiscoInfoMap.Delete(packet->guid.g);
                                goto after_dicso_info_handled;
                            default:
                                goto after_dicso_info_set;
                            }

                            if(DiscoInfoMap.Has(packet->guid.g))
                            {
                                p=DiscoInfoMap.Get(packet->guid.g);
                                if(p->peerFlag & FLAG_DISCO_DIRECT || pnepm->data[0]==0) //如果原来的标志是定向 或者新信息来自广播,更新信息
                                {
                                    p->guid = packet->guid;
                                    p->addr = packet->systemAddress;
                                    p->bdPort = pnepm->AsstID;
                                }
                            }
                            else
                            {
                                p = new PeerInfo;
                                p->peerFlag = 0;
                                p->guid = packet->guid;
                                p->addr = packet->systemAddress;
                                p->bdPort = pnepm->AsstID;
                                DiscoInfoMap.Set(packet->guid.g,p);
                                //bNewDiscoInfo = true;
                            }
 after_dicso_info_set:
                            if(pnepm->PacketType1 != Disco_Reply && (mDiscoverType==Disco_Echo || mDiscoverType==Disco_ExtendTool))	   //只有Echo型的才回复
                            {
                                ReactiveDisco((mDiscoverType==Disco_ExtendTool)?Disco_ETReply:Disco_Reply,(pnepm->data[0]&FLAG_DISCO_DIRECT)?_inet_ntoa(packet->systemAddress.address.addr4.sin_addr):NULL);//返回0对方不做回复

                                netpacket_raise->PacketType0 = ID_USER_NETENGINE_DISCOVER;//包要比较短 会被忽略 开arp用.这个ID会被SendToEx覆盖,可以不设置
                                this->SendToEx(packet->systemAddress.ToString(false),pnepm->AsstID,(NetPacket*)netpacket_raise,sizeof(NetEngineMsg)-1,ID_USER_NETENGINE_DISCOVER);
                            }
after_dicso_info_handled:
                            if(mbDiscoReport)
                            {
                                pnepm->PacketType0 = TYPE_NETENGINE_REPORT;
                                break;   //如果要汇报，跳出这个case，数据被提交
                            }
                        }
                        //mRakPeer->DeallocatePacket(packet);
                        _DeallocatePacket(packet,bHelperPacket);
                        continue;//continue 表示不析构数据包 除非自己已经析了
                    case ID_ADVERTISE_SYSTEM:
                        netpacket_raise->PacketType0 = TYPE_OUT_CONNECTION_DATA;
                        break;//ret默认-1,break下去是会析的
                    case ID_USER_ADVERTISESYSTEM:
                        OnAdvertiseSystem(packet,bHelperPacket);
                        continue; //continue 表示不析构数据包
                    }
                    break;
                case ID_CONNECTION_LOST:
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_LOST;
                    this->OperatPeerInfo(packet->guid,packet->systemAddress,true);//移除
                    break;
                case ID_USER_ALREADY_CONNECTED:
                    netpacket_raise = (NetEngineMsg*)&netConn;
                    len_raise = sizeof(NetEngineConnection)-1;
                    netpacket_raise->PacketType0 = TYPE_ALREADY_CONNECTED;
                    netConn.id = _GetConnID(packet->systemAddress,packet->guid,true,(unsigned char)netpacket->PacketType0);
                    this->OperatPeerInfo(packet->guid,packet->systemAddress);
                    break;
                case ID_CONNECTION_REQUEST_ACCEPTED:
                    netpacket_raise = (NetEngineMsg*)&netConn;
                    len_raise = sizeof(NetEngineConnection)-1;
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_ACCEPT;
                    netConn.id = _GetConnID(packet->systemAddress,packet->guid,true,(unsigned char)netpacket->PacketType0);
                    this->OperatPeerInfo(packet->guid,packet->systemAddress);
                    break;
                case ID_NEW_INCOMING_CONNECTION:
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_INCOMING;
                    this->OperatPeerInfo(packet->guid,packet->systemAddress);
                    break;
                case ID_NAT_ALREADY_IN_PROGRESS:				//Bytes starting at offset 1 contains the RakNetGUID
                case ID_NAT_TARGET_NOT_CONNECTED:			//Bytes starting at offset 1 contains the RakNetGUID
                case ID_NAT_TARGET_UNRESPONSIVE:				//Bytes starting at offset 1 contains the RakNetGUID
                case ID_NAT_CONNECTION_TO_TARGET_LOST:	//Bytes starting at offset 1 contains the RakNetGUID
                    {	//因为数据是大端的..不能直接转化 从raknet里抓出来的处理方法
                        RakNet::BitStream inBitStream((unsigned char *) &(netpacket->PacketType1), sizeof(RakNetGUID), false);
                        inBitStream.Read(packet->guid);//改成打孔目标的guid
                    }
                case ID_NAT_PUNCHTHROUGH_FAILED:	//它和上面四个不同,没有为offset1填充目标GUID,packet里面的id就是目标id

                case ID_ALREADY_CONNECTED:
                    //表示你所想连接的目标认为你已经和他连着的,可你自己不一定这么认为.这通常是没来数据有去数据.
                    //如果真的有连,那么connect会立即表示already connect.到这里算失败,最多需要TimeOutTime后重试
                    //http://www.jenkinssoftware.com/forum/index.php?topic=4727.0
                case ID_CONNECTION_ATTEMPT_FAILED:
                case ID_CONNECTION_BANNED:
                case ID_NO_FREE_INCOMING_CONNECTIONS:
                    netpacket_raise = (NetEngineMsg*)&netConn;
                    len_raise = sizeof(NetEngineConnection)-1;
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_FAILED;
                    netConn.id = _GetConnID(packet->systemAddress,packet->guid,true,(unsigned char)netpacket->PacketType0);
                    break;
                case ID_INVALID_PASSWORD:
                case ID_INCOMPATIBLE_PROTOCOL_VERSION:
                    JELog("ConnFaild [%u]",netpacket->PacketType0);
                    netpacket_raise = (NetEngineMsg*)&netConn;
                    len_raise = sizeof(NetEngineConnection)-1;
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_FAILED;
                    netConn.id = _GetConnID(packet->systemAddress,packet->guid,true,(unsigned char)netpacket->PacketType0);
                    mRakPeer->CloseConnection(packet->guid,false); //@d.5.14 立即重连会返回already_connect_endpoint.特意关一次看看 http://www.jenkinssoftware.com/forum/index.php?topic=4875.0
                    break;
                case ID_USER_PROXY_ESTABLISH:
                    // 仿照ID_NAT_PUNCHTHROUGH_SUCCEEDED,1时我们是发起者
                    // 不同的是我们自己构造的消息中包括了目标的外部地址
                    // 此时packet中的地址是代理的服务地址,消息中的地址是目标与代理联系的外部地址
                    if(netpacket->PacketType1==1) // 我们是发起者. 代理完成后还需要连接过去.
                    {
                        NetMsgProxyEstablish *pnmpe = (NetMsgProxyEstablish*)netpacket;
                        netConn.id = _GetConnID(UNASSIGNED_SYSTEM_ADDRESS,packet->guid,false);
                        //代理或打孔完成,上一句找出连接初期自己生成的cid,然后下面连接代理地址又产生一个新的cid.
                        //此cid注明status为ConnI_ConnOnAdvance. 当后生成的这个id被_GetConnID优先获取到(获取id函数中地址优先)
                        //发现status为OnAdvance就会去找初始的那个cid(初始cid是connect调用者握在手中的识别id,需要返给他)然后
                        //删掉在此生成的新cid,也就是下面那个connect产生的cidAdv.   不难理解此处如果删初期cid调用者将无法得到id匹配
                        if(netConn.id)
                        {
                            ConnID cidAdv;
                            ConnInfo *pci_;
                            JDLog("update PROXY conninfo %s",packet->systemAddress.ToString());
                            pci_ = ConnectingInfoMap.Get(netConn.id);
                            pci_->directConn.Set(pnmpe->targetAddr.address.addr4);
                            cidAdv = this->Connect(packet->systemAddress.ToString(false),packet->systemAddress.GetPort());
                            if(!cidAdv)
                            {
                                JDLog("PROXY success,connect %s directly failed",packet->systemAddress.ToString());
                                MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
                                _SendToLoopback(packet->guid,(NetPacket *)&type,0,packet->systemAddress,ID_CONNECTION_ATTEMPT_FAILED);
                            }
                            else
                            {
                                pci_ = ConnectingInfoMap.Get(cidAdv);
                                pci_->targetID = packet->guid.g;
                                pci_->status = ConnI_ConnOnAdvance;
                            }
                        }
                        else
                        {
                            JELog("Can't find record %llu at ConnectingInfoMap when PROXY_ESTABLISH.",packet->guid.g);
                        }
                    }
                    //mRakPeer->DeallocatePacket(packet);
                    _DeallocatePacket(packet,bHelperPacket);
                    continue;
                case ID_NAT_PUNCHTHROUGH_SUCCEEDED:
                    //代理或打孔成功后,旧ConnID关联的directConn更新成目标的外部地址,打孔方式使用这个地址connect,代理方式不同,使用的是代理的外部地址connect(非目标的外部地址)
                    //下面的connect导致新生成一条ConncetingInfoMap信息,新信息使用了connect使用的地址作为key,会被优先_GetConnID到
                    //如果保证此时connect失败能得到目标id,那处理办法是删掉新生成的才能保证调用者得到预期的ConnID
                    //能保证吗?大概是不能.所以只能保留新记录,修改status为ConnI_ConnOnAdvance. 此时要保证的是新旧ConnID关联的targetID一样
                    if(netpacket->PacketType1==1) // 我们是发起者. 打孔完成后还需要连接过去.
                    {
                        netConn.id = _GetConnID(UNASSIGNED_SYSTEM_ADDRESS,packet->guid,false);
                        if(netConn.id)
                        {
                            ConnID cidAdv;
                            ConnInfo *pci_;
                            JDLog("update PUNCHTHROUGH conninfo %s",packet->systemAddress.ToString());
                            pci_ = ConnectingInfoMap.Get(netConn.id);
                            pci_->directConn.Set(packet->systemAddress.address.addr4);
                            cidAdv = this->Connect(packet->systemAddress.ToString(false),packet->systemAddress.GetPort());
                            if(!cidAdv)
                            {
                                JDLog("PUNCHTHROUGH success,connect %s directly failed",packet->systemAddress.ToString());
                                MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
                                _SendToLoopback(packet->guid,(NetPacket *)&type,0,packet->systemAddress,ID_CONNECTION_ATTEMPT_FAILED);
                            }
                            else
                            {
                                pci_ = ConnectingInfoMap.Get(cidAdv);
                                pci_->targetID = packet->guid.g;
                                pci_->status = ConnI_ConnOnAdvance;
                            }
                            //修改status 为 ConnI_ConnOnAdvance
                        }
                        else
                        {
                            JELog("Can't find record %llu at ConnectingInfoMap when PUNCHTHROUGH_SUCCEEDED.",packet->guid.g);
                        }
                    }
                    //mRakPeer->DeallocatePacket(packet);
                    _DeallocatePacket(packet,bHelperPacket)	;
                    continue;
                case ID_DISCONNECTION_NOTIFICATION:
                    netpacket_raise->PacketType0 = TYPE_CONNECTION_CLOSE;
                    this->OperatPeerInfo(packet->guid,packet->systemAddress,true);
                    break;
                    //TODO: 会用到Plugin的消息
                case ID_UNCONNECTED_PING:
                case ID_UNCONNECTED_PONG:
                    //经由代理的连接建立时会泄露这两个信息上来.参见UDPProxyClient.cpp Line:197
                case ID_UNCONNECTED_PING_OPEN_CONNECTIONS:
                    //mRakPeer->DeallocatePacket(packet);
                    _DeallocatePacket(packet,bHelperPacket)	;
                    continue;
                default:
                    JELog("{NetWrapper::DoReceive}Unexpect Netmessage %d",netpacket_raise->PacketType0);
                    //JDLog("ID_RELAY_PLUGIN=%d ID_USER_PACKET_ENUM=%d",ID_RELAY_PLUGIN,ID_USER_PACKET_ENUM);
#ifdef _DEBUG
                    netpacket_raise->PacketType0 = TYPE_NOT_DEFINED;		   // 编写的时候没有考虑到的
                    break;
#else
                    //mRakPeer->DeallocatePacket(packet); // 丢弃不认识的数据包
                    _DeallocatePacket(packet,bHelperPacket)	;
                    continue;
#endif
            }
            if(mSinkForNetEngine)
                ret = mSinkForNetEngine->OnNetMsg(this,packet->guid.g,packet->systemAddress.address.addr4,netpacket_raise,len_raise,head);

            if(ret<0 || bHelperPacket)_DeallocatePacket(packet,bHelperPacket);//mRakPeer->DeallocatePacket(packet);    //只提供非helper rakpeer的包做延缓释放
            else mSinkForNetEngine->OnDelegateFree(this,(void*)packet,(NetEngineMsg*)packet->data,packet->length-1,ret);


            if(!isProcAll) // OnNetMsg一次就退出本函数了.便于调用者调度
            {	return true; }
        }
    } while (packet);
    return false;
}
bool XNetEngine::OnDatagramHandler(RNS2RecvStruct* recvStruct)
{
    if(gNetEngineOpenFilter)
    {
        return gNetEngineOpenFilter->mSinkForNetEngine->OnDatagram
                (recvStruct->data,recvStruct->bytesRead,
                 recvStruct->systemAddress.address.addr4.sin_addr.s_addr,
                 recvStruct->systemAddress.address.addr4.sin_port);
    }
    return true;
}
void XNetEngine::OpenDatagramFilter()
{
    gNetEngineOpenFilter = this;
    mRakPeer->SetIncomingDatagramEventHandler(OnDatagramHandler);
}
void XNetEngine::_DeallocatePacket(Packet* packet,bool bHelperPacket)
{
    if(bHelperPacket)mRakPeerHelper->DeallocatePacket(packet);
    else mRakPeer->DeallocatePacket(packet);
}
bool XNetEngine::SetTimeOutTime(unsigned int timeoutTime)
{
    if(!mRakPeer)return false;
    mRakPeer->SetTimeoutTime(timeoutTime,UNASSIGNED_SYSTEM_ADDRESS);
    return true;
}
bool XNetEngine::SetMaxIncomingConnections(unsigned short maximumIncomingConnections)
{
    if(!mRakPeer)return false;
    mRakPeer->SetMaximumIncomingConnections(maximumIncomingConnections);
    return true;
}
void XNetEngine::OnAdvertiseSystem(RakNet::Packet *packet,bool bHelperPacket)
{
    //RakNet::Packet*
    RakNet::Packet** ppp;
    int s,t;	//s是0~MAX_OFFLINE_SEGMENTS,当前序.t为该包的分片总数目
    //JDLog("\t\t\tAdviseSystemEx packet.");
    NetEnginePackMsg *nepm = (NetEnginePackMsg*)packet->data;

    RakNet::BitStream inBitStream(packet->data+2, sizeof(NetEnginePackMsg)-2, false);
    inBitStream.Read(nepm->MainID);
    inBitStream.Read(nepm->AsstID);  //这段保证大小端都起作用

    s = nepm->MainID % MAX_OFFLINE_SEGMENTS;
    t = (nepm->AsstID+MAX_OFFLINE_DATA_LENGTH-1) / MAX_OFFLINE_DATA_LENGTH;
    if(nepm->PacketType1!=ID_USER_ADVERTISESYSTEM || s>=t || t>MAX_OFFLINE_SEGMENTS  //sizeof(NetEngineMsg)其实是sizeof(NetEnginePackMsg)-1
        || (s+1!=t && packet->length!=MAX_OFFLINE_DATA_LENGTH+sizeof(NetEngineMsg))) //如果不是序列最后一个包,那包长应该是最大长度
    {
        _DeallocatePacket(packet,bHelperPacket)	;//mRakPeer->DeallocatePacket(packet);
        JELog("Unexpect AdviseSystemEx packet.");
        return ;
    }
    //JDLog("收到分包 串%u,负载长度%d",nepm->MainID,nepm->AsstID);

    bool readyForRaise = true;
    uint64_t uPackSerial = packet->guid.g ^ (nepm->MainID/MAX_OFFLINE_SEGMENTS*MAX_OFFLINE_SEGMENTS);
    //(nepm->MainID/10*10)是序列的第一个序号.上面uPackSerial是可能碰撞的,但是概率很小
    if(!LongPackCache.Has(uPackSerial))
    {
        ppp = new RakNet::Packet*[MAX_OFFLINE_SEGMENTS];
        memset(ppp,0,sizeof(RakNet::Packet*)*MAX_OFFLINE_SEGMENTS);
        LongPackCache.Set(uPackSerial,ppp);
        _PackCheck *pc = new _PackCheck;
        pc->serial = uPackSerial;
        pc->timetick = mProcTime;
        LongPackCacheCheckQueue.Push(pc);
    }
    else
    {
        ppp = LongPackCache.Get(uPackSerial);
    }
    if(ppp[s]) //已经有了?挺可疑的,难道uPackSerial碰撞了?只能是保证不泄露
    {
        JELog("OutBand Data Collision.");
        _DeallocatePacket(packet,bHelperPacket);//mRakPeer->DeallocatePacket(ppp[s]);
    }
    ppp[s]=packet;
    for(int i=0;i<t;i++)	//检查包是否到达完整
    {
        if(ppp[i]==NULL)
        {
            readyForRaise=false;
            break;
        }
    }
    if(readyForRaise) //组包
    {
        int len,uplen;
        NetID id=packet->guid.g;
        SystemAddress sa = packet->systemAddress;
        uplen = nepm->AsstID;
        NetPacket *pnp = (NetPacket*)new char[nepm->AsstID+1];
        pnp->PacketType = TYPE_OUT_CONNECTION_DATA;
        for(int i=0;i<t;i++)
        {
            len = (i+1==t)?(nepm->AsstID-MAX_OFFLINE_DATA_LENGTH*i) : MAX_OFFLINE_DATA_LENGTH ;
            memcpy(((char*)pnp) + i*MAX_OFFLINE_DATA_LENGTH+1,ppp[i]->data+sizeof(NetEngineMsg),len);//sizeof(NetEngineMsg)其实是sizeof(NetEnginePackMsg)-1
            _DeallocatePacket(ppp[i],bHelperPacket)	;//mRakPeer->DeallocatePacket(ppp[i]);
        }
        //JDLog("组包提交 len:%d",uplen);

        if(mSinkForNetEngine)
            mSinkForNetEngine->OnNetMsg(this,id,sa.address.addr4,(NetEngineMsg*)pnp,uplen,0);
        //m_RakPeer->DeallocatePacket(packet);
        delete[] (char*)pnp;
        delete[] ppp;
        LongPackCache.Delete(uPackSerial);
    }
    CheckLongPack(mProcTime,bHelperPacket);
    return;
}

void XNetEngine::OperatPeerInfo(const RakNetGUID &guid,const SystemAddress &address,bool remove)
{
    PeerInfo *pi;
    if(remove)
    {
        if(PeerInfoMap.Has(guid.g))
        {
            pi = PeerInfoMap.Get(guid.g);
            delete pi;
            PeerInfoMap.Delete(guid.g);
        }
        //+
        if(ProxyTunnelMap.Has(guid.g))
        {
            ProxyTunnelMap.Get(guid.g)->tick = mProcTime;
        }
    }
    else
    {

        if(PeerInfoMap.Has(guid.g))
            pi = PeerInfoMap.Get(guid.g);
        else
        {
            pi = new PeerInfo;
            pi->peerFlag = 0;
        }
        pi->addr = address;
        pi->guid = guid;

        PeerInfoMap.Set(guid.g,pi);
    }
}

//bool XNetEngine::SendTo(NetID target,NetPacket *packet, const int DataLenInPack)
//{
//	PeerInfo* p=NULL;
//	NetPort port=0;
//	if(PeerInfoMap.Has(target))
//	{
//		p = PeerInfoMap.Get(target);
//		if(p->bdPort)port = p->bdPort;
//		else port = p->addr.debugPort;
//	}
//	if(p)return SendToEx(p->addr.ToString(false),port,packet,DataLenInPack);
//}
bool XNetEngine::SendTo(const char* target,unsigned short port,NetPacket *packet, const int DataLenInPack)
{
    RakAssert(mRakPeer);
    RakAssert( packet && DataLenInPack > 0 );
    return this->SendToEx(target,port,packet,DataLenInPack);
}
bool XNetEngine::SendTo(const char* addrStr,NetPacket *packet, const int DataLenInPack)
{
    char *p;
    RakAssert(mRakPeer);
    RakAssert( packet && DataLenInPack > 0 );
    strcpy(stcBuff,addrStr);
    p = strrchr(stcBuff,'|');
    if(p)
    {
        unsigned short port;
        *p++=0;
        port=(unsigned short)atoi(p);
        return this->SendToEx(stcBuff,port,packet,DataLenInPack);
    }
    return false;
}
bool XNetEngine::_Broadcast(unsigned short port,NetPacket *packet, const int DataLenInPack,MessageID PackType)
{
    RakAssert(mRakPeer);
    RakAssert( packet && DataLenInPack > 0 );
    //if(port==bdListenPort && bdListenPortAnother)this->SendToEx("255.255.255.255",bdListenPortAnother,packet,DataLenInPack,PackType);
    //return this->SendToEx("255.255.255.255",port,packet,DataLenInPack,PackType);
    const char* Tar = NULL;
    unsigned int target	 ;
    for(int i = 0 ;i<sDiscoInfo.totalGroupCount;i++)
    {
        target = (sDiscoInfo.infoGroup[i].ip & sDiscoInfo.infoGroup[i].mask) | (~sDiscoInfo.infoGroup[i].mask);
        Tar = _inet_ntoa(target);
        if(port==bdListenPort && bdListenPortAnother)this->SendToEx(Tar,bdListenPortAnother,packet,DataLenInPack,PackType);
        this->SendToEx(Tar,port,packet,DataLenInPack,PackType);
    }
    if(sDiscoInfo.totalGroupCount==0)
    {
        if(port==bdListenPort && bdListenPortAnother)this->SendToEx("255.255.255.255",bdListenPortAnother,packet,DataLenInPack,PackType);
        this->SendToEx("255.255.255.255",port,packet,DataLenInPack,PackType);
    }
    return true;
}
bool XNetEngine::Broadcast(unsigned short port,NetPacket *packet, const int DataLenInPack)
{
    return this->_Broadcast(port,packet,DataLenInPack);
}
bool XNetEngine::Broadcast(NetPacket *packet, const int DataLenInPack)
{
    if(bdListenPort)return this->_Broadcast(bdListenPort,packet,DataLenInPack);
    return false;
}
bool XNetEngine::SendToEx(const char* target,unsigned short port,NetPacket *packet, const int DataLenInPack,MessageID PackType)
{	//PackType指定短消息包的packetType用于NetEngine自己的一些消息沟通
    bool ret=true;
    //跟RakPeer.cpp Line 119 MAX_OFFLINE_DATA_LENGTH 400 相关联
    RakAssert(DataLenInPack<(MAX_OFFLINE_DATA_LENGTH*MAX_OFFLINE_SEGMENTS));
    if(DataLenInPack<MAX_OFFLINE_DATA_LENGTH)
    {
        packet->PacketType = PackType;
        return mRakPeer->AdvertiseSystem(target,port,(char*)packet,DataLenInPack+1); //d.01.05连packettype一起发送
    }
    else
    {
        static unsigned short OuterExSerial=MAX_OFFLINE_SEGMENTS;
        int packets = (DataLenInPack+MAX_OFFLINE_DATA_LENGTH-1) / MAX_OFFLINE_DATA_LENGTH;
        //JDLog("Split to %d packets",packets);
        //RakNet::BitStream bs[10];
        for(int pack=0;pack<packets;pack++)
        {
            int len = ((pack+1) == packets)?(DataLenInPack-pack*MAX_OFFLINE_DATA_LENGTH) : MAX_OFFLINE_DATA_LENGTH;
            RakNet::BitStream bs;
            bs.Write((MessageID)ID_USER_ADVERTISESYSTEM); //这种写法大小端都不怕
            bs.WriteAlignedVar16((char*)&OuterExSerial);//插入一个序号
            bs.WriteAlignedVar16((char*)&DataLenInPack);//插入实际长度
            bs.WriteAlignedBytes((const unsigned char*) &packet->DataFirstByte+pack*MAX_OFFLINE_DATA_LENGTH,len);
            //JDLog("--发送分包%u,长度%d,->%s:%u",OuterExSerial,len,target,port);
            OuterExSerial++;
            if(false==mRakPeer->AdvertiseSystem(target,port, (const char*) bs.GetData(), bs.GetNumberOfBytesUsed()))
            {	ret=false;	break;}
            //JDLog("成功发送分包%u,长度%d,总负载长度%d",OuterExSerial-1,len,DataLenInPack);
        }
        //MAX_OFFLINE_SEGMENTS为10时,每个整十的序号就是第一包,
        OuterExSerial = (OuterExSerial/MAX_OFFLINE_SEGMENTS+1)*MAX_OFFLINE_SEGMENTS;
        if(OuterExSerial>65500)OuterExSerial = MAX_OFFLINE_SEGMENTS;
        return ret;
    }
}


bool XNetEngine::SendToLoopback(NetID from,NetPacket *packet, const int DataLenInPack,int head)
{
    SystemAddress sa = UNASSIGNED_SYSTEM_ADDRESS;
    MessageID type;
    PeerInfo* pi;
    if(head>PACKET_DELIVER_HEAD_MAX)return false;
    if(PeerInfoMap.Has(from))
    {
        pi = PeerInfoMap.Get(from);
        sa = pi->addr;
    }
    if(head==0)
    {	type = (MessageID)ID_OUT_OF_BAND_INTERNAL;	}
    else
    {	type = (MessageID)(PACKET_DELIVER_TYPE_MAX-head);	}
    return _SendToLoopback(RakNetGUID(from),packet,DataLenInPack,sa,type);
    //return _SendToLoopback(from,packet,DataLenInPack,sa,(MessageID)ID_USER_PACKET_LOOPBACK);
}
bool XNetEngine::_SendToLoopback(const RakNetGUID &from,NetPacket *packet, const int DataLenInPack,const SystemAddress &sa,MessageID rakNetMessageID)
{ //当指定rakNetMessageID==ID_USER_PACKET_SPECIFIC时,发送的包会原样上提给上层处理.否则到_procpacket自己会处理
    //JDLog("SendToLoopback MessageID:%u",rakNetMessageID);
    Packet *rkPacket;
    if(rakNetMessageID==ID_USER_PACKET_SPECIFIC)
    {
        rkPacket = mRakPeer->AllocatePacket(DataLenInPack+2);
        rkPacket->data[0] = rakNetMessageID;
        memcpy(rkPacket->data+1, packet, DataLenInPack+1);
    }
    else
    {
        rkPacket = mRakPeer->AllocatePacket(DataLenInPack+1);
        packet->PacketType = rakNetMessageID;
        memcpy(rkPacket->data, packet, DataLenInPack+1);
    }
    rkPacket->guid = from;
    rkPacket->systemAddress = sa;
    mRakPeer->PushBackPacket(rkPacket,false);
    return true;
}


























void XNetEngine::AttachAsPunchthroughServer(bool trueToAttach)
{
    RakAssert(mRakPeer);
    if(trueToAttach)
    {
#ifdef PUNCHTHROUGH_DEBUG
        natPunchthroughServer.SetDebugInterface(this);
#endif
        mRakPeer->AttachPlugin(&natPunchthroughServer);
    }
    else// if(isAttachPunchthroughServer && !trueToAttach)
    {
        mRakPeer->DetachPlugin(&natPunchthroughServer);
    }


}
void XNetEngine::AttachAsPunchthroughClient(bool trueToAttach)
{
    RakAssert(mRakPeer);
    if(trueToAttach)
    {
#ifdef PUNCHTHROUGH_DEBUG
        natPunchthroughClient.SetDebugInterface(this);
#endif
        mRakPeer->AttachPlugin(&natPunchthroughClient);
    }
    else
    {
        mRakPeer->DetachPlugin(&natPunchthroughClient);
    }
}
void XNetEngine::AttachAsUDPProxyCoordinator(bool trueToAttach)
{
    RakAssert(mRakPeer);
    if(trueToAttach)
    {
        mRakPeer->AttachPlugin(&udpProxyCoordinator);
        udpProxyCoordinator.SetRemoteLoginPassword(COORDINATER_PASSWORD);
    }
    else
    {
        mRakPeer->DetachPlugin(&udpProxyCoordinator);
    }
}

void XNetEngine::AttachAsUDPProxyServer(unsigned short maxForwardEntries,bool trueToAttach/*,NetID coordinator*/)
{
    RakAssert(mRakPeer);
    if(trueToAttach)
    {
        mRakPeer->AttachPlugin(&udpProxyServer);
        udpProxyServer.SetResultHandler(this);
        udpProxyServer.udpForwarder.SetMaxForwardEntries(maxForwardEntries);
    }
    else
    {
        mRakPeer->DetachPlugin(&udpProxyServer);
    }
}


void XNetEngine::AttachAsUDPProxyClient(bool trueToAttach)
{
    RakAssert(mRakPeer);
    if(trueToAttach)
    {
        mRakPeer->AttachPlugin(&udpProxyClient);
        udpProxyClient.SetResultHandler(this);
    }
    else
    {
        mRakPeer->DetachPlugin(&udpProxyClient);
    }
}


bool XNetEngine::JoinCoordinator(NetID coo)
{
    //PeerInfoMutex.Lock();
    if(PeerInfoMap.Has(coo))
    {
        bool ret;
        ret = udpProxyServer.LoginToCoordinator(COORDINATER_PASSWORD,PeerInfoMap.Get(coo)->addr);
        //PeerInfoMutex.Unlock();
        return ret;
    }
    return false;
    //PeerInfoMutex.Unlock();
}

























//UDPProxyClientResultHandler
void XNetEngine::OnForwardingSuccess(const char *proxyIPAddress, unsigned short proxyPort,
                                         SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    //作者为我们加了一个参数,有targetGuid更可靠好用了
    //http://www.jenkinssoftware.com/forum/index.php?topic=4758.0
    Q_UNUSED(proxyCoordinator);Q_UNUSED(sourceAddress);Q_UNUSED(proxyClientPlugin);
    NetMsgProxyEstablish nmpe;
    ProxyTunnel *pt=NULL;
    SystemAddress ProxyForwardAddr(proxyIPAddress,proxyPort);
    JDLog("Datagrams forwarded by proxy %s:%i to target %s.", proxyIPAddress, proxyPort, targetAddress.ToString());

    if(ProxyTunnelMap.Has(targetGuid.g))pt=ProxyTunnelMap.Get(targetGuid.g);
    else pt = new ProxyTunnel;
    pt->proxyAddress = ProxyForwardAddr;
    pt->tick = 0;
    ProxyTunnelMap.Set(targetGuid.g,pt);
    nmpe.positive = 1;
    nmpe.targetAddr = targetAddress;
    _SendToLoopback(targetGuid,(NetPacket*)&nmpe,sizeof(NetMsgProxyEstablish)-1,ProxyForwardAddr,ID_USER_PROXY_ESTABLISH);
    //if(CONNECTION_ATTEMPT_STARTED!=m_RakPeer->Connect(proxyIPAddress,proxyPort,DEFAULT_CONNECTION_PASSWORD,DEFAULT_CONNECTION_PASSWORDLENGTH))
    //	//if(CONNECTION_ATTEMPT_STARTED!=m_RakPeer->Connect(proxyIPAddress,proxyPort,0,0))
    //{
    //	unsigned char type = ID_CONNECTION_ATTEMPT_FAILED;//ID_NET_CONNECTION_FAILED;
    //	Packet *rkPacket = m_RakPeer->AllocatePacket(1);
    //	memcpy(rkPacket->data, &type, 1);
    //	rkPacket->guid = UNASSIGNED_RAKNET_GUID;//GetMyNetID();
    //	rkPacket->systemAddress = targetAddress;
    //	m_RakPeer->PushBackPacket(rkPacket,false);
    //}
}
void XNetEngine::OnForwardingNotification(const char *proxyIPAddress, unsigned short proxyPort,
                                              SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    Q_UNUSED(proxyCoordinator);Q_UNUSED(targetAddress);Q_UNUSED(proxyClientPlugin);
    JDLog("Source %s has setup forwarding to us through proxy %s:%i.", sourceAddress.ToString(false), proxyIPAddress, proxyPort);
    NetMsgProxyEstablish nmpe;
    SystemAddress ProxyForwardAddr(proxyIPAddress,proxyPort);
    nmpe.positive = 0;
    nmpe.targetAddr = targetAddress;
    _SendToLoopback(targetGuid,(NetPacket *)&nmpe,sizeof(NetMsgProxyEstablish)-1,ProxyForwardAddr,ID_USER_PROXY_ESTABLISH);
}
void XNetEngine::OnNoServersOnline(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    Q_UNUSED(proxyCoordinator);Q_UNUSED(sourceAddress);Q_UNUSED(proxyClientPlugin);
    JDLog("Failure: No servers logged into coordinator.\n");
    MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
    _SendToLoopback(targetGuid,(NetPacket *)&type,0,targetAddress,ID_CONNECTION_ATTEMPT_FAILED);
}
void XNetEngine::OnRecipientNotConnected(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    Q_UNUSED(proxyCoordinator);Q_UNUSED(sourceAddress);Q_UNUSED(proxyClientPlugin);//Q_UNUSED(targetGuid);
    JDLog("Failure: Recipient not connected to coordinator.");
    MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
    _SendToLoopback(targetGuid,(NetPacket *)&type,0,targetAddress,ID_CONNECTION_ATTEMPT_FAILED);
}
void XNetEngine::OnAllServersBusy(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    Q_UNUSED(proxyCoordinator);Q_UNUSED(sourceAddress);Q_UNUSED(proxyClientPlugin);
    JDLog("Failure: No servers have available forwarding ports.");
    MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
    _SendToLoopback(targetGuid,(NetPacket *)&type,0,targetAddress,ID_CONNECTION_ATTEMPT_FAILED);
}
void XNetEngine::OnForwardingInProgress(const char *proxyIPAddress, unsigned short proxyPort, SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
//void XNetEngine::OnForwardingInProgress(SystemAddress proxyCoordinator, SystemAddress sourceAddress, SystemAddress targetAddress, RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin)
{
    Q_UNUSED(proxyIPAddress);Q_UNUSED(proxyPort);
    Q_UNUSED(proxyCoordinator);Q_UNUSED(sourceAddress);Q_UNUSED(proxyClientPlugin);//Q_UNUSED(targetAddress);
    JDLog("Notification: Forwarding already in progress.");
    MessageID type = ID_CONNECTION_ATTEMPT_FAILED;
    _SendToLoopback(targetGuid,(NetPacket *)&type,0,targetAddress,ID_CONNECTION_ATTEMPT_FAILED);
}


//UDPProxyServerResultHandler
void XNetEngine::OnLoginSuccess(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
{
    Q_UNUSED(usedPassword);Q_UNUSED(proxyServerPlugin);
    JDLog("Login coordinator success");
    MessageID type = TYPE_COORDINATOR_JOIN_SUCCESS;
    _SendToLoopback(UNASSIGNED_RAKNET_GUID,(NetPacket *)&type,0,UNASSIGNED_SYSTEM_ADDRESS,ID_USER_PACKET_SPECIFIC);

}
void XNetEngine::OnAlreadyLoggedIn(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
{
    Q_UNUSED(usedPassword);Q_UNUSED(proxyServerPlugin);
    JDLog("Already join coordinator");
    MessageID type = TYPE_COORDINATOR_ALREADY_JOIN;
    _SendToLoopback(UNASSIGNED_RAKNET_GUID,(NetPacket *)&type,0,UNASSIGNED_SYSTEM_ADDRESS,ID_USER_PACKET_SPECIFIC);

}
void XNetEngine::OnNoPasswordSet(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
{
    Q_UNUSED (usedPassword); Q_UNUSED (proxyServerPlugin);
    JDLog("No password set on coordinator");
    MessageID type = TYPE_COORDINATOR_JOIN_FAILED;
    _SendToLoopback(UNASSIGNED_RAKNET_GUID,(NetPacket *)&type,0,UNASSIGNED_SYSTEM_ADDRESS,ID_USER_PACKET_SPECIFIC);
}

void XNetEngine::OnWrongPassword(RakNet::RakString usedPassword, RakNet::UDPProxyServer *proxyServerPlugin)
{
    Q_UNUSED(usedPassword);Q_UNUSED(proxyServerPlugin);
    JELog("Can't join coordinator because of wrong password.");
    MessageID type = TYPE_COORDINATOR_JOIN_FAILED;
    _SendToLoopback(UNASSIGNED_RAKNET_GUID,(NetPacket *)&type,0,UNASSIGNED_SYSTEM_ADDRESS,ID_USER_PACKET_SPECIFIC);
}

