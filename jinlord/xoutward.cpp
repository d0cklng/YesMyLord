#include "jinassert.h"
#include "jinlogger.h"
#include "jinnetaddress.h"
#include "jinassistthread.h"
#include "jinhashmap.h"
#include "jinfiledir.h"
#include "jinfile.h"
#include "xlorddef.h"
#include "jinpackhelper.h"
#include <stdlib.h>
#include "xoutward.h"
#include "jincompress.h"

#define KB *1024
#define MB *1024*1024
typedef JinHashMap<uint32_t, XLorsSocket*> TunnSockMap;  //tunnel id => socket.
typedef JinHashMap<XLorsSocket*, int> SockPendMap;
static const uint64_t kFlowControlMaxCache = 1 MB;
static const uint64_t kFlowControlAckNoDelay = 256 KB;
static const uint16_t kDefaultMaxConnPipe = 10;
static const int kFlowControlMinAckInterval = 1200;
//static const int kCompressEffectLen = 100;
//static const int kCompressEffectPercent = 90;



struct XOutwardOuterInfo
{
    uint64_t fcAcked;   //FlowControl 对方收到且确认的
    uint64_t fcSended;  //FlowControl 已发送到
    TimerID ackCountDownTimer;  //控制最快ack时间间隔,同时确保没数据后也要把最后的数据ack掉
    bool ackAlreadyCoolDown;  //如果true,表示将马上ReplyAck,否则表示不急于ack
    uint64_t fcWeAcked;
    uint64_t fcWeRecv;
    SockPendMap* pendDoRecv;  //流控机制在拥塞时不执行recv,在此排队
    TunnSockMap* TunnSock;  //分配的tunnid对应的socket
    NetID targetid;
    uint16_t tunnidAssigner;
    JinCompress *compressor;
#include "jincompress.h"

};

XOutward::XOutward(const JinNetAddr &bindaddr, const JinNetPort &bindport,
             const JinString &selfIdentify)
    : info_(NULL)
    , listen_(NULL)
    , xlorb_(NULL)
    , isStart_(false)
    , selfIdentity_(selfIdentify)
    , proxyAssist_(this)
{
    bindAddr_ = bindaddr;
    bindPort_ = bindport;
    info_ = (XOutwardOuterInfo*)JMalloc(sizeof(XOutwardOuterInfo));
    memset(info_,0,sizeof(XOutwardOuterInfo));

    JAssert(PTHTTP == -PTErrorHTTP);
    JAssert(PTHTTPS == -PTErrorHTTPS);
}

XOutward::~XOutward()
{
    stop();
    if(info_->compressor) JDelete(info_->compressor);
    JFree(info_);
}

bool XOutward::start(IXLorb* xlorb)
{
    JAssert(xlorb && xlorb_==NULL);
    xlorb_ = xlorb;
    if(isStart_)stop();
    JAssert(isStart_==false);
    //先不构造listen_直到find成功

    isStart_ = true;

    return true;
}

void XOutward::stop()
{
    if(!isStart_)return;
    if(info_ && info_->ackCountDownTimer)
    {
        JinMainThread::Get()->getTimer()->cancel(info_->ackCountDownTimer);
        info_->ackCountDownTimer = 0;
    }
    if(listen_)
    {
        listen_->close();
        JDelete(listen_);
        listen_ = NULL;
    }

    JinHashMap<uint16_t, ProxyClientInfo*>::iterator it = allIncoming_.begin();
    while(it!=allIncoming_.end())
    {
        ProxyClientInfo* info = it.second();
        JAssert(info->sock->tunnelID() == it.first());
        JDLog("direct close sock tunnid[%hu], no wait.",info->sock->tunnelID());
        info->sock->close();
        JDelete(info->sock);
        JDelete(info);
        ++it;
    }
    allIncoming_.clear();

    JinHashMap<uint16_t, XOutwardForwardInfo*>::iterator it2 = allForwarding_.begin();
    while(it2!=allForwarding_.end())
    {
        XOutwardForwardInfo* finfo = it2.second();
        finfo->listen.close();
        JDelete(finfo);
        ++it2;
    }
    allForwarding_.clear();

    isStart_ = false;
}

