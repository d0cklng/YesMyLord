#include "rakagent.h"
#include "jinassistthread.h"
#include "lordinnerdef.h"
#include "jinlogger.h"
#include "isinkforrakagent.h"

static const int kConnCoServerCheckTime = 10000;
static const int kCoServerReConnTime = 25000;  //失败重连时间
static const int kCoServerConnDoneTime = 30000;  //连接这么久后断开

RakAgent::RakAgent(ISinkForRakAgent* callback, unsigned area)
    : callback_(callback)
    , zoneArea_(area%CoServerScale)
    , isStart_(false)
    , netid_(0)
    , outTargetConnID_(0)
    , outTargetTheOnly_(0)
    , outTargetArea_(0)
    , outTargetState_(RA_IDLE)
    , timer_(0)
    , connCoServerTimerID_(0)
{
    memset(refBuf_internal,0,sizeof(int)*CoServerScale);
    memset(srvContact,0,sizeof(CoServerContact)*CoServerScale);

}

RakAgent::~RakAgent()
{
    this->stop();
}

bool RakAgent::start(const JinNetAddr &addr, const JinNetPort &port)
{
    if(isStart_)stop();
    JAssert(isStart_ == false);
    JAssert(rak_ == NULL);
    rak_ = INetEngine::GetInstance();
    if(!rak_) return false;
    netid_ = rak_->Init(this);

    unsigned short portBind = rak_->Bind(port.port(),addr.toAddr4(gDLogBufAddr2));
    if(portBind == 0)
    {
        JELog("bind failed at:%s|%hu",gDLogBufAddr2,port.port());
        goto start_end;
    }
    addr_ = addr;
    if(portBind != port.port())
    {
        JPLog("port set %hu but real bind %hu",port.port(),portBind);
        port_ = JinNetPort(portBind);
    }
    JPLog("Rak engine bind %s|%hu OK",gDLogBufAddr2,port.port());
    upnpc_.OpenPort(port_);

    rak_->AttachAsPunchthroughClient();
    rak_->AttachAsUDPProxyClient();

    if(timer_ == NULL) timer_ = JinMainThread::Get()->getTimer();
    connCoServerTimerID_ = timer_->start(kConnCoServerCheckTime,true,sHandleTimeout,this);

    isStart_ = true;
start_end:
    if(!isStart_){
        JELog("RakAgent start failed.");
        this->stop();
        return false;
    }
    return true;

}

void RakAgent::openFilter()
{
    JAssert(rak_);
    if(!rak_)return;
    rak_->OpenDatagramFilter();
}

bool RakAgent::connTarget(const NetID &id, unsigned area,
                          JinNetAddr &addr, JinNetPort &port)
{
    JAssert(area<CoServerScale);
    JPLog("connTarget>%s|%hu id=%"PRIu64"(r%u)",
          addr.toAddr(),port.port(),id,area);
    //不允许连接coServer
    for(int i=0;i<CoServerScale;i++)
    {
        if(srvContact[i].id == id){
            JELog("not allow conn coServer as target!id=%"PRIu64,id);
            return false;
        }
    }
    if(id == outTargetTheOnly_)
    {
        JAssert(outTargetState_>RA_IDLE);
        JELog("connTarget same with old id=%"PRIu64,id);
        return false;
    }
    if(outTargetState_>RA_IDLE)
    {
        this->closeTarget();
    }
    outTargetTheOnly_ = id;
    JAssert(outTargetState_==RA_IDLE);
    outTargetArea_ = area;

    if(srvContact[area].id == 0) //没有
    {
        JDLog("direct conn outTarget %s|%hu",addr.toAddr(),port.port());
        outTargetConnID_ = rak_->Connect(addr.toAddr(),port.port());
        outTargetState_ = RA_CONN;

        if(srvContact[area].cid == 0)
        {
            openCoServerConn(area);
        }
    }
    else
    {
        JDLog("direct connEx outTarget %s|%hu",addr.toAddr(),port.port());
        outTargetConnID_ = rak_->ConnectEx(CONN_METHOD_PUNCHTHROUGH,srvContact[area].id,outTargetTheOnly_);
        outTargetState_ = RA_CONNEX;
    }

    if(outTargetConnID_ == 0) //direct failed?
    {
        JELog("conn direct1 failed.");
        outTargetTheOnly_ = 0;
        outTargetState_ = RA_IDLE;
        callback_->OnOutwardConn(false,id,JinNetAddr(),JinNetPort());
        return false;
    }
    return true;
}

