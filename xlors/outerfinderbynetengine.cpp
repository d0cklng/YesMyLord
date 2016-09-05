#include "outerfinderbynetengine.h"
#include "jinassert.h"
#include "jinlogger.h"
#include "jinassistthread.h"
#include "jindatatime.h"
#include "xlors_share.h"
#include "jinpackhelper.h"


OuterFinderByNetEngine::OuterFinderByNetEngine(ISinkForOuterFinder* cb, const JinString &selfIdentify)
    : IOuterFinder(cb)
    , finder_(NULL)
    , timer_(NULL)
    , timerid_(0)
    , netid_(NETID_INVALID)
    , jinzeyucn_(NETID_INVALID)
    , reconntid_(0)
    , selfIdent_(selfIdentify)
    , state_(WaitXlorsConn)
{
    targetidentify_[0] = '\0';
}

OuterFinderByNetEngine::~OuterFinderByNetEngine()
{

}

bool OuterFinderByNetEngine::Start(const JinNetAddr &addr)
{
    outside_ = addr;
    if(isStart_)Stop();
    JAssert(isStart_==false);
    JAssert(finder_==NULL);
    char buf[32];
    finder_ = INetEngine::GetInstance();
    if(!finder_) return false;
    netid_ = finder_->Init(this);

    if(outside_.rawIP() != 0)
    {  //如果没指定地址就不绑定，如此不用触发防火墙.
        unsigned short portBind = finder_->Bind(0,outside_.toAddr4(buf));
        JPLog("port:%hu bind.", portBind);
        if(portBind == 0)
        {
            INetEngine::DestroyInstance(finder_);
            finder_ = NULL;
            return false;
        }
    }
    connjzycn_ = finder_->Connect(kLorsDefaultAddr,kLorsDefaultPort);
    finder_->AttachAsPunchthroughClient();
    finder_->AttachAsUDPProxyClient();
    timer_ = JinMainThread::Get()->getTimer();
    timerid_ = timer_->start(3,true,sHandleTimeout,this);

    HostPort port = finder_->GetMyNetID();
    if(port > 0)
    {
        upnpc_.OpenPort(JinNetPort(port));
    }

    isStart_ = true;

    return true;
}

void OuterFinderByNetEngine::Stop()
{
    if(!isStart_)return;
    if(timerid_) timer_->cancel(timerid_);
    if(reconntid_) timer_->cancel(reconntid_);
    INetEngine::DestroyInstance(finder_);
    connjzycn_ = 0;
    isStart_ = false;
}

