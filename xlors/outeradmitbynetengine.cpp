#include "outeradmitbynetengine.h"
#include "jinassert.h"
#include "jinlogger.h"
#include "jinassistthread.h"
#include "jindatatime.h"
#include "xlors_share.h"
#include "jinpackhelper.h"


OuterAdmitByNetEngine::OuterAdmitByNetEngine(ISinkForOuterAdmit* cb, const JinString &selfIdentify)
    : IOuterAdmit(cb)
    , admit_(NULL)
    , timer_(NULL)
    , timerid_(0)
    , netid_(NETID_INVALID)
    , jinzeyucn_(NETID_INVALID)
    , reconntid_(0)
    , selfIdent_(selfIdentify)
{

}

OuterAdmitByNetEngine::~OuterAdmitByNetEngine()
{

}

bool OuterAdmitByNetEngine::Start(const JinNetAddr &addr)
{
    outside_ = addr;
    if(isStart_)Stop();
    JAssert(isStart_==false);
    JAssert(admit_==NULL);
    char buf[32];
    admit_ = INetEngine::GetInstance();
    if(!admit_) return false;
    netid_ = admit_->Init(this);

    if(outside_.rawIP() != 0)
    {  //如果没指定地址就不绑定，如此不用触发防火墙.
        unsigned short portBind = admit_->Bind(0,outside_.toAddr4(buf));
        JPLog("port:%hu bind.", portBind);
        if(portBind == 0)
        {
            INetEngine::DestroyInstance(admit_);
            admit_ = NULL;
            return false;
        }
    }
    connjzycn_ = admit_->Connect(kLorsDefaultAddr,kLorsDefaultPort);

    admit_->AttachAsPunchthroughClient();
    admit_->AttachAsUDPProxyClient();

    timer_ = JinMainThread::Get()->getTimer();
    timerid_ = timer_->start(3,true,sHandleTimeout,this);

    isStart_ = true;

    return true;
}

void OuterAdmitByNetEngine::Stop()
{
    if(!isStart_)return;
    if(timerid_) timer_->cancel(timerid_);
    if(reconntid_) timer_->cancel(reconntid_);
    INetEngine::DestroyInstance(admit_);
    connjzycn_ = 0;
    isStart_ = false;
}

int OuterAdmitByNetEngine::OnNetMsg(INetEngine *src, NetID id, sockaddr_in addr4, NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    JAssert(src == admit_);
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
        else if(!callback_->OnRecv(id,addr,port,packet,lenFromNetMsg,head))
        {
            JDLog("drop client because of bad recv.");
            DoClose(id);
        }
    }
        break;
    case TYPE_ALREADY_CONNECTED:  //?? 可疑状态
    case TYPE_CONNECTION_ACCEPT: //只会主动连服务器,所以只考虑jinzeyucn服务器.
    {
        NetEngineConnection *nc = (NetEngineConnection*)packet;
        JAssert(connjzycn_ == nc->id);
        if(connjzycn_ && connjzycn_ == nc->id)
        {
            connjzycn_ = 0;
            JPLog("success conn to %s",kLorsDefaultAddr);
            jinzeyucn_ = id;
            OnXlorsConn();
        }
    }
        break;
    case TYPE_CONNECTION_FAILED: //只会主动连服务器,所以只考虑jinzeyucn服务器.
    {
        NetEngineConnection *nc = (NetEngineConnection*)packet;
        //JAssert(connjzycn_ == nc->id); 发现打孔连接会进到这里. nc-id为空正好与connjzycn_相等.
        if(connjzycn_ && connjzycn_ == nc->id)
        {
            connjzycn_ = 0;
            JAssert(reconntid_ == 0);
            JPLog("fail conn to %s",kLorsDefaultAddr);
            reconntid_ = timer_->start(kReconnJinzeyuTimeMore,false,sHandleTimeout,this);
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
        }
        else
        {
            callback_->OnClose(id,bGrace);
        }
    }
        break;
    case TYPE_CONNECTION_INCOMING:
    {
        if(!callback_->OnIncoming(id,addr,port))
        {
            JDLog("drop client because of id exist.");
            admit_->Close(id);
        }
    }
        break;
    default:
        JDLog("drop client because of unexpect type=%hhu.",packet->PacketType0);
        DoClose(id);
    }
    return -1;
}


void OuterAdmitByNetEngine::sHandleTimeout(TimerID id, void *user, uint64_t user64)
{
    OuterAdmitByNetEngine* lorb = (OuterAdmitByNetEngine*)user;
    lorb->handleTimeout(id);
}

void OuterAdmitByNetEngine::handleTimeout(TimerID id)
{
    //JAssert(id == timerid_);
    if(id == timerid_)
    {
        if(!admit_)return;
        while(admit_->ProcPacket(JinDateTime::tickCount())) { }
    }
    else if(id == reconntid_)
    {
        reconntid_ = 0;
        JAssert(connjzycn_ == 0);
        JDLog("reconn to %s",kLorsDefaultAddr);
        connjzycn_ = admit_->Connect(kLorsDefaultAddr,kLorsDefaultPort);
    }
}

bool OuterAdmitByNetEngine::DoSend(NetPacket *pack, int DataLenInPack, NetID id, uint16_t tunnid)
{
    int headMixUp = tunnid%250+1;
    return admit_->Send(pack,DataLenInPack,SEND_NORMAL,PACK_TCPLIKE,0,id, false, headMixUp);
}

bool OuterAdmitByNetEngine::DoClose(const NetID &id)
{ //主动close 不回调.
    return admit_->Close(id);
    //OnClose(id,false);
}

bool OuterAdmitByNetEngine::OnXlorsConn()
{
    // 发送信息注册.
    JinPackJoiner joiner;
    joiner.setHeadID(kMainO2S,kAsstLogin);
    //joiner.pushString(kOuCert,);  //暂未发送,服务暂不验证
    //joiner.pushString(kOuPubKey,);
    joiner.pushString(kUkIdentity,selfIdent_.c_str());

    JAssert(jinzeyucn_ != NETID_INVALID);
    return this->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, jinzeyucn_);

}

void OuterAdmitByNetEngine::OnXlorsRecv(NetEngineMsg *packet, const int lenFromMainID)
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
    default:
        JELog("can't handle asstid:%hu",parser.asstID());
    }
}

IOuterAdmit *IOuterAdmit::CreateAdmit(ISinkForOuterAdmit *cb, const JinString &selfIdentify)
{
    return JNew(OuterAdmitByNetEngine,cb,selfIdentify);
}

void IOuterAdmit::DestroyAdmit(IOuterAdmit *oa)
{
    JDelete(oa);
}
