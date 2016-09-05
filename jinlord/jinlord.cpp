#include "jinlord.h"
#include "jinchart.h"
#include "xinward.h"
#include "xoutward.h"
#include "jinchart.h"
#include "rakagent.h"

JinLord::JinLord(const JinNetAddr &bindaddr, const JinNetPort &bindport,
                 const JinNetAddr &localAddr4realTarget, const JinString &selfIdentify)
    : isStart_(false)
    , bindAddr_(bindaddr)
    , bindPort_(bindport)
    , localAddr4realTarget_(localAddr4realTarget)
    , selfIdentity_(selfIdentify)
#ifdef JDEBUG
    , quitcheck_(0)
#endif
    , agent_(NULL)
    , chart_(NULL)
    , outward_(NULL)
    , inward_(NULL)
    , ipc_(NULL)
{

}

JinLord::~JinLord()
{
    this->stop();
}

bool JinLord::start()
{
    if(isStart_)stop();
    JinKey160 selfKey;
    uint8_t area;
    JAssert(isStart_==false);
    ipc_ = JNew(JinIPC,NAME_OF_JINLORD_IPC);
    if(!ipc_){
        JELog("JinIPC create failed");
        goto start_end;
    }
    if(!ipc_->startHost(PASSWORD_OF_JINLORD_IPC,PASSWORD_LENGTH_OF_JINLORD_IPC))
    {
        JELog("JinIPC host failed");
        goto start_end;
    }
    chart_ = JNew(JinChart);
    if(!chart_){
        JELog("create chart failed");
        goto start_end;
    }
    chart_->setSelf(JinKey160()); //总是以默认方式拿dht-id(从文件读,否则随机)
    selfKey = chart_->getSelf();
    if(selfIdentity_.length() == 0)
    {
        selfIdentity_ = selfKey.toString();
        JDLog("no set selfIdentity! using=%s",selfIdentity_.c_str());
    }
    area = selfKey.getBit4At(0);
    agent_ = JNew(RakAgent,this,area);
    if(!agent_) goto start_end;
    if(!agent_->start(bindAddr_,bindPort_))
        goto start_end;

    inward_ = JNew(XInward,localAddr4realTarget_,selfIdentity_);
    outward_ = JNew(XOutward,bindAddr_,bindPort_,selfIdentity_);
    if(inward_ == NULL || outward_ == NULL)
        goto start_end;

    agent_->openFilter();
    chart_->init(bindPort_,bindAddr_,1,agent_);

    isStart_ = true;
start_end:
    if(!isStart_){
        JELog("JinLord start failed.");
        this->stop();
        return false;
    }
    return true;
}

void JinLord::stop()
{
    if(!isStart_) return;
    if(agent_)
    {
        //agent_->stop();
        JDelete(agent_);
        agent_ = NULL;
    }
    if(inward_)
    {
        //inward_->stop();
        JDelete(inward_);
        inward_ = NULL;
    }
    if(outward_)
    {
        //outward_->stop();
        JDelete(outward_);
        outward_ = NULL;
    }
    if(chart_)
    {
        JDelete(chart_);
        chart_ = NULL;
    }
    if(ipc_)
    {
        //ipc_->stop();
        JDelete(ipc_);
        ipc_ = NULL;
    }
    isStart_ = false;
}

bool JinLord::OnDatagramFilter(char *netpack, int len, uint32_t naddr, uint16_t nport)
{
    //返回true raknet将继续处理包,否则就忽略了.
    // 分包依据是'd'打头认为是dht包,其他是raknet包.
    JAssert(len>0);
    if(netpack[0]=='d')
    {
        JinNetAddr addr;
        addr = naddr;
        chart_->OnRecvFrom(netpack,(uint32_t)len,addr,
                           JinNetPort::fromRawPort(nport));
        return false;
    }
    return true;
}

void JinLord::OnOutwardConn(bool isOK, const NetID &id, const JinNetAddr &addr, const JinNetPort &port)
{
    if(isOK) outward_->OnFind(id,addr,port);
    //TODO ...
}

void JinLord::OnOutwardConnClose(const NetID &id, bool bGrace)
{
    outward_->OnClose(id,bGrace);
}

void JinLord::OnOutwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                            NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    outward_->OnRecv(id,addr,port,packet,lenFromNetMsg,head);
}

void JinLord::OnInwardConnClose(const NetID &id, bool bGrace)
{
    inward_->OnClose(id,bGrace);
}

void JinLord::OnInwardIncoming(const NetID &id, const JinNetAddr &addr, const JinNetPort &port)
{
    inward_->OnIncoming(id,addr,port);
}

void JinLord::OnInwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                           NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    inward_->OnRecv(id,addr,port,packet,lenFromNetMsg,head);
}

bool JinLord::DoSend(NetPacket *pack, int DataLenInPack, NetID id, uint16_t tunnid)
{
    int headMixUp = tunnid%250+1;
    return agent_->send(pack,DataLenInPack,SEND_NORMAL,PACK_TCPLIKE,0,id, false, headMixUp);
}

void JinLord::DoClose(const NetID &id)
{
    agent_->close(id);
}

void JinLord::OnError(LordError err, bool fromOutward)
{
    JELog("LordError=%d fromOutward=%d",(int)err,fromOutward?1:0);
    if(fromOutward)
    {
        outward_->stop();
        //TODO ...
    }
}

void JinLord::OnIPCMsg(uint16_t id, void *buf, uint32_t len)
{
    JDLog("OnIPCMsg recv id=%hu, len=%u", id, len);
}

