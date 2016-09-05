#include "jinassert.h"
#include "jinlogger.h"
#include "jinnetaddress.h"
#include "jinassistthread.h"
#include "jinhashmap.h"
struct ClientMoreInfo;
#ifndef ClientInfoMoreType
typedef ClientMoreInfo *InfoMoreType;
#define ClientInfoMoreType
#endif
#include "clientinfoallocator.h"
#include "jinfiledir.h"
#include "jinfile.h"
#include "xlors_share.h"
#include "jinpackhelper.h"
#include "jincompress.h"
#include "xlorb.h"
#define KB *1024
#define MB *1024*1024
typedef JinHashMap<uint32_t, XLorsSocket*> TunnSockMap;  //tunnel id => socket.
typedef JinHashMap<XLorsSocket*, int> SockPendMap;
static const uint64_t kFlowControlMaxCache = 1 MB;
static const uint64_t kFlowControlAckNoDelay = 256 KB;
static const uint16_t kDefaultMaxConnPipe = 100;
static const int kFlowControlMinAckInterval = 1200;
//static const int kCompressEffectLen = 100;
//static const int kCompressEffectPercent = 90;

struct ClientMoreInfo
{
    //XLorb *lorb;  //回调时作为user可以复用.
    uint64_t fcAcked;   //FlowControl 对方收到且确认的
    uint64_t fcSended;  //FlowControl 已发送到
    TimerID ackCountDownTimer;  //控制最快ack时间间隔,同时确保没数据后也要把最后的数据ack掉
    bool ackAlreadyCoolDown;   //如果true,表示将马上ReplyAck,否则表示不急于ack
    uint64_t fcWeAcked;
    uint64_t fcWeRecv;
    SockPendMap *pendDoRecv;  //流控机制在拥塞时不执行recv,在此排队
    TunnSockMap *tunnSock;  //分配的tunnid对应的socket
    //JinString *cached;
    JinCompress *compressor;
};

struct XLorsSet
{
    uint16_t maxConnPipe;
    uint64_t flowCtrlCache;
};



XLorb::XLorb(const JinNetAddr &inside,const JinNetAddr &outside,
             const JinString &selfIdentify)
    : outerAdmit_(NULL)
    , selfIdentity_(selfIdentify)
    , isStart_(false)
    , quitcheck_(0)
{
    outside_ = outside;
    inside_ = inside;
    lorsSet = (XLorsSet*)JMalloc(sizeof(XLorsSet));
    lorsSet->maxConnPipe = kDefaultMaxConnPipe;
    lorsSet->flowCtrlCache = kFlowControlMaxCache;

}

XLorb::~XLorb()
{
    stop();
    JFree(lorsSet);
}

bool XLorb::start()
{
    if(isStart_)stop();
    JAssert(isStart_==false);
    JAssert(outerAdmit_==NULL);

    outerAdmit_ = IOuterAdmit::CreateAdmit(this,selfIdentity_);
    if(!outerAdmit_) return false;
    if(!outerAdmit_->Start(outside_))
    {
        IOuterAdmit::DestroyAdmit(outerAdmit_);
        outerAdmit_ = NULL;
    }
    quitcheck_ = JinMainThread::Get()->getTimer()->start(5000,true,sHandleTimeout,this,1);

    isStart_ = true;

    return true;
}

void XLorb::stop()
{
    if(!isStart_)return;
    if(quitcheck_) JinMainThread::Get()->getTimer()->cancel(quitcheck_);
    quitcheck_ = 0;
    JinHashMap<NetID,ClientInfo*>::iterator it = allConnected_.begin();
    while(it != allConnected_.end())
    {
        ClientInfo* info = it.second();
        NetID id = it.first();
        if(info)
        {
            DoCloseAdmitID(info, id);
            allocator_.back(info);
        }
        //allConnected_.remove(id);
        it = allConnected_.erase(it);
        //++it;
    }

    if(outerAdmit_)outerAdmit_->Stop();
    IOuterAdmit::DestroyAdmit(outerAdmit_);
    outerAdmit_ = NULL;
    isStart_ = false;
}