void RakAgent::closeTarget()
{
    JPLog("closeTarget> state=%d id="PRIu64" connid=%"PRIu64,
          outTargetState_,outTargetTheOnly_,outTargetConnID_);
    switch(outTargetState_)
    {
    case RA_CONN:
        JAssert((bool)outTargetConnID_);
        outTargetConnID_ = 0;
        break;
    case RA_LINK:
        rak_->Close(outTargetTheOnly_);
        break;
    }
    outTargetState_ = RA_IDLE;
    outTargetTheOnly_ = 0;
    JAssert(outTargetConnID_==0);
    outTargetConnID_ = 0;
}

void RakAgent::stop()
{
    if(!isStart_)return;
    if(connCoServerTimerID_) timer_->cancel(connCoServerTimerID_);
    for(int i=0;i<CoServerScale;i++)
    {
        closeCoServerConn(i);
    }
    if(outTargetState_ > RA_IDLE)
    {
        closeTarget();
    }
    //if(reconntid_) timer_->cancel(reconntid_);
    if(rak_){
        INetEngine::DestroyInstance(rak_);
        rak_ = NULL;
    }

    isStart_ = false;
}

bool RakAgent::send(NetPacket *packet, int DataLenInPack,
                    SendPriority priority, PackStyle packStyle,
                    char orderingChannel, NetID targetID,
                    bool boardcast, int head)
{
    JAssert(rak_);
    if(!rak_) return false;
    return rak_->Send(packet,DataLenInPack,priority,packStyle,orderingChannel,targetID,boardcast,head);
}

void RakAgent::close(NetID id)
{
    JDLog("close netid:%"PRIu64,id);
    JAssert(rak_);
    if(!rak_) return;
    if(id < NETWORKID_RESERVER)
    {
        JELog("id < NETWORKID_RESERVER");
        return;
    }
    if(outTargetTheOnly_ == id)
    {
        JELog("outTargetTheOnly_ == id");
        return;
    }
    for(int i=0;i<CoServerScale;i++)
    {
        if(srvContact[i].id == id)
        {
            JELog("srvContact[%d].id == id",i);
            return;
        }
    }
    rak_->Close(id);
}

int RakAgent::OnNetMsg(INetEngine *src, NetID id, sockaddr_in addr4, NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    JAssert("not modify yet.!!!"==0);
    JAssert(src == rak_);
    JinNetAddr addr = JinNetAddr::fromRawIP(addr4.sin_addr.s_addr);
    JinNetPort port = JinNetPort::fromRawPort(addr4.sin_port);
    JDLog("id=%"PRIu64",%s|%hu type=%hhu, LenInPack=%d",
          id,addr.toAddr(gDLogBufAddr),port.port(),
          packet->PacketType0,lenFromNetMsg);
    switch (packet->PacketType0)
    {
    case TYPE_APPLICATION_DATA:
        HandleApplicationData(id,addr,port,packet,lenFromNetMsg,head);
        break;
    case TYPE_CONNECTION_ACCEPT:
        HandleConnectionAccept(id,addr,port,packet,lenFromNetMsg);
        break;
    case TYPE_CONNECTION_FAILED: //只会主动连服务器,所以只考虑jinzeyucn服务器.
        HandleConnectionFailed(id,packet);
        break;
    case TYPE_CONNECTION_CLOSE:
        HandleConnectionClose(id,true);
        break;
    case TYPE_CONNECTION_LOST:
        HandleConnectionClose(id,false);
        break;
    case TYPE_CONNECTION_INCOMING:
        HandleConnectionIncoming(id,addr,port,packet,lenFromNetMsg);
    default:
        JDLog("drop unexpect msg type=%d",(int)packet->PacketType0);
        //DoClose(id);
        //TODO DoClose remote ;
    }
    return -1;
}

bool RakAgent::OnDatagram(char *netpack, int len, uint32_t naddr, NetPort nport)
{
    return callback_->OnDatagramFilter(netpack,len,naddr,nport);
}

void RakAgent::DoRecvFrom()
{
    return;
}

bool RakAgent::SendOut(const unsigned char *buffer, size_t len, const JinNetAddr &addr, const JinNetPort &port)
{
    if(!rak_ || len<=1) return false;
    return rak_->SendToRaw(addr.toAddr(),port.port(),(const char*)buffer,len);
}

uint16_t RakAgent::SockPort()
{
    return port_.port();
}

void RakAgent::HandleApplicationData(const NetID &id, JinNetAddr &addr, JinNetPort &port, NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    if(id == outTargetTheOnly_)
        callback_->OnOutwardRecv(id,addr,port,packet,lenFromNetMsg,head);
    else
        callback_->OnInwardRecv(id,addr,port,packet,lenFromNetMsg,head);
}

