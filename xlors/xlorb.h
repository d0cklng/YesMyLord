#ifndef XLORB_H
#define XLORB_H
#include "jinnetaddress.h"
#include "INetEngine.h"
#include "jintimer.h"
#include "jindatatime.h"
#include "clientinfoallocator.h"
#include "ioutercontact.h"
#include "xlors_share.h"
#ifndef CallbackInfoType
typedef ClientInfo *CallbackInfo;
#define CallbackInfoType
#endif
#include "xlors_socket.h"

struct XLorsSet;
struct ClientMoreInfo;
class XLorsSocket;
class JinPackParser;
//xlord是一个位于前线 inner的对象。一头连用户，一头向目标
class XLorb : public ISinkForOuterAdmit, public ISinkForLorsSocket
{
public:
    XLorb(const JinNetAddr &inside,const JinNetAddr &outside,
          const JinString &selfIdentify);
    ~XLorb();
    bool start();
    void stop();
protected:
    static void sHandleTimeout(TimerID id, void *user, uint64_t user64);
    void handleTimeout(TimerID id, ClientInfo* info);
    void DoCloseAdmitID(ClientInfo *info, const NetID &id);
    void DoRecvUnderFlow(XLorsSocket *sock, CallbackInfo info);
protected:
    //ISinkForLorsSocket
    virtual void OnConnect(XLorsSocket *sock, CallbackInfo info, bool success, const JinNetAddr& addr, const JinNetPort& port);
    virtual void OnRecv(XLorsSocket *sock, CallbackInfo info, const char *buf, uint32_t byteRecved);
    virtual void OnDisconnect(XLorsSocket *sock, CallbackInfo info, bool byRemote);

    //ISinkForOuterAdmit
    virtual bool OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                NetEngineMsg *packet, const int lenFromMainID, int head);
    virtual bool OnIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port);
    virtual void OnClose(const NetID &id, bool grace);
    void CheckQuit();
    bool OnAdmitDataTransfer(ClientInfo *info, JinPackParser *parser, size_t len);
    bool ReplyAckTo(ClientInfo *info, uint64_t ack);
    bool OnAdmitOpenTo(ClientInfo *info, JinPackParser *parser);  //返回false会导致"严重"断开
    bool ReplyOpenTo(ClientInfo* info, uint16_t tunnid, XlorsErrorCode ec, const char* eMsg=NULL);  //回应OpenTo
    bool OnAdmitCloseTunnel(ClientInfo *info, JinPackParser *parser);
    bool OnAdmitGreeting(ClientInfo *info, JinPackParser *parser);
    bool ReplyGreeting(ClientInfo* info, char cmpChr);
    bool OnAdmitFlowCtrl(ClientInfo *info, JinPackParser *parser);
private:
    JinHashMap<NetID,ClientInfo*> allConnected_;
    ClientInfoAllocator allocator_;
    XLorsSet *lorsSet;
    IOuterAdmit *outerAdmit_;
    JinNetAddr inside_;
    JinNetAddr outside_;
    JinString selfIdentity_;
    bool isStart_;
    TimerID quitcheck_;
};

#endif // XLORB_H
