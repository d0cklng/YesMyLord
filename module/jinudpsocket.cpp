#include "jinnetstartup.h"
#include "jinudpsocket.h"
#include "jintcpsocket.h"
#include "jinasyncengine.h"
#include "jinlogger.h"
//#include ""

JinUdpSocket::JinUdpSocket()
    : lastError_(0)
    //, aop_(NULL)
{
    hsocket_ = SOCKINV;
}

JinUdpSocket::~JinUdpSocket()
{
    if(isSocket()){
        this->close();
    }
    JAssert(aop_ == NULL);
}

int JinUdpSocket::bind(const JinNetAddr &addr, const JinNetPort &port)
{
    if(!isSocket() && lastError_==0)
    {
        int ret = this->socket();
        if(ret != kTSucc) return ret;
    }
    JAssert(isSocket());

    sockaddr_in rawAddr;
    rawAddr.sin_family = AF_INET;
    rawAddr.sin_addr.s_addr = addr.rawIP();//htonl(m_BindAddress);
    rawAddr.sin_port = port.rawPort();//m_ListenPort
    int ret = ::bind(hsocket_,(sockaddr *)&rawAddr,sizeof(rawAddr));
    if( ret == -1 )
    {
#ifdef JWIN
        lastError_ = WSAGetLastError();
#else
        lastError_ = errno;
#endif
        return MAKE_EC(kTBindError,lastError_);
    }

    isBind_ = true;
    return kTSucc;
}

int JinUdpSocket::sendTo(JinAsynBuffer &buffer, const uint32_t bufPos, uint32_t byteToSend,
                          const JinNetAddr& addr, const JinNetPort& port)
{
    if(!isSocket() && lastError_==0)
    {  //udp允许不bind直接sendto发送，所以此处代办socket()调用.
        int ret = this->socket();
        if(ret != kTSucc) return ret;
    }
    JAssert(isSocket());

    if(!AE::Get()->postSendTo(aop_,buffer,bufPos,byteToSend,addr.rawIP(),port.rawPort()))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTSendError,lastError_);
    }
    return kTSucc;
}

int JinUdpSocket::recvFrom(JinAsynBuffer &buffer, const uint32_t bufPos, uint32_t byteMaxRecv)
{
    JAssert(isSocket());
    if(!AE::Get()->postRecvFrom(aop_,buffer,bufPos,byteMaxRecv))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTRecvError,lastError_);
    }
    return kTSucc;
}

void JinUdpSocket::close()
{
    isBind_ = false;

    if(aop_){
        AE::Get()->detach(aop_);
        aop_ = NULL;
    }

    if(hsocket_!=SOCKINV){
        closesocket(hsocket_);
        hsocket_ = SOCKINV;
    }
}

bool JinUdpSocket::getLocalAddr(JinNetAddr &addr, JinNetPort &port)
{
    JAssert(isSocket());
    if(!isSocket())return false;
    sockaddr_in rawAddr;
    rawAddr.sin_family = AF_INET;
    socklen_t alen = sizeof(rawAddr);
    if(-1 == getsockname(hsocket_,(struct sockaddr *)&rawAddr,&alen))
    {
        JELog("udp getLocalAddr, e=%d",GetErrorCode(true));
        return false;
    }
    addr = JinNetAddr::fromRawIP(rawAddr.sin_addr.s_addr);
    port = JinNetPort::fromRawPort(rawAddr.sin_port);
    return true;
}

int JinUdpSocket::socket()
{
    JAssert(hsocket_ == SOCKINV);
#ifdef JWIN
    hsocket_ = WSASocket(AF_INET,SOCK_DGRAM,IPPROTO_UDP,NULL,0,WSA_FLAG_OVERLAPPED);
    if(hsocket_ == INVALID_SOCKET)
    {
        hsocket_ = SOCKINV;
        lastError_ = WSAGetLastError();
        return MAKE_EC(kTSocketError,lastError_);
    }
    //http://support.microsoft.com/kb/263823
    //使用WSAIoctl设置UDP socket的工作模式，让其忽略这个错误.
    BOOL bNewBehavior = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(hsocket_, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);

    BOOL bOptVal = FALSE;  //想解决10052问题,但是无效,注释掉.
    int bOptLen = sizeof (BOOL);
    setsockopt(hsocket_, SOL_SOCKET, SO_KEEPALIVE, (char *) &bOptVal, bOptLen);
#else
    hsocket_ = ::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(hsocket_ <= 0)
    {
        hsocket_ = SOCKINV;
        lastError_ = errno;
        return MAKE_EC(kTSocketError,lastError_);
    }
    //set socket nonblock
    int flags;
    flags = fcntl(hsocket_, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(hsocket_, F_SETFL, flags);

#endif
    aop_ = AE::Get()->attach(this);
    if(aop_==NULL){
        this->close();
        return kTAsynInit;
    }

    return kTSucc;
}


void JinUdpSocket::OnMainPull(bool isSuccess, HdObject handle, AsynOpType type,
                              void *buf, uint64_t opSize, uint64_t tranSize,
                              void *user, uint64_t user64)
{
    switch(type)
    {

    case kSockSendTo:
    {
        JAssert(handle == this->handle_);
        JAssert(isSuccess == (tranSize==opSize));
        this->OnSendTo(buf,(uint32_t)opSize,(uint32_t)tranSize,
                       JinNetAddr::fromRawIP((uint32_t)user64),
                       JinNetPort::fromRawPort((uint16_t)(uintptr_t)user));
        return;
    }
    case kSockRecvFrom:
    {
        JAssert(handle == this->handle_);
        JAssert(isSuccess == (tranSize==opSize));
        size_t iByte = sizeof(sockaddr);
        sockaddr_in* srcAddr = (sockaddr_in*)((char*)buf+opSize-iByte);
        this->OnRecvFrom(buf,(uint32_t)tranSize,
                         JinNetAddr::fromRawIP(srcAddr->sin_addr.s_addr),
                         JinNetPort::fromRawPort(srcAddr->sin_port));
        return;
    }
    default:
        break;
    }


    JAssert4(false,"Unexpect AsynOpType=",type);
}