void XLorb::DoCloseAdmitID(ClientInfo *info, const NetID &id)
{
    JAssert(info->id == id);
    JDLog("closing addr=%s|%hu id=%"PRIu64,
          info->fromAddr.toAddr4(gDLogBufAddr),info->fromPort.port(),id);
    ClientMoreInfo* &minfo = info->more;
    if(minfo)
    {
        TunnSockMap* tm = minfo->tunnSock;
        if(tm)
        {
            TunnSockMap::iterator it = tm->begin();
            while(it!=tm->end())
            {
                XLorsSocket* lsock = it.second();
                lsock->close();
                JDelete(lsock);
                ++it;
            }
            tm->clear();
        }
        if(minfo->ackCountDownTimer)
        {
            JinMainThread::Get()->getTimer()->cancel\
                    (minfo->ackCountDownTimer);
            minfo->ackCountDownTimer = 0;
        }
        if(minfo->pendDoRecv)
        {
            JDelete(minfo->pendDoRecv);
        }
        if(minfo->tunnSock)
        {
            JDelete(minfo->tunnSock);
        }
        if(minfo->compressor)
        {
            JDelete(minfo->compressor);
        }
        JFree(minfo);
        minfo = 0;
    }
}

void XLorb::DoRecvUnderFlow(XLorsSocket *sock, CallbackInfo info)
{
    if(info->more->fcSended - info->more->fcAcked > lorsSet->flowCtrlCache)
    {
        JDLog("pend do recv. tunnid[%hu]",sock->tunnelID());
        if(info->more->pendDoRecv == NULL)
        {
            info->more->pendDoRecv = JNew(SockPendMap);
            if(info->more->pendDoRecv == NULL)
            {
                sock->doRecv();
                return;
            }
        }
        JAssert(!info->more->pendDoRecv->has(sock));
        info->more->pendDoRecv->set(sock, 0);
    }
    else
        sock->doRecv();
    return;
}

void XLorb::sHandleTimeout(TimerID id, void *user, uint64_t user64)
{
    XLorb* lorb = (XLorb*)user;
    lorb->handleTimeout(id,(ClientInfo*)user64);
}

void XLorb::handleTimeout(TimerID id, ClientInfo* info)
{
    if(id == quitcheck_)
    {
        CheckQuit();
        return;
    }

    JAssert(id == info->more->ackCountDownTimer);
    if(id != info->more->ackCountDownTimer) return;
    info->more->ackCountDownTimer = 0;
    if(info->more->fcWeRecv > info->more->fcWeAcked)
    {
        info->more->fcWeAcked = info->more->fcWeRecv;
        ReplyAckTo(info, info->more->fcWeAcked);
        info->more->ackAlreadyCoolDown = false;
    }
}

//ISinkForLorsSocket
void XLorb::OnConnect(XLorsSocket *sock, CallbackInfo info, bool success, const JinNetAddr &addr, const JinNetPort &port)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstConnDone);
    uint16_t tunnid = sock->tunnelID();
    JAssert(tunnid > 0);
    joiner.pushNumber(kUkTunnelID,tunnid);
    joiner.pushNumber(kUkErrorCode,(uint16_t)(success?kEcSuccess:kEcConnFailed));
    //kUkIPv4\kUkPort 给SOCK5用
    JinNetAddr localAddr; JinNetPort localPort;
    sock->getLocalAddr(localAddr,localPort);
    joiner.pushNumber(kUkIPv4,localAddr.rawIP());
    joiner.pushNumber(kUkPort,localPort.rawPort());
    outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id, tunnid);
    JPLog("tunnid[%hu] succ=%d %s|%hu.",tunnid,(int)success,
          addr.toAddr4(gDLogBufAddr),port.port());
    if(!success)
    {
        info->more->tunnSock->remove(tunnid);
        sock->close();
        JDelete(sock);
        return;
    }

    DoRecvUnderFlow(sock,info);
    return;
}

