#include "jinnetstartup.h"
#include "jintcpsocket.h"
#include "jinasyncengine.h"
#include "jinlogger.h"

#ifndef AE_USING_IOCP
#include <fcntl.h>
#include <netinet/in.h>
#endif

JinTcpSocket::JinTcpSocket()
    : lastError_(0)
    //, aop_(NULL)
    , accSock_(SOCKINV)
    , isBind_(false)
    , isEstablish_(false)
{
    hsocket_ = SOCKINV;
}

JinTcpSocket::JinTcpSocket(HdSocket sock, const JinNetAddr& remoteAddr, const JinNetPort& remotePort,
                           const JinNetAddr& localAddr, const JinNetPort& localPort)
    : lastError_(0)
    //, aop_(NULL)
    , accSock_(SOCKINV)
    , isBind_(true)
    , isEstablish_(true)
{
    hsocket_ = sock;
    localAddr_ = localAddr; //可能未指定，也不获取,
    localPort_ = localPort; //如果有必要再获取时再获取.
    remoteAddr_ = remoteAddr;
    remotePort_ = remotePort;
    aop_ = AE::Get()->attach(this);
    if(aop_==NULL){
        this->close();
    }
}

JinTcpSocket::~JinTcpSocket()
{
    if(isSocket()){
        this->close();
    }
    JAssert(aop_ == NULL);
}

int JinTcpSocket::socket()
{
    JAssert(hsocket_ == SOCKINV);
#ifdef JWIN
    hsocket_ = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);
    if(hsocket_ == INVALID_SOCKET)
    {
        hsocket_ = SOCKINV;
        lastError_ = WSAGetLastError();
        return MAKE_EC(kTSocketError,lastError_);
    }
#else
    hsocket_ = ::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
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

int JinTcpSocket::bind(const JinNetAddr& addr, const JinNetPort& port)
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
    if( ret == -1) //if( ret == SOCKET_ERROR )
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTBindError,lastError_);
    }
    //如果已设置，则不用再获取.
    if(addr.rawIP()==0 || port.rawPort()==0)
    {
        socklen_t len = sizeof(rawAddr);
        if(-1 == getsockname(hsocket_,(struct sockaddr *)&rawAddr,&len))
        {
            JELog("getsockname after bind, e=%d",GetErrorCode(true));
        }
        localAddr_ = JinNetAddr::fromRawIP(rawAddr.sin_addr.s_addr);
        localPort_ = JinNetPort::fromRawPort(rawAddr.sin_port);
    }
    else
    {
        localAddr_ = addr;
        localPort_ = port;
    }
    isBind_ = true;
    JDLog("tcp sock bind @%s,%d",localAddr_.toAddr(gDLogBufAddr),(int)localPort_.port());
    return kTSucc;
}

int JinTcpSocket::listen(int backlog)
{
    JAssert(isSocket());
    int ret = ::listen(hsocket_,backlog);
    if( ret == -1)
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTBindError,lastError_);
    }

    return kTSucc;
}

int JinTcpSocket::connect(const JinNetAddr& addr, const JinNetPort& port)
{
    if(!isSocket() && lastError_==0)
    {
        int ret = this->socket();
        if(ret != kTSucc) return ret;
#ifdef JWIN  //MS ConnectEx 必须先bind
        if(!isBind_)
        {
            ret = this->bind("0.0.0.0", 0);
            if(ret != kTSucc) return ret;
        }
#endif
    }
    JAssert(isSocket());

    if(!AE::Get()->postConnect(aop_,addr.rawIP(),port.rawPort()))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTConnectError,lastError_);
    }
    return kTSucc;
}

int JinTcpSocket::accept()  //TODO 如果accept过程，对象销毁， hAccept泄漏
{
    JAssert(isSocket());
#ifdef AE_USING_IOCP
    HdSocket hAccept = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);
    if(hAccept == INVALID_SOCKET)
    {
        lastError_ = WSAGetLastError();
        return MAKE_EC(kTSocketAccept,lastError_);
    }
    if(!AE::Get()->postAccept(aop_, hAccept))
    {
        closesocket(hAccept);
        lastError_ = WSAGetLastError();
        return MAKE_EC(kTAcceptError,lastError_);
    }
    accSock_ = hAccept;
#else
    if(!AE::Get()->postAccept(aop_))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTAcceptError,lastError_);
    }
#endif
    return kTSucc;
}

int JinTcpSocket::send(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend)
{
    JAssert(isSocket());
    if(!AE::Get()->postSend(aop_,buffer,bufPos,byteToSend))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTSendError,lastError_);
    }
    return kTSucc;
}

int JinTcpSocket::recv(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv)
{
    JAssert(isSocket());
    if(!AE::Get()->postRecv(aop_,buffer,bufPos,byteMaxRecv))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTRecvError,lastError_);
    }
    return kTSucc;
}

int JinTcpSocket::disconnect()
{
    JAssert(isSocket());
    if(!AE::Get()->postDisconnect(aop_))
    {
        lastError_ = GetErrorCode(true);
        return MAKE_EC(kTDisconnectError,lastError_);
    }
    return kTSucc;

}

void JinTcpSocket::close()
{
    //必须先detach，假如先closesocket把sock置SOCKINV那么
    //detach时通过aop拿不到socket
    JDLog("TcpSocket close, obj=%"PRIu64,(unsigned long long)handle_);
    if(aop_){
        AE::Get()->detach(aop_);
        aop_ = NULL;
    }

    if(accSock_ != SOCKINV)
    {
        closesocket(accSock_);
        accSock_ = SOCKINV;
    }
    isBind_ = false;
    isEstablish_ = false;

    if(hsocket_!=SOCKINV){
        closesocket(hsocket_);
        hsocket_ = SOCKINV;
    }
}