bool XOutward::addForwarding(uint16_t bindport,const JinString &targetaddr,uint16_t targetport)
{
    if(allForwarding_.has(bindport))
    {
        JELog("port=%hu exist!",bindport);
        return false;
    }
    XOutwardForwardInfo *finfo = JNew(XOutwardForwardInfo);
    if(finfo == NULL)
    {
        JELog("Out of memory?");
        return false;
    }
    XLorsSocket *sock = &finfo->listen;
    sock->setCallback(this);
    sock->setTunnelID(0);
    sock->setCallbackInfo(NULL);
    JDLog("do forward bind %s:%hu",bindAddr_.toAddr(gDLogBufAddr),bindport);
    int ret = sock->bind(bindAddr_,JinNetPort(bindport));
    if(ret != kTSucc)
    {
        JELog("bind failed! %d",ret);
        JDelete(finfo);
        return false;
    }
    ret = sock->listen();
    if(ret != kTSucc)
    {
        JELog("listen failed! %d",ret);
        JDelete(finfo);
        return false;
    }
    ret = sock->accept();
    if(ret != kTSucc){
        JELog("accept failed! %d",ret);
        JDelete(finfo);
        return false;
    }
    if(!allForwarding_.set(bindport,finfo))
    {
        JDelete(finfo);
        return false;
    }
    finfo->targetAddr = targetaddr;
    finfo->targetPort = targetport;
    JPLog("bind %s:%hu forwarding %s:%hu",
          bindAddr_.toAddr(gDLogBufAddr),bindport,
          targetaddr.c_str(),targetport);
    //JDLog("succ add forward, listen=%hu",bindport);
    return true;
}


void XOutward::DoRecvUnderFlow(XLorsSocket *sock)
{
    if(info_->fcSended - info_->fcAcked > kFlowControlMaxCache)
    {
        if(info_->pendDoRecv == NULL)
        {
            info_->pendDoRecv = JNew(SockPendMap);
            if(info_->pendDoRecv == NULL)
            {
                sock->doRecv();
                return;
            }
        }
        JAssert(!info_->pendDoRecv->has(sock));
        info_->pendDoRecv->set(sock, 0);
    }
    else
        sock->doRecv();
    return;
}

void XOutward::CloseTunnel(uint16_t tunnid)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainF2A,kAsstCloseTunnel);
    JAssert(tunnid > 0);
    joiner.pushNumber(kUkTunnelID,tunnid);
    xlorb_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info_->targetid, tunnid);
    JDLog("CloseTunnel tunnid[%hu]",tunnid);
}

void XOutward::CloseIncoming(ProxyClientInfo *info)
{
    int dret = info->sock->disconnect();
    if(dret != kTSucc)
    {
        bool ret = allIncoming_.remove(info->sock->tunnelID());
        JAssert(ret);
        JDLog("disconn direct fail, close at once. tunnid[%hu]",info->sock->tunnelID());
        info->sock->close();
        JDelete(info->sock);
        JDelete(info);
    }
}

void XOutward::DoSendData(uint16_t tunnid, const char *buf, uint32_t len)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainF2A,kAsstDataTransfer);
    JAssert(tunnid > 0);
    joiner.pushNumber(kUkTunnelID,tunnid);
    //尝试压缩
    bool bCompress = false;
    if(info_->compressor && len>0)
    {
        JinRoBuf rbuf = info_->compressor->compress(buf,len);
        JAssert(rbuf.len!=0);
        //if(rbuf.len != 0 && rbuf.len < len &&  //压缩成功且有效,
        //        (rbuf.len+kCompressEffectLen)<=len &&
        //        rbuf.len < len*kCompressEffectPercent/100)
        if(rbuf.len != 0)  // 流压缩，所有的都得发对方都要解，否则解不出来.--》如此正好不是明文传输了..
        {
            joiner.pushNumber(kUkCompress,(uint8_t)'q');
            joiner.pushString(kUkData,rbuf.buf,(uint32_t)rbuf.len);
            bCompress = true;
        }
    }
    if(!bCompress)
    {
        joiner.pushString(kUkData,buf,len);
    }
    xlorb_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info_->targetid,
                         bCompress?kUkCompress:tunnid); //压缩的用同一通道保持顺序
    info_->fcSended += joiner.length();
    JDLog("[debug]tunnid[%hu] send> %d => total sended %llu",
          tunnid,joiner.length(),info_->fcSended);

}