//ISinkForLorsSocket
void XLorb::OnRecv(XLorsSocket *sock, CallbackInfo info,const char *buf, uint32_t byteRecved)
{
#ifdef JDEBUG
    if(byteRecved<=32)
    {
        JinString s(buf,byteRecved);
        JDLog("debugD: %s",s.toReadableHex().c_str());
    }
#endif
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstDataTransfer);
    uint16_t tunnid = sock->tunnelID();
    JAssert(tunnid > 0);
    joiner.pushNumber(kUkTunnelID,tunnid);
    //尝试压缩
    bool bCompress = false;
    if(info->more->compressor && byteRecved>0)
    {
        JinRoBuf rbuf = info->more->compressor->compress(buf,byteRecved);
        JAssert(rbuf.len!=0);
        //if(rbuf.len != 0 && rbuf.len<byteRecved &&
        //        (rbuf.len+kCompressEffectLen)<=byteRecved &&
        //        rbuf.len < byteRecved*kCompressEffectPercent/100)
        if(rbuf.len != 0)  // 流压缩，所有的都得发对方都要解，否则解不出来.--》如此正好不是明文传输了..
        {
            joiner.pushNumber(kUkCompress,(uint8_t)'q');
            joiner.pushString(kUkData,rbuf.buf,(uint32_t)rbuf.len);
            bCompress = true;
        }
    }
    if(!bCompress)
    {
        joiner.pushString(kUkData,buf,byteRecved);
    }
    //joiner.pushString(kUkData,(const char*)buf,byteRecved);
    outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id,
                        bCompress?kUkCompress:tunnid); //压缩的用同一通道保持顺序
    info->more->fcSended += joiner.length();
    JDLog("[debug]tunnid[%hu] recv< %d => total sended %llu",
          tunnid,joiner.length(),info->more->fcSended);

    DoRecvUnderFlow(sock,info);
    return;
}

//ISinkForLorsSocket
void XLorb::OnDisconnect(XLorsSocket *sock, CallbackInfo info, bool byRemote)
{
    JDLog("disconn byRemote=%d. tunnid[%hu]",(int)byRemote,sock->tunnelID());
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstConnAbort);
    uint16_t tunnid = sock->tunnelID();
    JAssert(tunnid > 0);
    joiner.pushNumber(kUkTunnelID,tunnid);
    joiner.pushString(kUkMessage,(byRemote?"byRemote":"internal"));
    outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id, tunnid);

    sock->close();
    info->more->tunnSock->remove(tunnid);
    if(info->more->pendDoRecv && info->more->pendDoRecv->has(sock))
        info->more->pendDoRecv->remove(sock);
    JDelete(sock);

    return;
}


//ISinkForOuterAdmit
bool XLorb::OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                   NetEngineMsg *packet, const int lenFromMainID, int head)
{
    UNUSED(addr);UNUSED(port);UNUSED(head);
    ClientInfo *info = allConnected_.get(id, NULL);
    if(!info || lenFromMainID<0) //不可能<0
    {
        JELog("id=%"PRIu64" not connected.",id);
        return false;
    }

    JinPackParser parser((const char*)packet,lenFromMainID+1,1);
    if(!parser.isValid() || parser.mainID()!=kMainF2A)
    {
        JELog("id=%"PRIu64" packet parser error.",id);
        return false;
    }

    switch(parser.asstID())
    {
    case kAsstDataTransfer:
        return OnAdmitDataTransfer(info,&parser,lenFromMainID+1);
        break;
    case kAsstOpenTo:
        JDLog("id=%"PRIu64" get kAsstOpenTo",id);
        return OnAdmitOpenTo(info,&parser);
        break;
    case kAsstCloseTunnel:
        JDLog("id=%"PRIu64" get kAsstCloseTunnel",id);
        return OnAdmitCloseTunnel(info,&parser);
        break;
    case kAsstFlowCtrl:
        return OnAdmitFlowCtrl(info,&parser);
        break;
    case kAsstGreeting:
        return OnAdmitGreeting(info,&parser);
        break;
    default:
        JELog("can't handle asstid:%hu",parser.asstID());
    }

    return true; //返回false表示会断掉连接,很严重.
}

