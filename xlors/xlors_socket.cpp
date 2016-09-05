#include "xlors_socket.h"
#include "jinlogger.h"
#include "jinassert.h"
#include "jindnsparser.h"

const static int kRecvBuffLength = 4096;



XLorsSocket::XLorsSocket()
    : isConnOk_(false)
    , dnsParseDoing_(NULL)
    , tunnid_(0)
    , cbinfo_(NULL)
    , cb_(NULL)
#ifdef JDEBUG
    , OnlyOneRecvOnceFlag(false)
#endif
{
    rbuf_.alloc(kRecvBuffLength);
}

int XLorsSocket::doConnect(JinString target, JinNetPort port)
{
    JAssert(isConnOk_ == false);
    bool bIsDomain = false;
    const char* nameTar = target.c_str();
    for(int i=0;nameTar[i];i++)
    {
        if(nameTar[i]>57||nameTar[i]<46)	//if发现是域名
        {
            bIsDomain = true;
            break;
        }
    }
    if(!bIsDomain)
    {
        return this->connect(JinNetAddr(target.c_str()),port);
    }
    else
    {
        JAssert(dnsParseDoing_ == NULL);
        JinNetAddr ipAddr;
        dnsParseDoing_ = JinDNS::Get()->parse(target.c_str(),Av4,sOnDnsParsed,this,port.port(),ipAddr);
        if(!dnsParseDoing_)
        {
            OnDnsParsed(target.c_str(),ipAddr,port.port());
        }
    }
    return kTSucc;
}

int XLorsSocket::doDisconnect()
{
    if(dnsParseDoing_)
    {
        JAssert(!isSocket());
        JinDNS::Get()->cancel(dnsParseDoing_);
        dnsParseDoing_ = 0;
        return kTSucc;
    }
    else
    {
        return this->disconnect();
    }
}

void XLorsSocket::sOnDnsParsed(const char *domain, JinNetAddr addr, void *user, uint64_t user64)
{
    XLorsSocket* lorsock = (XLorsSocket*)user;
    lorsock->OnDnsParsed(domain,addr,user64);
}

void XLorsSocket::OnDnsParsed(const char *domain, JinNetAddr addr, uint64_t user64)
{
    JAssert(isConnOk_ == false);
    dnsParseDoing_ = NULL;
    if(addr.isValid())
    {
        if(kTSucc == this->connect(addr,JinNetPort((uint16_t)user64))) return;
    }
    cb_->OnDisconnect(this, cbinfo_, false);
}

XLorsSocket::XLorsSocket(HdSocket sock, const JinNetAddr &remoteAddr, const JinNetPort &remotePort)
    : JinTcpSocket(sock,remoteAddr,remotePort)
    , isConnOk_(true)
    , dnsParseDoing_(NULL)
    , cb_(NULL)
#ifdef JDEBUG
    , OnlyOneRecvOnceFlag(false)
#endif
{
    rbuf_.alloc(kRecvBuffLength);
    //this->doRecv();
}

XLorsSocket::~XLorsSocket()
{
    if(dnsParseDoing_)
    {
        JinDNS::Get()->cancel(dnsParseDoing_);
        dnsParseDoing_ = NULL;
    }
    if(isConnOk_)
    {
        this->close();
    }
    isConnOk_ = false;
}

int XLorsSocket::send(JinAsynBuffer &buffer, uint32_t byteToSend)
{
    JDLog("[debug]send to lorsock %u, tunnid[%hu] isconn=%d",byteToSend,this->tunnelID(),(int)isConnOk_);
    if(!isConnOk_)
    {
        cacheBeforeConn.cat(buffer.buffc(),byteToSend);
        return kTSucc;
    }
    else
    {
        return JinTcpSocket::send(buffer,0,byteToSend);
    }
}

void XLorsSocket::doRecv()
{
    JAssert(!OnlyOneRecvOnceFlag);
    JAssert(isConnOk_);
    if(rbuf_.buffc() == 0
            || kTSucc != this->recv(rbuf_,0,kRecvBuffLength))
    {
        JELog("occur error! @recv failed.tunnid[%hu]",this->tunnelID());
        this->close();
        isConnOk_ = false;
        cb_->OnDisconnect(this, cbinfo_, false);
    }
#ifdef JDEBUG
    OnlyOneRecvOnceFlag = true;
#endif
}

void XLorsSocket::OnConnect(bool success, const JinNetAddr &addr, const JinNetPort &port)
{
    JAssert(tunnid_!=0);
    if(success)
    {
        isConnOk_ = true;
        if(cacheBeforeConn.length())
        {
            JinAsynBuffer cachesend(cacheBeforeConn.length());
            memcpy(cachesend.buff(),cacheBeforeConn.c_str(),cacheBeforeConn.length());
            if(kTSucc != this->send(cachesend,cacheBeforeConn.length()))
            {//不预期这样，不做什么动作.
                JELog("occur error! @send on connect");
                //this->close();
                //isConnOk_ = false;
                //cb_->OnDisconnect(false);
            }
            cacheBeforeConn.clear();
        }
        //this->doRecv();
    }
    cb_->OnConnect(this, cbinfo_, success,addr,port);
}

void XLorsSocket::OnRecv(void *buf, uint32_t byteRecved)
{
    JAssert(tunnid_!=0);
#ifdef JDEBUG
    OnlyOneRecvOnceFlag = false;
#endif
    cb_->OnRecv(this, cbinfo_, (const char*)buf,byteRecved);
}

void XLorsSocket::OnDisconnect(bool byRemote)
{
    JAssert(tunnid_!=0);
    isConnOk_ = false;
    cb_->OnDisconnect(this, cbinfo_, byRemote);
}
