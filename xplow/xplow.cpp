#include "xplow.h"
#include "jinassert.h"
#include "plowsocket.h"

XPlow::XPlow(JinNetAddr outside, JinNetPort outport,
             JinNetAddr inside, JinNetPort srvport,
             JinNetAddr srvaddr)
    : listenSock_(NULL)
    , isStart_(false)
{
    outside_ = outside;
    outport_ = outport;
    inside_ = inside;
    srvport_ = srvport;
    srvaddr_ = srvaddr;
}

XPlow::~XPlow()
{
    stop();
}

bool XPlow::start()
{
    if(isStart_)stop();
    JAssert(isStart_==false);
    JAssert(listenSock_==NULL);
    int ret = kTSucc;
    listenSock_ = new PlowListenSocket();
    if(!listenSock_)return false;

    listenSock_->setAddr(inside_,srvport_,srvaddr_);

    ret = listenSock_->bind(outside_,outport_);
    if(ret != kTSucc){
        delete listenSock_;
        return false;
    }

    ret = listenSock_->listen();
    if(ret != kTSucc){
        delete listenSock_;
        return false;
    }

    ret = listenSock_->accept();
    if(ret != kTSucc){
        delete listenSock_;
        return false;
    }

    isStart_ = true;

    return true;
}

void XPlow::stop()
{
    if(!isStart_)return;

    listenSock_->close();
    isStart_ = false;

    delete listenSock_;
    listenSock_ = NULL;
}