void XOutward::DoSendOpenTo(uint16_t tunnid, const JinString &target, uint16_t port, const JinString &data)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainF2A,kAsstOpenTo);
    joiner.pushNumber(kUkTunnelID,tunnid);
    joiner.pushString(kUkTarget,target.c_str(),(uint32_t)target.length());
    joiner.pushNumber(kUkPort,port);
    if(data.length())
    {
        joiner.pushString(kUkData,data.c_str(),(uint32_t)data.length());
    }
    xlorb_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info_->targetid, tunnid);
    JPLog("tunnid[%hu] Open to %s|%hu",tunnid,target.c_str(),port);
    //是否要     info_->fcSended += joiner.length();
}

void XOutward::sHandleTimeout(TimerID id, void *user, uint64_t user64)
{
    UNUSED(user64);
    XOutward* lord = (XOutward*)user;
    lord->handleTimeout(id);
}

void XOutward::handleTimeout(TimerID id)
{
    JAssert(id == info_->ackCountDownTimer);
    if(id != info_->ackCountDownTimer) return;
    info_->ackCountDownTimer = 0;
    if(info_->fcWeRecv > info_->fcWeAcked)
    {
        info_->fcWeAcked = info_->fcWeRecv;
        ReplyAckTo(info_->fcWeAcked);
        info_->ackAlreadyCoolDown = false;
    }
}

//ISinkForLorsSocket
void XOutward::OnRecv(XLorsSocket *sock, ProxyClientInfo *info, const char *buf, uint32_t byteRecved)
{
    JAssert(info->sock = sock);
    JDLog("OnRecv len=%u tunnid[%hu]",byteRecved, sock->tunnelID());
#ifdef JDEBUG
    if(byteRecved<32)
    {
        JinString s(buf,byteRecved);
        JDLog("debugA: %s",s.toReadableHex().c_str());
    }
#endif
    if( proxyAssist_.OnProxyRecv(buf,byteRecved,info) )
    {
        DoRecvUnderFlow(info->sock);
    }
    else
    {
        JDLog("OnProxyRecv false, tunnid[%hu]",sock->tunnelID());
    }

}

//ISinkForLorsSocket
void XOutward::OnDisconnect(XLorsSocket *sock, ProxyClientInfo *info, bool byRemote)
{
    JDLog("disconn byRemote=%d. tunnid[%hu], target=%s",(int)byRemote,sock->tunnelID(),info->target.c_str());
    bool ret = allIncoming_.remove(sock->tunnelID());
    JAssert(ret);
    if(byRemote && info->state > Larger_IS_AlreadySend_OpenTo)
    {
        JDLog("close tunnid[%hu] to xlorb",sock->tunnelID());
        CloseTunnel(sock->tunnelID());
    }
    sock->close();
    JDelete(sock);
    JDelete(info);
    return;
}

//ISinkForLorsSocket
void XOutward::OnAccept(XLorsSocket *sock, ProxyClientInfo *cbinfo, HdSocket accepted, const JinNetAddr &remoteAddr, const JinNetPort &remotePort)
{
    UNUSED(cbinfo);
    //JPLog("income from %s|%hu", remoteAddr.toAddr(gDLogBufAddr),remotePort.port());
    //JAssert(sock == listen_); 现在有forwarding绑定了.
    do  //选取tunnid.
    {
        info_->tunnidAssigner = info_->tunnidAssigner+1;
        if(info_->tunnidAssigner == 0) info_->tunnidAssigner = 1;
        if(!allIncoming_.has(info_->tunnidAssigner)) break;
    }
    while(info_->tunnidAssigner);
    ProxyClientInfo *info = JNew(ProxyClientInfo);//(ProxyClientInfo*)JMalloc(sizeof(ProxyClientInfo));
    XLorsSocket *nsock = JNew(XLorsSocket,accepted,remoteAddr,remotePort);
    if(!nsock || !info)
    {
        if(nsock)JDelete(nsock);
        if(info)JDelete(info);
        JELog("out of memory!");
        //JinMainThread::();
        return;
    }
    info->sock = nsock;
    info->state = WaitTarget;
    info->proxyType = PTContinueRecv;
    info->contentLengthLeft = 0;
    nsock->setTunnelID(info_->tunnidAssigner);
    nsock->setCallback(this);
    nsock->setCallbackInfo(info);
    allIncoming_.set(info_->tunnidAssigner, info);
    JPLog("accept sock=%p %s|%hu tunnid[%hu]",nsock,
          remoteAddr.toAddr(gDLogBufAddr),remotePort.port(), info_->tunnidAssigner);

    if(sock != listen_)
    { //forwarding listen sock.
        JinNetAddr bdaddr; JinNetPort bdport;
        sock->getLocalAddr(bdaddr,bdport);
        XOutwardForwardInfo* finfo = allForwarding_.get(bdport.port(),NULL);
        JAssert(finfo);
        if(finfo)
        {
            char bufL[256];
            int len = sprintf(bufL,"CONNECT %s:%hu HTTP/1.1\r\n\r\n",finfo->targetAddr.c_str(),
                              finfo->targetPort);
            JDLog("tunnid[%hu] make forwarding to %s:%hu",nsock->tunnelID(),
                  finfo->targetAddr.c_str(),finfo->targetPort);
            if(!proxyAssist_.OnProxyRecv(bufL,len,info) )
            {
                JAssert(false);
            }
            JAssert(info->proxyType == PTHTTPS);
            info->proxyType = PTFORWARD;
        }
    }

    int ret;
    if(kTSucc != (ret = sock->accept()))
    {
        JELog("accept error=%d",ret);
    }
    DoRecvUnderFlow(nsock);
}