bool JinTcpSocket::getLocalAddr(JinNetAddr &addr, JinNetPort &port)
{
    JAssert(isSocket());
    if(!isSocket())return false;
    if(!localAddr_.isValid() || localPort_.port()==0)
    {
        sockaddr_in rawAddr;
        rawAddr.sin_family = AF_INET;
        socklen_t alen = sizeof(rawAddr);
        if(-1 == getsockname(hsocket_,(struct sockaddr *)&rawAddr,&alen))
        {
            JELog("tcp getLocalAddr, e=%d",GetErrorCode(true));
            return false;
        }
        localAddr_ = JinNetAddr::fromRawIP(rawAddr.sin_addr.s_addr);
        localPort_ = JinNetPort::fromRawPort(rawAddr.sin_port);

    }
    addr = localAddr_;
    port = localPort_;
    return true;
}

bool JinTcpSocket::getRemoteAddr(JinNetAddr &addr, JinNetPort &port)
{
    if(!isEstablish_)return false;
    addr = remoteAddr_;
    port = remotePort_;
    return true;
}

void JinTcpSocket::OnMainPull(bool isSuccess, HdObject handle, AsynOpType type,
                              void *buf, uint64_t opSize, uint64_t tranSize,
                              void *user, uint64_t user64)
{
    switch(type)
    {
    case kSockAccept:
    {
        JAssert(handle == this->handle_);
        HdSocket accSock = (HdSocket)(long)user;
        JinNetAddr accAddr;
        JinNetPort accPort;
        if(opSize)
        {
#ifdef AE_USING_IOCP
            int llen,rlen;
            JAssert(accSock_ == accSock);
            LPSOCKADDR pLocalSockaddr;
            LPSOCKADDR pRemoteSockaddr;
            //const DWORD addressLength = sizeof(SOCKADDR_IN) + 16;
            //GetAcceptExSockaddrs(buf,(DWORD)opSize,addressLength,addressLength,

            //accept sock继承listen sock的属性.
            //继承很重要啊,不然shutdown失败对端没反应..据说获取socket地址也会失败.
            //(shutdown10057未连接或没地址).
            if(SOCKET_ERROR == setsockopt( accSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                           (char *)&this->hsocket_,sizeof(SOCKET) ))
            {
                JELog("error SO_UPDATE_ACCEPT_CONTEXT %d",(int) WSAGetLastError());
                //g_dbg.ReleaseMessageA("Error : setsockopt Failed. Code : %u",WSAGetLastError());
            }

            winGetAcceptExSockaddrs(this->hsocket_,buf,0,(DWORD)opSize,(DWORD)opSize,
                                    &pLocalSockaddr,&llen,&pRemoteSockaddr,&rlen);

            if(pRemoteSockaddr && pLocalSockaddr)
            {
                accAddr = JinNetAddr::fromRawIP(((LPSOCKADDR_IN)pRemoteSockaddr)->sin_addr.s_addr);
                accPort = JinNetPort::fromRawPort(((LPSOCKADDR_IN)pRemoteSockaddr)->sin_port);
            }
#else
            int flags = fcntl(accSock,F_GETFL,0);
            fcntl(accSock, F_SETFL, flags | O_NONBLOCK);
            accAddr = JinNetAddr::fromRawIP(  ((sockaddr_in*)buf)->sin_addr.s_addr);
            accPort = JinNetPort::fromRawPort(((sockaddr_in*)buf)->sin_port);
#endif
        }
        //socket for acception is never (isEstablish_==true)
        accSock_ = SOCKINV;
        this->OnAccept(accSock,accAddr,accPort);
        return;
    }
    case kSockConnect:
    {
        //JAssert(isSuccess == (tranSize!=opSize));
        JAssert(handle == this->handle_);
#ifdef JWIN
        if(isSuccess)
        {
            if(-1 == setsockopt( this->hsocket_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                                           NULL, 0 ))
            {
                JAssert3(false,"setsockopt for connected sock failed, sock",&handle);
            }
        }
#endif

        remoteAddr_ = JinNetAddr::fromRawIP((uint32_t)user64);
        remotePort_ = JinNetPort::fromRawPort((uint16_t)(uintptr_t)user);
        isEstablish_ = true;

        this->OnConnect(isSuccess,remoteAddr_,remotePort_);

        return;
    }
    case kSockSend:
    {
        JAssert(handle == this->handle_);
        JAssert(isSuccess == (tranSize==opSize));
        this->OnSend(buf,(uint32_t)opSize,(uint32_t)tranSize);
        return;
    }
    case kSockRecv:
    {
        if(tranSize == 0)
        {   //实测发现，如果对方Disconnect，底层IO为TRUE，transfer-size为0
            //如果对方不Disconnect直接close句柄，底层IO为FALSE，transfer-size为0
            //到此处已经区分不出来了.
            //+
            this->OnDisconnect(true);
            return;
        }
        JAssert(handle == this->handle_);
        JAssert(isSuccess == (tranSize==opSize));
        this->OnRecv(buf,(uint32_t)tranSize);
        return;
    }
    case kSockDisconnect:
        isEstablish_ = false;
        this->close();
        this->OnDisconnect(false);
        return;
    default:
        break;
    }


    JAssert4(false,"Unexpect AsynOpType=",type);
}