//ISinkForOuterAdmit
bool XLorb::OnIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port)
{
    JPLog("income addr=%s|%hu id=%"PRIu64,addr.toAddr4(gDLogBufAddr),port.port(),id);
    if(allConnected_.has(id)) return false;
    ClientInfo* info = allocator_.alloc();
    info->state = ClientInfo::kConn;
    info->fromAddr = addr;
    info->fromPort = port;
    info->id = id;
    info->firstGlance = JinDateTime::tickCount();
    info->more = NULL;
    allConnected_.set(id,info);
    return true;
}

//ISinkForOuterAdmit
void XLorb::OnClose(const NetID &id, bool grace)
{
    JPLog("closing id=%"PRIu64", grace=%d",id,grace?1:0);
    ClientInfo *info = allConnected_.get(id, NULL);
    if(info)
    {
        DoCloseAdmitID(info, id);
        allConnected_.remove(id);
        allocator_.back(info);
    }
}

void XLorb::CheckQuit()
{
    JinFileDir tmpDir;
    tmpDir.setWith(kDirHome);
    const char* quitflag = tmpDir.fullPath("XLorb.quit");
    if(JinFile::exist(quitflag))
    {
        JinFile::remove(quitflag);
        JinMainThread::notifyQuit();
    }
}

bool XLorb::OnAdmitDataTransfer(ClientInfo *info, JinPackParser *parser, size_t len)
{
    InfoMoreType& more = info->more;
    if(more == NULL || more->tunnSock == NULL){
        JELog("no more, no more tunn sock. more=%p",more);
        return true;  //true表示不影响已有连接.
    }
    more->fcWeRecv += len;  //no tunnid的情况仍不想影响ack,故提前处理了.
    uint16_t tunnid = parser->get16u(kUkTunnelID);
    JDLog("[debug]tunnid[%hu] >trans %d => total recved %llu",tunnid,len,more->fcWeRecv);

    if(more->fcWeRecv - more->fcWeAcked >= kFlowControlAckNoDelay)
    {
        if(!more->ackAlreadyCoolDown)
        {
            if(more->ackCountDownTimer){
                JinMainThread::Get()->getTimer()->cancel(more->ackCountDownTimer);
                more->ackCountDownTimer = 0;
            }
            //时间为0的timer,下一个poll一定会触发.相当于我可以等网络这一波多个包收完.
            more->ackCountDownTimer = JinMainThread::Get()->getTimer()->start\
                    (0,false,sHandleTimeout,this,uintptr_t(info));
            more->ackAlreadyCoolDown = true;
        }
    }
    else
    {
        if(more->ackCountDownTimer==0){
            more->ackCountDownTimer = JinMainThread::Get()->getTimer()->start\
                    (kFlowControlMinAckInterval,false,sHandleTimeout,this,uintptr_t(info));
        }
    }

    //uint16_t tunnid = parser->get16u(kUkTunnelID);
    XLorsSocket *lsock = more->tunnSock->get(tunnid,NULL);
    if(lsock==NULL)
    {
        JDLog("no tunnid socket, tunnid[%hu]",tunnid);
        //return true; 被舍弃的tunnid也可能收到data, ack一样要处理,另外压缩因为要保持流压缩的优势,每块数据都要处理.
    }
    JinRoBuf data = parser->get(kUkData);
    if(data.len > 0)
    {
        //发现和解压.
        uint8_t cmp = parser->get8u(kUkCompress);
        if((more->compressor==NULL && cmp!=0) || (cmp!=0 && cmp!='q'))
        {
            JELog("unexpect compress %c, obj=%p",cmp,more->compressor);
            return false;
        }
        if(cmp == 'q')
        {
//#ifdef JDEBUG
//            size_t oldlen = data.len;
//            data = more->compressor->decompress(data.buf,data.len);
//            JAssert(data.len > oldlen);
//#else
            data = more->compressor->decompress(data.buf,data.len);
//#endif
            if(data.len == 0)
            {
                JELog("decompress fail");
                return false;
            }
        }
        if(lsock)
        {
#ifdef JDEBUG
            if(data.len<=32)
            {
                JinString s(data.buf,data.len);
                JDLog("debugC: %s",s.toReadableHex().c_str());
            }
#endif
            JinAsynBuffer abuf(data.len);
            memcpy(abuf.buff(),data.buf,data.len);
            lsock->send(abuf,(uint32_t)data.len);
        }
        return true;
    }
    else
    {
        JELog("get null kUkData");
    }
    return false;
}