void RakAgent::HandleConnectionAccept(const NetID &id, JinNetAddr &addr, JinNetPort &port, NetEngineMsg *packet, const int lenFromNetMsg)
{
    //正常的连出只有outward
    NetEngineConnection *nc = (NetEngineConnection*)packet;
    if(outTargetConnID_ && outTargetConnID_ == nc->id)
    {
        JPLog("success conn to target:%s|%hu id=%"PRIu64" ,target=%"PRIu64,
              addr.toAddr(),port.port(),id,outTargetTheOnly_);
        //JAssert(outTargetTheOnly_==0);
        if(id != outTargetTheOnly_)
        {
            JELog("succ id=%"PRIu64" <> target=%"PRIu64,id,outTargetTheOnly_);
            HandleConnectionFailed(id,packet);
            return;
        }
        outTargetConnID_ = 0;
        outTargetTheOnly_ = id;
        outTargetState_ = RA_LINK;
        callback_->OnOutwardConn(true,id,addr,port);
    }
    else
    {
        handleCoServerConnOk(false,id,nc->id);
        //JELog("unexpect conn notify id=%"PRIu64" target:%s|%hu",
        //      id,addr.toAddr(),port.port());
    }
}

void RakAgent::HandleConnectionFailed(const NetID &id, NetEngineMsg *packet)
{
    //正常的连出只有outward
    NetEngineConnection *nc = (NetEngineConnection*)packet;
    if(outTargetConnID_ && outTargetConnID_ == nc->id)
    {
        outTargetConnID_ = 0;
        JPLog("fail conn to outTarget id=%"PRIu64,id);
        if(outTargetState_ == RA_CONN)
        {
            if(srvContact[outTargetArea_].id == 0)
            {
                if(srvContact[outTargetArea_].cid)
                {
                    outTargetState_ = RA_WAIT;
                    return;
                }
                //否则应该是尝试连coServer失败了.失败下滚.
            }
            else
            {
                JDLog("retry connEx outTarget when connFail id=%"PRIu64" coServer%d=%"PRIu64,
                      outTargetTheOnly_,outTargetArea_,srvContact[outTargetArea_].id);
                outTargetConnID_ = rak_->ConnectEx(CONN_METHOD_PUNCHTHROUGH,srvContact[outTargetArea_].id,outTargetTheOnly_);
                outTargetState_ = RA_CONNEX;
                if(outTargetConnID_) return;
                JELog("conn direct2 failed.");
            }
        }
        else if(outTargetState_ != RA_CONNEX)
        {
            JELog("outTarget conn failed with state=%d",outTargetState_);
            return;
        }
        NetID target = outTargetTheOnly_;
        JAssert(id == target);
        outTargetTheOnly_ = 0;
        outTargetState_ = RA_IDLE;
        callback_->OnOutwardConn(false,target,JinNetAddr(),JinNetPort());
    }
    else
    {
        handleCoServerConnFailed(nc->id);
    }
}

void RakAgent::HandleConnectionIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port, NetEngineMsg *packet, const int lenFromNetMsg)
{
    callback_->OnInwardIncoming(id,addr,port);
}

void RakAgent::HandleAlreadyConnected(const NetID &id, JinNetAddr &addr, JinNetPort &port, NetEngineMsg *packet, const int lenFromNetMsg)
{
    NetEngineConnection *nc = (NetEngineConnection*)packet;
    handleCoServerConnOk(true,id,nc->id);
}

void RakAgent::HandleConnectionClose(const NetID &id, bool bGrace)
{
    if(outTargetTheOnly_ == id)
    {
        outTargetTheOnly_ = 0;
        outTargetState_ = RA_IDLE;
        JAssert(outTargetConnID_ == 0);
        outTargetConnID_ = 0;
        callback_->OnOutwardConnClose(id,bGrace);
    }
    else
    {
        callback_->OnInwardConnClose(id,bGrace);
    }
}

void RakAgent::onCoServerConn(int idx, bool isSucc)
{
    JDLog("notify coServer conn idx=%d isSucc=%d", idx, isSucc);
    if(outTargetState_ == RA_WAIT && idx == outTargetArea_)
    {
        if(isSucc)
        {
            JDLog("retry connEx outTarget when coServerConn id=%"PRIu64" coServer%d=%"PRIu64,
                  outTargetTheOnly_,idx,srvContact[idx].id);
            JAssert(srvContact[idx].id);
            outTargetConnID_ = rak_->ConnectEx(CONN_METHOD_PUNCHTHROUGH,srvContact[idx].id,outTargetTheOnly_);
            outTargetState_ = RA_CONNEX;
            if(outTargetConnID_) return;
            JELog("conn direct3 failed.");
        }
        callback_->OnOutwardConn(false,outTargetTheOnly_,JinNetAddr(),JinNetPort());
        return;
    }
}