//ISinkForOuterFinder
LordError XOutward::OnFind(const NetID &id, const JinNetAddr &addr, const JinNetPort &port)
{
    JDLog("succ contact %"PRIu64" %s|%hu",id, addr.toAddr(gDLogBufAddr),port.port());
    info_->targetid = id;

    JAssert(listen_==NULL);
    listen_ = JNew(XLorsSocket);
    if(listen_ == NULL)
    {
        JELog("out of memory!");
        return LeOutOfMemory;
    }
    listen_->setCallback(this);
    listen_->setTunnelID(0);
    listen_->setCallbackInfo(NULL);
    JDLog("do bind %s:%hu",bindAddr_.toAddr(gDLogBufAddr),bindPort_.port());
    int ret = listen_->bind(bindAddr_,bindPort_);
    if(ret != kTSucc)
    {
        JELog("bind failed! %d",ret);
        return LeProxyBindErr;
    }
    ret = listen_->listen();
    if(ret != kTSucc)
    {
        JELog("listen failed! %d",ret);
        return LeProxyListenErr;
    }
    ret = listen_->accept();
    if(ret != kTSucc){
        JELog("accept failed! %d",ret);
        return LeProxyAcceptErr;
    }


    // 发送Greeting
    JinPackJoiner joiner;
    joiner.setHeadID(kMainF2A,kAsstGreeting);
    joiner.pushString(kUkIdentity,selfIdentity_.c_str()); //对对方而言，是不可靠的名字.
    joiner.pushString(kUkCompress,"q",1);  //支持quicklz,虽然就1个但必须用string

    if(!xlorb_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info_->targetid))
    {
        JELog("send error.");
        return LeSendGreeingErr;
    }
    return LeSuccess;
}

//ISinkForOuterFinder
void XOutward::OnRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                   NetEngineMsg *packet, const int lenFromMainID, int head)
{
    UNUSED(addr);UNUSED(port);UNUSED(head);
    if(lenFromMainID<0) return;

    JinPackParser parser((const char*)packet,lenFromMainID+1,1);
    if(!parser.isValid() || parser.mainID()!=kMainA2F)
    {
        JELog("packet parser error.",id);
        return;
    }

    uint16_t tunnid = parser.get16u(kUkTunnelID);
    ProxyClientInfo *info = allIncoming_.get(tunnid,NULL);
    if(info == NULL && parser.asstID()!=kAsstFlowCtrl && parser.asstID()!=kAsstGreeting)
    {
        JDLog("tunnid[%hu] not found. assid=%hu",tunnid,parser.asstID());
        //CloseTunnel(tunnid); 事实上正常情况这都是多余的.非正常情况谁管呢.
        if(parser.asstID()!=kAsstDataTransfer)
        {
            //data transfer一直都不处理info为NULL,然而应该要处理
            //一方面是ack的内容,一方面是流加密必须顺序不可缺少.
            return;
        }
    }

    switch(parser.asstID())
    {
    case kAsstDataTransfer:
        OnFinderDataTransfer(info,&parser,lenFromMainID+1);
        break;
    case kAsstRtnOpenTo:
        JDLog("tunnid[%hu] get kAsstRtnOpenTo %s|%hu info=%p id=%"PRIu64,info->sock->tunnelID(),
              info->target.c_str(),info->port.port(),info,id);
        OnFinderRtnOpenTo(info,&parser);
        break;
    case kAsstConnDone:
        JDLog("tunnid[%hu] get kAsstConnDone %s|%hu info=%p id=%"PRIu64,info->sock->tunnelID(),
              info->target.c_str(),info->port.port(),info,id);
        OnFinderConnDone(info,&parser);
        break;
    case kAsstConnAbort:
        JDLog("tunnid[%hu] get kAsstConnAbort %s|%hu info=%p id=%"PRIu64,info->sock->tunnelID(),
              info->target.c_str(),info->port.port(),info,id);
        OnFinderConnAbort(info,&parser);
        break;
    case kAsstGreeting:
        JDLog("get kAsstGreeting(Rtn) info=%p id=%"PRIu64,info,id);
        OnFinderGreeting(info,&parser);
        break;
    case kAsstFlowCtrl:
    {
        uint64_t ack = parser.get64u(kUkAck);
        OnFinderFlowCtrl(ack);
    }
        break;
    default:
        JELog("can't handle asstid:%hu",parser.asstID());
    }

    return;
}

