#include "xlors.h"
#include "jinassert.h"
#include "jinlogger.h"
#include "jinassistthread.h"
#include "jinpackhelper.h"
#include "jinfiledir.h"
#include "jinfile.h"

struct ClientMoreInfo
{
    //XLors *lorb;  //回调时作为user可以复用.
    //  cert
    //  pubkey
    char identity[kIdentityLength];  //字符串,因为有\0 最多63字节

};


XLors::XLors(JinNetAddr addr, JinNetPort port)
    : server_(NULL)
    , timer_(NULL)
    , timerid_(0)
    , quitcheck_(0)
    , netid_(NETID_INVALID)
    , isStart_(false)
{
    outside_ = addr;
    outport_ = port;
}

XLors::~XLors()
{
    stop();
}

bool XLors::start()
{
    if(isStart_)stop();
    JAssert(isStart_==false);
    JAssert(server_==NULL);
    char buf[32];
    server_ = INetEngine::GetInstance();
    if(!server_) return false;
    netid_ = server_->Init(this);

    unsigned short portBind = server_->Bind(outport_.port(),outside_.toAddr4(buf));
    JPLog("port:%hu bind.", portBind);
    if(portBind == 0)
    {
        INetEngine::DestroyInstance(server_);
        server_ = NULL;
        return false;
    }

    server_->AttachAsPunchthroughServer();
    server_->AttachAsUDPProxyCoordinator();

    timer_ = JinMainThread::Get()->getTimer();
    timerid_ = timer_->start(10,true,sHandleTimeout,this);
    quitcheck_ = timer_->start(5000,true,sHandleTimeout,this);

    isStart_ = true;

    return true;
}

void XLors::stop()
{
    if(!isStart_)return;
    if(timerid_) timer_->cancel(timerid_);
    timerid_ = 0;
    if(quitcheck_) timer_->cancel(quitcheck_);
    quitcheck_ = 0;
    allLogin_.clear();
    JinHashMap<NetID,ClientInfo*>::iterator it = allConnected_.begin();
    while(it!=allConnected_.end())
    {
        ClientInfo* info = it.second();
        allocator_.back(info);
        ++it;
    }
    INetEngine::DestroyInstance(server_);
    isStart_ = false;

}

void XLors::sHandleTimeout(TimerID id, void *user, uint64_t user64)
{
    XLors* lors = (XLors*)user;
    lors->handleTimeout(id);
}

void XLors::handleTimeout(TimerID id)
{
    //JAssert(id == timerid_);
    if(id == timerid_)
    {
        if(!server_)return;

        while(server_->ProcPacket(JinDateTime::tickCount()))
        {}
    }
    else if(id == quitcheck_)
    {
        CheckQuit();
    }
}

int XLors::OnNetMsg(INetEngine *src, NetID id, sockaddr_in addr4, NetEngineMsg *packet, const int lenFromNetMsg, int head)
{
    JAssert(src == server_);
    JinNetAddr addr = JinNetAddr::fromRawIP(addr4.sin_addr.s_addr);
    JinNetPort port = JinNetPort::fromRawPort(addr4.sin_port);
    JDLog("id=%"PRIu64",%s|%hu type=%hhu, LenInPack=%d",
          id,addr.toAddr(liteBuf),port.port(),
          packet->PacketType0,lenFromNetMsg);
    switch (packet->PacketType0)
    {
    case TYPE_APPLICATION_DATA:
    {
        if(!OnRecv(id,addr,port,packet,lenFromNetMsg,head))
        {
            JDLog("drop client because of bad recv.");
            doClose(id);
        }
    }
        break;
    case TYPE_CONNECTION_CLOSE:
    {
        OnClose(id,true);
    }
        break;
    case TYPE_CONNECTION_LOST:
    {
        OnClose(id,false);
    }
        break;
    case TYPE_CONNECTION_INCOMING:
    {
        if(!OnIncoming(id,addr,port))
        {
            JDLog("drop client because of id exist.");
            server_->Close(id);
        }
    }
        break;
    default:
        JDLog("drop client because of unexpect type.");
        doClose(id);
    }
    return -1;
}

void XLors::doClose(const NetID &id)
{
    server_->Close(id);
    OnClose(id,false);
}

bool XLors::doSend(NetPacket *pack, int DataLenInPack, NetID id)
{
    int headMixUp = DataLenInPack%250+1;
    return server_->Send(pack,DataLenInPack,SEND_NORMAL,PACK_DATAGRAM,0,id, false, headMixUp);
}

bool XLors::OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                   NetEngineMsg *packet, const int lenFromMainID, int head)
{
    //UNUSED(addr);UNUSED(port);UNUSED(head);
    ClientInfo *info = allConnected_.get(id, NULL);
    if(!info || lenFromMainID<0) //不可能<0
    {
        JELog("id=%"PRIu64" not connected.",id);
        return false;
    }

    JinPackParser parser((const char*)packet,lenFromMainID+1,1);
    if(!parser.isValid() || parser.mainID()!=kMainO2S)
    {
        JELog("id=%"PRIu64" packet parser error.",id);
        return false;
    }

    switch(parser.asstID())
    {
    case kAsstLogin:
        JDLog("id=%"PRIu64" get kAsstLogin",id);
        return OnOuterLogin(info,&parser);
        break;
    case kAsstFind:
        JDLog("id=%"PRIu64" get kAsstFind",id);
        return OnOuterFind(info,&parser);
        break;
    default:
        JELog("can't handle asstid:%hu",parser.asstID());
    }

    return true;
}