TIMEOUT_HANDLER_IMPLEMENT(RakAgent)  //TimerID id
{
    if(id == connCoServerTimerID_)
    {
        T64 tickNow = JinDateTime::tickCount();

        //负责重试重连自己area的服务器'
        CoServerContact &ct = srvContact[zoneArea_];
        if(ct.cid==NULL && ct.id == NULL &&
                tickNow - ct.actTime > kCoServerReConnTime)
        {
            JDLog("reconn zoneArea coServer now");
            openCoServerConn(zoneArea_);
        }

        //检查超时的服务器连接将其关闭
        for(int i=0;i<CoServerScale;i++)
        {
            if(i==zoneArea_)continue;
            if(srvContact[i].id != 0 &&
                    tickNow - srvContact[i].actTime > kCoServerConnDoneTime)
            {
                closeCoServerConn(i);
            }
        }

    }
}

void RakAgent::openCoServerConn(int idx)
{
    JDLog("open coServer r%d",idx);
    CoServerContact &ct = srvContact[idx];
    if(ct.cid)
    {
        JELog("already in conn prgress.");
        return;
    }
    if(ct.id)
    {
        JELog("already connected, id=%"PRIu64,ct.id);
        return;
    }
    JAssert(ct.ref == NULL);
    JAssert(ct.actTime == 0);
    ct.ref = NULL;
    //ct.actTime = 0;
    char coServerDomain[64];
    sprintf(coServerDomain,"r%02d."CoServerDomainBase,idx);
    JDLog("conn coServer:%s|%hu",coServerDomain,CoServerPortDefault);
    ct.cid = rak_->Connect(coServerDomain,CoServerPortDefault);
    ct.actTime = JinDateTime::tickCount();

}

void RakAgent::closeCoServerConn(int idx)
{
    CoServerContact &ct = srvContact[idx];
    if(ct.cid)
    {
        JAssert(ct.id == 0);
        ct.cid = 0;
    }
    if(ct.id != 0)
    {   //没有ref的代表ref是1未被引用
        if(ct.ref)
        {
            JAssert(*ct.ref>1);
            if(--*ct.ref ==  1)
            {
                *ct.ref = 0; //一定要置0 否则不嫩重用
            }
            ct.ref = NULL;
        }
        else
        {
            JDLog("close coserver conn,id=%"PRIu64,ct.id);
            rak_->Close(ct.id);
            ct.id = 0;
        }
    }
    ct.actTime = 0;
}

//返回真实影响的index.
void RakAgent::handleCoServerConnOk(bool isAlready, const NetID &id, const ConnID &cid)
{
    JAssert((bool)cid);
    int findAlready = -1;
    int *ref = NULL;
    int findIdx = -1;
    for(int i=0;i<CoServerScale;i++)
    {
        CoServerContact &sc = srvContact[i];
        JAssert(!sc.ref || *sc.ref>1);  //断定ref的值不能是1,要么没有 要有起点就是2
        if(sc.id == id)
        {
            findAlready = i;
            JAssert(ref == NULL || ref == sc.ref); //检查同id不同ref的情况.
            ref = sc.ref;
            if(findIdx >= 0)break;
        }
        if(sc.cid == cid)
        {
            findIdx = i;
            if(findAlready>=0)break;
        }
    }
    if(findIdx >= 0)
    {
        CoServerContact &sc = srvContact[findIdx];
        sc.cid = 0;
        sc.id = id;
        if(findAlready>=0)
        {
            if(ref == NULL)
            {
                for(int j=0;j<CoServerScale;j++)
                {
                    if(refBuf_internal[j]<=0)
                    {
                        ref = &refBuf_internal[j];
                        *ref = 1;
                        break;
                    }
                }
            }
            JAssert(ref);
            sc.ref = ref;
            ++ *ref;
        }
        sc.actTime = JinDateTime::tickCount();
        onCoServerConn(findIdx, true);
    }
    if(isAlready && findAlready<0)
    {
        JELog("already conn but not findAlready");
        //这个本该是server的id被其他用了??不可能吧.
    }
}

void RakAgent::handleCoServerConnFailed(const ConnID &cid)
{
    int findIdx = -1;
    for(int i=0;i<CoServerScale;i++)
    {
        CoServerContact &sc = srvContact[i];
        if(sc.cid == cid)
        {
            sc.cid = 0;
            sc.actTime = JinDateTime::tickCount();
            JAssert(sc.ref==NULL);
            findIdx = i;
            break;
        }
    }

    if(findIdx >= 0)
    {
        onCoServerConn(findIdx, false);
    }
    else
    {
        JELog("conn failed but not found in coServer.");
    }
}