//ISinkForOuterFinder
void XOutward::OnClose(const NetID &id, bool grace)
{
    JELog("halt %"PRIu64" grace=%d",id,(int)grace);
    JAssert(id == info_->targetid);
    if(info_->targetid != id) return;
    info_->targetid = 0;
    if(listen_)
    {
        listen_->close();
        JDelete(listen_);
        listen_ = NULL;
    }

    JinHashMap<uint16_t, ProxyClientInfo*>::iterator it = allIncoming_.begin();
    while(it!=allIncoming_.end())
    {
        ProxyClientInfo* info = it.second();
        JAssert(info->sock->tunnelID() == it.first());
        int dret = info->sock->disconnect();
        if(dret != kTSucc)
        {
            it = allIncoming_.erase(it);
            JDLog("disconn direct fail, close at once. tunnid[%hu]",info->sock->tunnelID());
            info->sock->close();
            JDelete(info->sock);
            JDelete(info);
        }
        else
        {
            ++it;
        }

    }
}

//被舍弃的tunnid也可能收到data, ack一样要处理,另外压缩因为要保持流压缩的优势,每块数据都要处理.
void XOutward::OnFinderDataTransfer(ProxyClientInfo *info, JinPackParser *parser, size_t len)
{
    info_->fcWeRecv += len;  //no tunnid的情况仍不想影响ack,故提前处理了.
    if(info)
    {
        JDLog("[debug]tunnid[%hu] trans< %d => total recved %llu",info->sock->tunnelID(),len,info_->fcWeRecv);
    }


    if(info_->fcWeRecv - info_->fcWeAcked >= kFlowControlAckNoDelay)
    {
        if(!info_->ackAlreadyCoolDown)
        {
        if(info_->ackCountDownTimer){
            JinMainThread::Get()->getTimer()->cancel(info_->ackCountDownTimer);
            info_->ackCountDownTimer = 0;
        }
        //时间为0的timer,下一个poll一定会触发.相当于我可以等网络这一波多个包收完.
        info_->ackCountDownTimer = JinMainThread::Get()->getTimer()->start\
                (0,false,sHandleTimeout,this);
        info_->ackAlreadyCoolDown = true;
        }
    }
    else
    {
        if(info_->ackCountDownTimer==0){
            info_->ackCountDownTimer = JinMainThread::Get()->getTimer()->start\
                    (kFlowControlMinAckInterval,false,sHandleTimeout,this);
        }
    }

    JinRoBuf data = parser->get(kUkData);
    if(data.len > 0)
    {
        //发现和解压.
        uint8_t cmp = parser->get8u(kUkCompress);
        if((info_->compressor==NULL && cmp!=0) || (cmp!=0 && cmp!='q'))
        {
            JELog("unexpect compress %c, obj=%p",cmp,info_->compressor);
            xlorb_->OnError(LeUnexpectCompress);
            return;
        }
        if(cmp == 'q')
        {
//#ifdef JDEBUG
//            size_t oldlen = data.len;
//            data = info_->compressor->decompress(data.buf,data.len);
//            JAssert(data.len > oldlen);
//#else
            data = info_->compressor->decompress(data.buf,data.len);
//#endif
            if(data.len == 0)
            {
                JELog("decompress fail");
                xlorb_->OnError(LeDecompressFail);
                return;
            }
        }

        if(info)
        {
#ifdef JDEBUG
            if(data.len<=32)
            {
                JinString s(data.buf,data.len);
                JDLog("debugB: %s",s.toReadableHex().c_str());
            }
#endif
            JinAsynBuffer abuf(data.len);
            memcpy(abuf.buff(),data.buf,data.len);
            info->sock->send(abuf,(uint32_t)data.len);
        }
    }
    return ;
}

