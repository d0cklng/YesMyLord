#include "plowsocket.h"
#include "jinlogger.h"
#include "jinassert.h"
const static int kRecvBuffLength = 4096;

PlowInSocket::PlowInSocket(JinTcpSocket* out)
    : out_(out)
    , isConnOk_(false)
    , cb_(NULL)
{
    rbuf_.alloc(kRecvBuffLength);
}

PlowInSocket::~PlowInSocket()
{
    if(isConnOk_)
    {
        this->close();
    }
    isConnOk_ = false;
    out_ = NULL;
}

int PlowInSocket::send(JinAsynBuffer &buffer, uint32_t byteToSend)
{
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

void PlowInSocket::doRecv()
{
    if(rbuf_.buffc()==0 || kTSucc != this->recv(rbuf_,0,kRecvBuffLength))
    {
         cb_->OnConnAbort(out_,this,kParInnerRecv);
    }
}

void PlowInSocket::OnConnect(bool success, const JinNetAddr &addr, const JinNetPort &port)
{
    if(success)
    {
        isConnOk_ = true;
        if(cacheBeforeConn.length())
        {
            JinAsynBuffer cachesend(cacheBeforeConn.length());
            memcpy(cachesend.buff(),cacheBeforeConn.c_str(),cacheBeforeConn.length());
            if(kTSucc != this->send(cachesend,cacheBeforeConn.length()))
            {
                cb_->OnConnAbort(out_,this,kParInnerSend1);
            }
        }
        this->doRecv();
    }
    else
    {
        cb_->OnConnAbort(out_,this,kParInnerConn);
    }
}

void PlowInSocket::OnRecv(void *buf, uint32_t byteRecved)
{
    JinAsynBuffer recvbuf(byteRecved);
    memcpy(recvbuf.buff(),buf,byteRecved);
    int ret = kTSucc;
    if(kTSucc != (ret = out_->send(recvbuf,0,byteRecved)))
    {
        JELog("out onrecv buf send failed. error=%d",ret);
        cb_->OnConnAbort(out_,this,kParInnerSend);
    }
    this->doRecv();
}

void PlowInSocket::OnDisconnect(bool byRemote)
{
    JDLog("in disconn by %s",byRemote?"server":"self");
    isConnOk_ = false;
    cb_->OnConnAbort(out_,this,kParInnerDisconn);
}


//================================================

PlowOutSocket::PlowOutSocket(HdSocket sock, const JinNetAddr &remoteAddr, const JinNetPort &remotePort)
    : JinTcpSocket(sock,remoteAddr,remotePort)
    , in_(NULL)
    , cb_(NULL)
{
    rbuf_.alloc(kRecvBuffLength);
}

PlowOutSocket::~PlowOutSocket()
{
    in_ = NULL;
}

void PlowOutSocket::setInside(PlowInSocket *in)
{
    in_ = in;
}

void PlowOutSocket::doRecv()
{
    if(rbuf_.buffc()==0 || kTSucc != this->recv(rbuf_,0,kRecvBuffLength))
    {
         cb_->OnConnAbort(this,in_,kParOuterRecv);
    }
}


void PlowOutSocket::OnRecv(void *buf, uint32_t byteRecved)
{
    JinAsynBuffer recvbuf(byteRecved);
    memcpy(recvbuf.buff(),buf,byteRecved);
    int ret = kTSucc;
    if(kTSucc != (ret = in_->send(recvbuf,byteRecved)))
    {
        JELog("out onrecv buf send failed. error=%d",ret);
        cb_->OnConnAbort(this,in_,kParOuterSend);
    }
    this->doRecv();
}

void PlowOutSocket::OnDisconnect(bool byRemote)
{
    JDLog("out disconn by remote=%s",byRemote?"remote":"self");
    cb_->OnConnAbort(this,in_,kParOuterDisconn);
}


//================================================




PlowListenSocket::PlowListenSocket()
{

}

PlowListenSocket::~PlowListenSocket()
{
    handlePendDestroy();
    JinHashMap<PlowOutSocket*,PlowInSocket*>::iterator it = plowMap_.begin();
    while(it != plowMap_.end())
    {
        PlowOutSocket*& outer = it.first();
        PlowInSocket*& inner = it.second();
        inner->close();
        outer->close();
        pendDestroy_.push_back(inner);
        pendDestroy_.push_back(outer);
        it = plowMap_.erase(it);
    }
}

void PlowListenSocket::setAddr(JinNetAddr inside, JinNetPort srvport, JinNetAddr srvaddr)
{
    inside_ = inside;
    srvport_ = srvport;
    srvaddr_ = srvaddr;
}

void PlowListenSocket::OnAccept(HdSocket accepted, const JinNetAddr &remoteAddr, const JinNetPort &remotePort)
{
    handlePendDestroy();
    PlowOutSocket* po = new PlowOutSocket(accepted,remoteAddr,remotePort);
    PlowInSocket* pi = new PlowInSocket(po);
    int ret = kTSucc;
    if(kTSucc != (ret = pi->bind(inside_,0)))
    {
        JELog("Plow inside bind error=%d",ret);
        po->close();
        delete po;
        return;
    }
    pi->setCallback(this);
    if(kTSucc != (ret = pi->connect(srvaddr_,srvport_)))
    {
        JELog("Plow inside connect error=%d",ret);
        po->close();
        delete po;
        return;
    }
    if(kTSucc != (ret = this->accept()))
    {
        JELog("Plow accept error=%d",ret);
        return;
    }
    po->setCallback(this);
    po->setInside(pi);
    po->doRecv();
    if(!plowMap_.set(po,pi))
    {
        JELog("plowMap set failed!",ret);
    }
    JDLog("accept new out=%p in=%p",po,pi);
}

void PlowListenSocket::OnConnAbort(JinTcpSocket *outer, JinTcpSocket *inner, int reason)
{
    JDLog("abort out=%p in=%p reason=%d",outer,inner,reason);
    inner->close();
    outer->close();
    pendDestroy_.push_back(inner);
    pendDestroy_.push_back(outer);
    plowMap_.remove(dynamic_cast<PlowOutSocket*>(outer));
}

void PlowListenSocket::handlePendDestroy()
{
    JinLinkedList<JinTcpSocket*>::iterator it = pendDestroy_.begin();
    while(it != pendDestroy_.end())
    {
        JinTcpSocket* s = *it;
        delete s;
        ++it;
    }
    pendDestroy_.clear();
}