bool XLorb::ReplyAckTo(ClientInfo *info, uint64_t ack)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstFlowCtrl);
    joiner.pushNumber(kUkAck,ack);

    return outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id);
}

bool XLorb::OnAdmitOpenTo(ClientInfo *info, JinPackParser *parser)
{
    JinString openTarget = parser->getStr(kUkTarget);
    uint16_t openPort = parser->get16u(kUkPort);
    uint16_t openTunnid = parser->get16u(kUkTunnelID);
    JinString openRequest = parser->getStr(kUkData);
    if(openPort == 0 || openTarget.length() == 0 || openTunnid == 0)
    {
        JELog("open %s|%hu tunnid[%hu] error",openTarget.c_str(),openPort,openTunnid);
        return false;
    }
    InfoMoreType& more = info->more;
    if(more == NULL)
    {
        more = (ClientMoreInfo*)JMalloc(sizeof(ClientMoreInfo));
        if(more == NULL)
        {
            ReplyOpenTo(info, openTunnid,kEcOutOfMemory,"OutOfMemory.1");
            return false;
        }
        memset(more,0,sizeof(ClientMoreInfo));
    }
    if(more->tunnSock == NULL)
    {
        more->tunnSock = JNew(TunnSockMap);
        if(more->tunnSock == NULL)
        {
            ReplyOpenTo(info, openTunnid,kEcOutOfMemory,"OutOfMemory.2");
            return false;
        }
    }
    if(more->tunnSock->size() >= lorsSet->maxConnPipe)
    {
        ReplyOpenTo(info, openTunnid,kEcReachMaximum,"ConnReachMax");
        return true;  //不严重,不返回false.
    }
    if(more->tunnSock->has(openTunnid))
    {
        ReplyOpenTo(info, openTunnid,kEcTunnidExist,"Tunnel id invalid.");
        return false;
    }
    XLorsSocket* lsock = JNew(XLorsSocket);
    if(lsock == NULL)
    {
        ReplyOpenTo(info, openTunnid,kEcOutOfMemory,"OutOfMemory.3");
        return false;
    }
    if(inside_.isValid())  lsock->bind(inside_,0);
    lsock->setTunnelID(openTunnid);
    lsock->setCallback(this);
    lsock->setCallbackInfo(info);
    JPLog("tunnid[%hu] doConnect %s|%hu. cache=%d",openTunnid,openTarget.c_str(),openPort,(int)openRequest.length());
    if(kTSucc != lsock->doConnect(openTarget, openPort))
    {
        JELog("open %s|%hu conn error",openTarget.c_str(),openPort);
        ReplyOpenTo(info, openTunnid,kEcInternalError,"DoConnDirectFail");
        JDelete(lsock);
        return false;
    }

    //预期会pend起来.
    JinAsynBuffer abuf(openRequest.length());
    memcpy(abuf.buff(),openRequest.c_str(),openRequest.length());
    lsock->send(abuf,(uint32_t)openRequest.length());

    if(!more->tunnSock->insert(openTunnid,lsock))
    {
        ReplyOpenTo(info, openTunnid,kEcInternalError,"TuMapInsertFail");
        JDelete(lsock);
        return false;
    }
    return ReplyOpenTo(info, openTunnid,kEcSuccess);
}