void XOutward::ReplyAckTo(uint64_t ack)
{
    JinPackJoiner joiner;
    joiner.setHeadID(kMainF2A,kAsstFlowCtrl);
    joiner.pushNumber(kUkAck,ack);

    if(!xlorb_->DoSend((NetPacket*)joiner.buff(),joiner.length()-1, info_->targetid))
    {
        JELog("send reply ack error.");
        xlorb_->OnError(LeSendReplyAckErr);
    }
}

void XOutward::OnFinderRtnOpenTo(ProxyClientInfo *info, JinPackParser *parser)
{
    uint16_t ec = parser->get16u(kUkErrorCode);
    //if(ec != kEcSuccess || (info->state != WaitOpenTo))
    if(ec != kEcSuccess || info->state < Larger_IS_AlreadySend_OpenTo
            || info->state > Larget_IS_Already_OpenTo)
    {
        JELog("waitOpenTo occur error ec = %d, state = %d",ec,info->state);

        proxyAssist_.ReplySpecialCode(info,PRC_SERVER_ERROR);
        CloseIncoming(info);
        return;
    }
    //继续等待. do nothing
}

void XOutward::OnFinderConnDone(ProxyClientInfo *info, JinPackParser *parser)
{
    //if(info->state != WaitOpenTo)
    if(info->state < Larger_IS_AlreadySend_OpenTo || info->state > Larget_IS_Already_OpenTo)
    {
        proxyAssist_.ReplySpecialCode(info,PRC_SERVER_ERROR);
        CloseIncoming(info);
        return;
    }
    uint16_t ec = parser->get16u(kUkErrorCode);
    if(ec == kEcSuccess)
    {
        JPLog("Conn success. tunnid[%hu] target=%s|%hu", info->sock->tunnelID(),info->target.c_str(),info->port.port());
        if(info->state ==WaitOpenToLargePost ) info->state = ProxyToLargePost;
        else info->state = ProxyTo;
        if(info->proxyType == PTHTTPS)
        {
            proxyAssist_.ReplySpecialCode(info,PRC_OK);
        }
        if(info->proxyType == PTSOCK5 || info->proxyType == PTSOCK4)
        { //获取代理绑定的地址和端口告诉客户端后清一下.
            uint32_t netip = parser->get32u(kUkIPv4);
            uint16_t netport = parser->get16u(kUkPort);
            char spBuf[6];
            memcpy(spBuf,&netip,4);
            memcpy(spBuf+4,&netport,2);
            info->cached = JinString(spBuf,6);  //通过cached传递. 这里xlord理解了一些东西,有点不好.
            proxyAssist_.ReplySpecialCode(info,PRC_OK);
            info->cached.clear();
        }
        //把cache发出去
        //JAssert(info->cached.length() == 0); //既然断言,要能控制不出这种情况.
        //原来不支持未OpenTo又来数据,需要断言没有cache,现在支持了就会有.
        if(info->cached.length())
        {
            JDLog("handle more data=%d recv when not OpenTo.tunnid[%hu]",(int)info->cached.length(),
                  info->sock->tunnelID());
            JinString cached = info->cached;
            info->cached.clear();
            //this->OnRecv(info->sock,info,cached.c_str(),(uint32_t)cached.length());
            if(!proxyAssist_.OnProxyRecv(cached.c_str(),(uint32_t)cached.length(),info))
            {
                JELog("OnProxyRecv false, tunnid[%hu]",info->sock->tunnelID());
            }
        }
    }
    else
    {
        JinString eMsg = parser->getStr(kUkMessage);
        JPLog("Conn failed. ec=%hu eMsg=%s", ec, eMsg.c_str());
        proxyAssist_.ReplySpecialCode(info,PRC_BAD_GATEWAY);
        CloseIncoming(info);
    }
}

