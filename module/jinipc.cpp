#include "jinipc.h"
#include "jinsharememory.h"
#include "jinbuffer.h"

struct JinIPCHead
{
    uint32_t allSize;  //全部的长度.包括allSize自己占据的长度.
    uint16_t simplePswCheck;  //可以简单验证所持有的psw是否正确.
    uint16_t udpPort;  //host监听端口.
};

JinIPC::JinIPC(const char* name)
    : sm_(NULL)
    , isHost_(true)
    , callback_(NULL)
{
    name_ = name;
    recvBuf_.alloc(4096);
}

JinIPC::~JinIPC()
{
    stop();
}

bool JinIPC::startHost(const char *psw, uint32_t len)
{
    JAssert(!isSocket());
    JAssert(sm_==NULL);
    JAssert(len>0 && len<255);
    this->stop();
    if(kTSucc != this->bind(JinNetAddr("127.0.0.1"),JinNetPort())){
        stop();
        return false;
    }
    JinNetAddr addr;
    if(!this->getLocalAddr(addr,selfPort_)){
        stop();
        return false;
    }
    printf("JinIPC host UDP:%hu",selfPort_.port());
    psw_ = JinString(psw,len);

    sm_ = JNew(JinShareMemory,name_.c_str());
    if(!sm_){
        stop();
        return false;
    }
    if(!sm_->open(sizeof(JinIPCHead), kMemoryCreate, kAccessReadWrite)){
        stop();
        return false;
    }
    JinIPCHead* head = (JinIPCHead*)sm_->buff();
    head->allSize = sizeof(JinIPCHead);
    head->simplePswCheck = simplePswToU16(psw,len);
    head->udpPort = selfPort_.port();
    isHost_ = true;
    this->recvFrom(recvBuf_,0,4096);
    return true;
}

bool JinIPC::startJoin(const char *psw, uint32_t len)
{
    JAssert(!isSocket());
    JAssert(sm_==NULL);
    JAssert(len>0 && len<255);
    this->stop();
    if(kTSucc != this->bind(JinNetAddr("127.0.0.1"),JinNetPort())){
        stop();
        return false;
    }
    JinNetAddr addr;
    if(!this->getLocalAddr(addr,selfPort_)){
        stop();
        return false;
    }
    printf("JinIPC start UDP:%hu",selfPort_.port());

    sm_ = JNew(JinShareMemory,name_.c_str());
    if(!sm_){
        stop();
        return false;
    }
    if(!sm_->open(sizeof(JinIPCHead), kMemoryOpen, kAccessRead)){
        stop();
        return false;
    }
    JinIPCHead* head = (JinIPCHead*)sm_->buff();
    if(sizeof(JinIPCHead)>head->allSize ||
            head->simplePswCheck != simplePswToU16(psw,len) ||
            head->udpPort == 0){
        stop();
        return false;
    }
    psw_ = JinString(psw,len);
    targetPort_ = JinNetPort(head->udpPort);
    isHost_ = false;
    JDelete(sm_);
    sm_ = NULL;
    this->recvFrom(recvBuf_,0,4096);
    return true;
}

bool JinIPC::sendIPCMsg(const void *buf, uint32_t len)
{
    if(targetPort_.port()==0)return false;
    return sendIPCMsg(targetPort_.port(),buf,len);
}

bool JinIPC::sendIPCMsg(uint16_t id, const void *buf, uint32_t len)
{
    JinAsynBuffer buffer(len);
    if(buffer.ref()==0) return false;
    memcpy(buffer.buff(),buf,len);
    packetDoBlur(buffer.buff(),len);
    return (kTSucc ==
            sendTo(buffer,0,len,
                   JinNetAddr("127.0.0.1"),
                   JinNetPort(id)));
}

void JinIPC::stop()
{
    if(isSocket())
    {
        this->close();
    }
    if(sm_)
    {
        JDelete(sm_);
        sm_ = NULL;
    }
    //name_.clear();
    psw_.clear();
}

//void JinIPC::OnSendTo(void *buf, uint32_t byteToSend, uint32_t byteSended, const JinNetAddr &addr, const JinNetPort &port)
//{

//}

void JinIPC::OnRecvFrom(void *buf, uint32_t byteRecved, const JinNetAddr &addr, const JinNetPort &port)
{
    UNUSED(addr);
    if(callback_)
    {
        packetDoBlur(buf,byteRecved);
        if(isHost_) targetPort_ = port;
        callback_->OnIPCMsg(port.port(),buf,byteRecved);
        if(isHost_) targetPort_ = 0;
    }
    this->recvFrom(recvBuf_,0,4096);
}

void JinIPC::packetDoBlur(void *buf, uint32_t len)
{
    if(psw_.length() == 0)return;
    uint8_t *p1 = (uint8_t*)buf;
    uint8_t *p2 = (uint8_t*)psw_.unsafe_str();
    while(len>0)
    {
        if(p2 - (uint8_t*)psw_.unsafe_str()>=(int)psw_.length())
            p2 = (uint8_t*)psw_.unsafe_str();
        *p1 = *p1 ^ *p2;
        ++p1; ++p2; --len;
    }
}

//void JinIPC::packetRestore(void *buf, uint32_t len)
//{
//    packetDoBlur(buf,len);
//}

uint16_t JinIPC::simplePswToU16(const char *psw, uint32_t len)
{
    uint16_t rtn = 0x3838;
    for(uint32_t i=0;i<(len+1)/2;i++)
    {
        rtn += (psw[i]<8 + psw[i+1]);
    }
    return rtn;

}