int OuterFinderByNetEngine::OnNetMsg(INetEngine *src, NetID id, sockaddr_in addr4, NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    JAssert(src == finder_);
    JinNetAddr addr = JinNetAddr::fromRawIP(addr4.sin_addr.s_addr);
    JinNetPort port = JinNetPort::fromRawPort(addr4.sin_port);
    JDLog("id=%"PRIu64",%s|%hu type=%hhu, LenInPack=%d",
          id,addr.toAddr(gDLogBufAddr),port.port(),
          packet->PacketType0,lenFromNetMsg);
    switch (packet->PacketType0)
    {
    case TYPE_APPLICATION_DATA:
    {
        if(id == jinzeyucn_)
        {
            OnXlorsRecv(packet,lenFromNetMsg);
        }
        else
        {
            callback_->OnRecv(id,addr,port,packet,lenFromNetMsg,head);
            //DoClose(id);
        }
    }
        break;
    case TYPE_CONNECTION_ACCEPT:
    {
        NetEngineConnection *nc = (NetEngineConnection*)packet;
        //JAssert(connjzycn_ == nc->id);
        if(connjzycn_ && connjzycn_ == nc->id)
        {
            connjzycn_ = 0;
            JPLog("success conn to %s",kLorsDefaultAddr);
            jinzeyucn_ = id;
            OnXlorsConn();
        }
        else if(conntarget_ && conntarget_ == nc->id)
        {
            conntarget_ = 0;
            JPLog("success conn to target.");
            targetid_ = id;
            JDLog("state => TargetOnWork");
            state_ = TargetOnWork;
            callback_->OnFind(id,addr,port);
        }
        else
        {
            JELog("expired conn notify.");
            finder_->Close(id);
        }
    }
        break;
    case TYPE_CONNECTION_FAILED: //只会主动连服务器,所以只考虑jinzeyucn服务器.
    {
        NetEngineConnection *nc = (NetEngineConnection*)packet;
        //JAssert(connjzycn_ == nc->id);
        if(connjzycn_ && connjzycn_ == nc->id)
        {
            connjzycn_ = 0;
            JAssert(reconntid_ == 0);
            JPLog("fail conn to %s",kLorsDefaultAddr);
            reconntid_ = timer_->start(kReconnJinzeyuTimeMore,false,sHandleTimeout,this);
        }
        else if(conntarget_ && conntarget_ == nc->id)
        {
            conntarget_ = 0;
            JPLog("fail conn to target.");
            //JDLog("state => TargetOnWork");
            //state_ = TargetOnWork;
            JinMainThread::notifyQuit();
        }
        else
        {
            JELog("expired conn fail notify.");
            //finder_->Close(id);
        }
    }
        break;
    case TYPE_CONNECTION_CLOSE:
    case TYPE_CONNECTION_LOST:
    {
        bool bGrace = (TYPE_CONNECTION_CLOSE==packet->PacketType0);
        if(id == jinzeyucn_)
        {
            JAssert(reconntid_ == 0);
            JPLog("conn %s from %s",(bGrace?"close":"lost"),kLorsDefaultAddr);
            jinzeyucn_ = NETID_INVALID;
            reconntid_ = timer_->start(kReconnJinzeyuTime,false,sHandleTimeout,this);
            if(state_ < Xlors_server_depend)
            {
                JDLog("state => WaitXlorsConn");
                state_ = WaitXlorsConn;
            }
        }
        else if(id == targetid_)
        {
            if(bGrace)
            {
                JDLog("conn closed.");
                callback_->OnClose(id, true);
                //JinMainThread::notifyQuit();
            }
            else
            {
                JDLog("state => back TargetConnecting");
                state_ = TargetConnecting;
                conntarget_ = finder_->ConnectEx(CONN_METHOD_PUNCHTHROUGH,jinzeyucn_,targetid_);
                callback_->OnClose(id, false);
            }
        }
        else
        {  //只有两种连出连接哦.
            JELog("expired conn lost. %"PRIu64, id);
        }
    }
        break;
    case TYPE_CONNECTION_INCOMING:
    {
        JDLog("income addr=%s|%hu id=%"PRIu64,addr.toAddr4(gDLogBufAddr),port.port(),id);
    }
        //no break;
    default:
        JDLog("drop client because of unexpect type. %d",(int)packet->PacketType0);
        //DoClose(id);
        //TODO DoClose remote ;
    }
    return -1;
}


void OuterFinderByNetEngine::sHandleTimeout(TimerID id, void *user, uint64_t user64)
{
    OuterFinderByNetEngine* lorb = (OuterFinderByNetEngine*)user;
    lorb->handleTimeout(id);
}

void OuterFinderByNetEngine::handleTimeout(TimerID id)
{
    //JAssert(id == timerid_);
    if(id == timerid_)
    {
        if(!finder_)return;
        while(finder_->ProcPacket(JinDateTime::tickCount())) { }
    }
    else if(id == reconntid_)
    {
        reconntid_ = 0;
        JAssert(connjzycn_ == 0);
        JDLog("reconn to %s",kLorsDefaultAddr);
        connjzycn_ = finder_->Connect(kLorsDefaultAddr,kLorsDefaultPort);
    }
}

bool OuterFinderByNetEngine::DoSend(NetPacket *pack, int DataLenInPack, NetID id, uint16_t tunnid)
{
    int headMixUp = tunnid%250+1;
    return finder_->Send(pack,DataLenInPack,SEND_NORMAL,PACK_TCPLIKE,0,id, false, headMixUp);
}

//bool OuterFinderByNetEngine::DoClose(const NetID &id)
//{ //主动close 不回调.
//    return finder_->Close(id);
//    //OnClose(id,false);
//}

void OuterFinderByNetEngine::Find(const char *identity)
{
    JDLog("Set identity find:%s",identity);
    size_t ilen = strlen(identity);
    JAssert(ilen < kIdentityLength);
    if(ilen >= kIdentityLength) return;
    strcpy(targetidentify_,identity);
    tryDoFinderWork();

}

const char *OuterFinderByNetEngine::CurrentTarget()
{
    return targetidentify_;
}