void XOutward::OnFinderConnAbort(ProxyClientInfo *info, JinPackParser *parser)
{
    JinString eMsg = parser->getStr(kUkMessage);
    JPLog("Conn abort. tunnid[%hu] target=%s eMsg=%s",info->sock->tunnelID(), info->target.c_str(), eMsg.c_str());
    if(info->state < Larget_IS_Already_OpenTo)
    {
        proxyAssist_.ReplySpecialCode(info,PRC_BAD_GATEWAY);
    }
    CloseIncoming(info);
}

void XOutward::OnFinderGreeting(ProxyClientInfo *info, JinPackParser *parser)
{
    //ClientMoreInfo* minfo = info->more;
    JinString identity = parser->getStr(kUkIdentity);
    if(identity.length()>0){
        JDLog("greeting rtn by %s",identity.c_str());
    }

    JinString cmp = parser->getStr(kUkCompress);
    if(cmp.length()>0)
    {
        JCompressType cmpType = kNoCompress;
        if(info_->compressor) {
            cmpType = info_->compressor->type();
        }
        size_t pos = STRNPOS;
        if(cmpType != kNoCompress)
        {   //判断现有compressor能否沿用.
            pos = cmp.find(JinString((char*)&cmpType,1),0,true);
            if(pos == STRNPOS)
            {
                JAssert(info_->compressor);
                JDelete(info_->compressor);
                info_->compressor = 0;
            }
        }
        if(pos == STRNPOS)
        {  //选择自己支持的类型创建对象.  --现只支持q,所以直接找q
            JAssert(info_->compressor == NULL);
            pos = cmp.find("q",0,true);
            if(pos != STRNPOS)
            {
                info_->compressor = JNew(JinCompress,kTypeQuickLz);
            }
        }
        if(info_->compressor == NULL)
        {
            xlorb_->OnError(LeCompreeNegotiationFail);
        }
    }
}

void XOutward::OnFinderFlowCtrl(uint64_t ack)
{
    JDLog("XLorb ack=%"PRIu64",last fcAcked=%"PRIu64",sended=%"PRIu64,
          ack, info_->fcAcked, info_->fcSended);
    if(ack <= info_->fcAcked || ack > info_->fcSended){
        JELog("bad ack.");
        xlorb_->OnError(LeBadFlowAckByA);
        return;
    }
    info_->fcAcked = ack;


    if(info_->pendDoRecv && info_->pendDoRecv->size()>0 &&
            info_->fcSended - info_->fcAcked < kFlowControlMaxCache)
    {
        SockPendMap::iterator it = info_->pendDoRecv->begin();
        while(it != info_->pendDoRecv->end())
        {
            XLorsSocket* lsock = it.first();
            lsock->doRecv();
            ++it;
        }
        info_->pendDoRecv->clear();
    }
}

void XOutward::ProxyDoSendData(ProxyClientInfo *info, const char *buf, uint32_t len)
{
    DoSendData(info->sock->tunnelID(),buf,len);
}

void XOutward::ProxyDoOpenTo(ProxyClientInfo *info, bool change)
{
    uint16_t tunnid = info->sock->tunnelID();
    if(change)
    {
        JAssert(info->target.length());
        CloseTunnel(tunnid);
        allIncoming_.remove(tunnid);
        do  //选取新的tunnid.
        {
            info_->tunnidAssigner = info_->tunnidAssigner+1;
            if(info_->tunnidAssigner == 0) info_->tunnidAssigner = 1;
            if(!allIncoming_.has(info_->tunnidAssigner)) break;
        }
        while(info_->tunnidAssigner);
        info->sock->setTunnelID(info_->tunnidAssigner);
        allIncoming_.set(info->sock->tunnelID(),info);
        JDLog("tunnid[%hu] <- new tunnid",info->sock->tunnelID());
        DoSendOpenTo(info->sock->tunnelID(),info->target,info->port.port(),info->cached);
    }
    else
    {
        DoSendOpenTo(info->sock->tunnelID(),info->target,info->port.port(),info->cached);
    }
}

void XOutward::ProxyDoReplyClient(ProxyClientInfo *info, const char *buf, uint32_t len, bool bClose)
{
    if(len > 0)
    {
        JinAsynBuffer abuf(len+1);
        memcpy(abuf.buffc(),buf,len);
        info->sock->send(abuf,len);
    }
    if(bClose)  CloseIncoming(info);
}

