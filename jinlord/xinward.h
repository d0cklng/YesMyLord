#ifndef XINWARD_H
#define XINWARD_H
#include "jinnetaddress.h"
#include "INetEngine.h"
#include "jintimer.h"
#include "jindatatime.h"
#include "clientinfoallocator.h"
#include "xlorddef.h"
#include "isinkforlord.h"
#ifndef CallbackInfoType
typedef ClientInfo *CallbackInfo;
#define CallbackInfoType
#endif
#include "xlorssocket.h"

struct XLorsSet;
struct ClientMoreInfo;
class XLorsSocket;
class JinPackParser;
//xlord是一个位于前线 inner的对象。一头连用户，一头向目标
class XInward : public ISinkForLorsSocket
{
public:
    XInward(const JinNetAddr &use4realTarget,
          const JinString &selfIdentify);
    ~XInward();
    bool start(IXLorb* xlorb);
    void stop();

    bool OnRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
        NetEngineMsg *packet, const int lenFromMainID, int head);
    bool OnIncoming(const NetID &id, const JinNetAddr &addr, const JinNetPort &port);
    void OnClose(const NetID &id, bool grace);
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
    IXLorb *xlorb_;
    JinNetAddr forTarAddr_;
    JinString selfIdentity_;
    bool isStart_;
};

#endif // XLORB_H