bool OuterFinderByNetEngine::OnXlorsConn()
{
    // 发送信息注册.
    JinPackJoiner joiner;
    joiner.setHeadID(kMainO2S,kAsstLogin);
    //joiner.pushString(kOuCert,);  //暂未发送,服务暂不验证
    //joiner.pushString(kOuPubKey,);
    joiner.pushString(kUkIdentity,selfIdent_.c_str());

    if(state_ == WaitXlorsConn)
    {
        JDLog("state => XlorsAlreadyConn");
        state_ = XlorsAlreadyConn;
        tryDoFinderWork(); //因为不一定要服务器认可,就可以开始找人.
    }
    JAssert(jinzeyucn_ != NETID_INVALID);
    return this->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, jinzeyucn_);

}

void OuterFinderByNetEngine::OnXlorsRecv(NetEngineMsg *packet, const int lenFromMainID)
{
    JinPackParser parser((const char*)packet,lenFromMainID+1,1);
    if(!parser.isValid() || parser.mainID()!=kMainS2O)
    {
        JELog("packet from server error.");
        return ;
    }

    switch(parser.asstID())
    {
    case kAsstRtnLogin:
    {
        //if(state_ != TargetConnecting)
        //{
        //    JDLog("state=%d not TargetConnecting",(int)state_);
        //    break;
        //}
        uint16_t ec = parser.get16u(kUkErrorCode);
        uint32_t rawip = parser.get32u(kUkIPv4);
        uint16_t port = parser.get16u(kUkPort);
        JDLog("My outer addr by jinzeyu.cn is %s|%hu",
              JinNetAddr::fromRawIP(rawip).toAddr4(gDLogBufAddr),port);
        if(ec != kEcSuccess)
        {
            JinString eMsg = parser.getStr(kUkMessage);
            JPLog("Login faild. ec=%hu eMsg=%s", ec, eMsg.c_str());

        }
        else
        {
            JPLog("Login success");
        }
    }
        break;
    case kAsstRtnFind:
    {
        if(state_ != TargetFinding)
        {
            JDLog("state=%d not targetfinding",(int)state_);
            break;
        }
        uint16_t ec = parser.get16u(kUkErrorCode);
        if(ec == kEcSuccess)
        {
            uint64_t netid = parser.get64u(kUkNetID);
            uint32_t ipv4 = parser.get32u(kUkIPv4);
            uint16_t port = parser.get16u(kUkPort);
            JPLog("Find %s success => netid:%"PRIu64" %s|%hu",targetidentify_,netid,
                  JinNetAddr::fromRawIP(ipv4).toAddr(gDLogBufAddr),port);

            if(netid == 0 || netid == NETID_INVALID)
            {
                JDLog("state => back XlorsAlreadyConn");
                state_ = XlorsAlreadyConn;

                JinMainThread::notifyQuit();
                break;
            }
            JDLog("state => TargetConnecting");
            state_ = TargetConnecting;

            conntarget_ = finder_->ConnectEx(CONN_METHOD_PUNCHTHROUGH,jinzeyucn_,netid);
        }
        else
        {
            JinString eMsg = parser.getStr(kUkMessage);
            JPLog("Find %s fail: %s",targetidentify_,eMsg.c_str());

            JDLog("state => back XlorsAlreadyConn");
            state_ = XlorsAlreadyConn;

            JinMainThread::notifyQuit();
        }

    }
        break;
    default:
        JELog("can't handle asstid:%hu",parser.asstID());
    }
}

void OuterFinderByNetEngine::tryDoFinderWork()
{
    JDLog("state_=%d ti[0]=%c",(int)state_,targetidentify_[0]);
    if(state_ == XlorsAlreadyConn && targetidentify_[0] != '\0')
    {
        // 发送查找.
        JinPackJoiner joiner;
        joiner.setHeadID(kMainO2S,kAsstFind);
        joiner.pushString(kUkIdentity,targetidentify_);

        JAssert(jinzeyucn_ != NETID_INVALID);
        this->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, jinzeyucn_);
        JDLog("state => TargetFinding");
        state_ = TargetFinding;
    }
}

IOuterFinder *IOuterFinder::CreateFinder(ISinkForOuterFinder *cb, const JinString &selfIdentify )
{
    return JNew(OuterFinderByNetEngine,cb,selfIdentify);
}

void IOuterFinder::DestroyFinder(IOuterFinder *of)
{
    JDelete(of);
}