bool XLorb::ReplyOpenTo(ClientInfo* info, uint16_t tunnid, XlorsErrorCode ec, const char* eMsg)
{
    JDLog("tunnid[%hu], ec=%d, eMsg=%s",tunnid,(int)ec,eMsg?eMsg:"");
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstRtnOpenTo);
    joiner.pushNumber(kUkTunnelID,tunnid);
    joiner.pushNumber(kUkErrorCode,(uint16_t)ec);
    if(eMsg) joiner.pushString(kUkMessage,eMsg);

    return outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id, tunnid);
}

bool XLorb::OnAdmitCloseTunnel(ClientInfo *info, JinPackParser *parser)
{
    InfoMoreType& more = info->more;
    if(more == NULL || more->tunnSock == NULL){
        JDLog("no more, no more tunn sock. more=%p",more);
        return true;  //true表示不影响已有连接.
    }
    uint16_t tunnid = parser->get16u(kUkTunnelID);
    XLorsSocket *lsock = more->tunnSock->get(tunnid,NULL);
    if(lsock==NULL){
        JDLog("no tunnid[%hu] socket",tunnid);
        return true;
    }
    JDLog("tunnid[%hu] disconn.",tunnid);
    int dret = lsock->doDisconnect();
    if(dret != kTSucc)
    {
        lsock->close();
        more->tunnSock->remove(tunnid);
        if(more->pendDoRecv && more->pendDoRecv->has(lsock))
            more->pendDoRecv->remove(lsock);
        JDelete(lsock);
    }
    return true;
}

bool XLorb::OnAdmitGreeting(ClientInfo *info, JinPackParser *parser)
{
    InfoMoreType& more = info->more;
    if(more == NULL)
    {
        more = (ClientMoreInfo*)JMalloc(sizeof(ClientMoreInfo));
        if(more == NULL) return false;
        memset(more,0,sizeof(ClientMoreInfo));
    }
    JinString identity = parser->getStr(kUkIdentity);
    if(identity.length()>0){
        JDLog("greeting by %s",identity.c_str());
    }

    JinString cmp = parser->getStr(kUkCompress);
    if(cmp.length()>0)
    {//如果重复提供，认为错误!
        if(more->compressor!=NULL){
            JDLog("duplicate compress negotiate,addr=%s|%hu id=%"PRIu64,
                  info->fromAddr.toAddr4(gDLogBufAddr),info->fromPort.port(),info->id);
            return false;
        }
        size_t pos = cmp.find("q",0,true);
        if(pos != STRNPOS)
        {
            JDLog("compressor using quicklz");
            more->compressor = JNew(JinCompress,kTypeQuickLz);
        }
    }

    return ReplyGreeting(info,'q');  //只支持compress=‘q’ quicklz
}

bool XLorb::ReplyGreeting(ClientInfo *info, char cmpChr)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainA2F,kAsstGreeting);
    joiner.pushString(kUkIdentity,selfIdentity_.c_str());
    joiner.pushNumber(kUkCompress,(uint8_t)cmpChr);
    return outerAdmit_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id);
}

bool XLorb::OnAdmitFlowCtrl(ClientInfo *info, JinPackParser *parser)
{
    InfoMoreType& more = info->more;
    if(more == NULL || more->tunnSock == NULL){
        JDLog("no more, no more tunn sock. more=%p",more);
        return true;  //true表示不影响已有连接.
    }
    uint64_t ack = parser->get64u(kUkAck);
    JDLog("XLord ack=%"PRIu64",last fcAcked=%"PRIu64",sended=%"PRIu64,
          ack, more->fcAcked, more->fcSended);
    if(ack <= more->fcAcked || ack > more->fcSended){
        JELog("bad ack.");
        return false;
    }
    more->fcAcked = ack;


    if(more->pendDoRecv && more->pendDoRecv->size()>0 &&
            more->fcSended - more->fcAcked < lorsSet->flowCtrlCache)
    {
        SockPendMap::iterator it = more->pendDoRecv->begin();
        while(it != more->pendDoRecv->end())
        {
            XLorsSocket* lsock = it.first();
            lsock->doRecv();
            ++it;
        }
        more->pendDoRecv->clear();
    }
    return true;
}