bool XLors::OnIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port)
{
    JDLog("income addr=%s|%hu id=%"PRIu64,addr.toAddr4(gDLogBufAddr),port.port(),id);
    if(allConnected_.has(id)) return false;
    ClientInfo* info = allocator_.alloc();
    info->state = ClientInfo::kConn;
    info->fromAddr = addr;
    info->fromPort = port;
    info->id = id;
    info->firstGlance = JinDateTime::tickCount();
    allConnected_.set(id,info);
    return true;
}

void XLors::OnClose(const NetID &id, bool grace)
{
    JDLog("closing id=%"PRIu64", grace=%d",id,grace?1:0);
    ClientInfo *info = allConnected_.get(id, NULL);
    if(info)
    {
        JAssert(info->id == id);
        JDLog("closing addr=%s|%hu id=%"PRIu64,
              info->fromAddr.toAddr4(gDLogBufAddr),info->fromPort.port(),id);
        if(info->more)
        {
            bool rmRet = allLogin_.remove(info->more->identity);
            JAssert(rmRet);
            JFree(info->more);
            info->more = NULL;
        }
        allConnected_.remove(id);
        allocator_.back(info);
    }
}

bool XLors::OnOuterLogin(ClientInfo *info, JinPackParser *parser)
{
    //JinString cert = parser->getStr(kUkCert);
    //JinString pubkey = parser->getStr(kUkPubKey);
    JinString identity = parser->getStr(kUkIdentity);

    //TODO verify cert & pubkey
    if(false)
    {
        ReplyLogin(info,kEcCertFailed);
    }

    if(identity.length()>=kIdentityLength || identity.length()==0)
    {
        const char* eMsg = "identity too long.";
        if(identity.length()==0) eMsg = "identity empty.";
        return ReplyLogin(info,kEcIdentityInvalid,"identity too long.");
    }
    if(allLogin_.has(identity.c_str()))
    {
        //return ReplyLogin 表示其实不断连接,除非发送本身出错.
        return ReplyLogin(info,kEcIdentityExist,"Identity already exist.");
    }

    if(info->more == NULL)
    {
        info->more = (ClientMoreInfo*)JMalloc(sizeof(ClientMoreInfo));
    }
    if(info->more == NULL)
    {
        return ReplyLogin(info,kEcOutOfMemory);
    }
    strcpy(info->more->identity,identity.c_str());
    allLogin_.set(info->more->identity,info);

    return ReplyLogin(info,kEcSuccess);
}

bool XLors::ReplyLogin(ClientInfo *info, XlorsErrorCode ec, const char *eMsg)
{
    JDLog("identity=%s, ec=%d, eMsg=%s",info->more->identity,(int)ec,eMsg?eMsg:"");
    JinPackJoiner joiner;
    joiner.setHeadID(kMainS2O,kAsstRtnLogin);
    joiner.pushNumber(kUkIPv4,info->fromAddr.rawIP());
    joiner.pushNumber(kUkPort,info->fromPort.port());
    joiner.pushNumber(kUkErrorCode,(uint16_t)ec);
    if(eMsg) joiner.pushString(kUkMessage,eMsg);

    return doSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id);

}

bool XLors::OnOuterFind(ClientInfo *info, JinPackParser *parser)
{
    JinString identity = parser->getStr(kUkIdentity);
    JDLog("find identity = %s",identity.c_str());

    if(identity.length()>=kIdentityLength || identity.length()==0)
    {
        const char* eMsg = "identity too long.";
        if(identity.length()==0) eMsg = "identity empty.";
        return ReplyFind(info,NULL,kEcIdentityInvalid,"identity too long.");
    }

    ClientInfo *found = allLogin_.get(identity.c_str(),NULL);
    if(!found)
    {
        return ReplyFind(info,NULL,kEcIdentityNotFound,"identity not found.");
    }
    return ReplyFind(info,found,kEcSuccess);
}

bool XLors::ReplyFind(ClientInfo *info, ClientInfo *found, XlorsErrorCode ec, const char *eMsg)
{
    JDLog("ec=%d, eMsg=%s",(int)ec,eMsg?eMsg:"");
    JinPackJoiner joiner;
    joiner.setHeadID(kMainS2O,kAsstRtnFind);
    if(ec == kEcSuccess)
    {
        joiner.pushNumber(kUkIPv4,found->fromAddr.rawIP());
        joiner.pushNumber(kUkPort,found->fromPort.port());
        joiner.pushNumber(kUkNetID,found->id);
    }
    joiner.pushNumber(kUkErrorCode,(uint16_t)ec);
    if(eMsg) joiner.pushString(kUkMessage,eMsg);

    return doSend((NetPacket*)joiner.buff(),joiner.length()-1, info->id);
}

void XLors::CheckQuit()
{
    JinFileDir tmpDir;
    tmpDir.setWith(kDirHome);
    const char* quitflag = tmpDir.fullPath("XLors.quit");
    if(JinFile::exist(quitflag))
    {
        JinFile::remove(quitflag);
        JinMainThread::notifyQuit();
    }
}


