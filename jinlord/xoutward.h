#ifndef XLORD_H
#define XLORD_H
#include "jinnetaddress.h"
#include "INetEngine.h"
#include "jintimer.h"
#include "jindatatime.h"
#include "xlorddef.h"
#include "lorderror.h"
#include "isinkforlord.h"
#include "xproxyassistant.h"
#ifndef CallbackInfoType
typedef ProxyClientInfo *CallbackInfo;
#define CallbackInfoType
#endif
#include "xlorssocket.h"
struct XOutwardOuterInfo;
class XLorsSocket;
class JinPackParser;
typedef JinLinkedList<XLorsSocket*> SocketList;

class XOutwardForwardInfo
{
public:
    XLorsSocket listen;
    JinString targetAddr;
    uint16_t targetPort;
};


class XOutward : public ISinkForLorsSocket
        ,public IProxyAssistCallBack
{
public:
    XOutward(const JinNetAddr &bindaddr, const JinNetPort &bindport,
          const JinString &selfIdentify);
    ~XOutward();
    bool start(IXLorb* xlorb);
    void stop();
    //增加一个固定forward到目的地址的绑定端口,用于不支持代理的程序通道.
    bool addForwarding(uint16_t bindport,const JinString &targetaddr,uint16_t targetport);
    //removeForwarding

    LordError OnFind(const NetID &id, const JinNetAddr &addr, const JinNetPort &port);
    void OnRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                NetEngineMsg *packet, const int lenFromNetMsg, int head);
    void OnClose(const NetID &id, bool grace);
protected:
    static void sHandleTimeout(TimerID id, void *user, uint64_t user64);
    void handleTimeout(TimerID id);
    void DoRecvUnderFlow(XLorsSocket *sock);
    void CloseTunnel(uint16_t tunnid);
    void CloseIncoming(ProxyClientInfo* info);

    void DoSendData(uint16_t tunnid,const char* buf, uint32_t len);
    void DoSendOpenTo(uint16_t tunnid,const JinString &target,uint16_t port,const JinString &data);
protected:
    //ISinkForLorsSocket
    virtual void OnConnect(XLorsSocket *sock, CallbackInfo info, bool success, const JinNetAddr& addr, const JinNetPort& port)
    { JAssert("impossible!"==NULL); }
    virtual void OnRecv(XLorsSocket *sock, CallbackInfo info, const char *buf, uint32_t byteRecved);
    virtual void OnDisconnect(XLorsSocket *sock, CallbackInfo info, bool byRemote);
    virtual void OnAccept(XLorsSocket* sock, CallbackInfo cbinfo, HdSocket accepted,
                          const JinNetAddr& remoteAddr, const JinNetPort& remotePort);

    void OnFinderDataTransfer(ProxyClientInfo *info, JinPackParser *parser, size_t len);
    void ReplyAckTo(uint64_t ack);
    void OnFinderRtnOpenTo(ProxyClientInfo *info, JinPackParser *parser);
    void OnFinderConnDone(ProxyClientInfo *info, JinPackParser *parser);
    void OnFinderConnAbort(ProxyClientInfo *info, JinPackParser *parser);
    void OnFinderGreeting(ProxyClientInfo *info, JinPackParser *parser);
    void OnFinderFlowCtrl(uint64_t ack);

    virtual void ProxyDoSendData(ProxyClientInfo* info,const char *buf, uint32_t len);
    virtual void ProxyDoOpenTo(ProxyClientInfo* info, bool change);
    virtual void ProxyDoReplyClient(ProxyClientInfo* info,const char *buf, uint32_t len, bool bClose=true);

private:
    XOutwardOuterInfo *info_;
    XLorsSocket* listen_;
    JinHashMap<uint16_t, ProxyClientInfo*> allIncoming_;
    JinHashMap<uint16_t, XOutwardForwardInfo*> allForwarding_; //用于forward的端口绑定.
    IXLorb *xlorb_;
    JinNetAddr bindAddr_;
    JinNetPort bindPort_;
    JinString selfIdentity_;
    bool isStart_;
    XProxyAssistant proxyAssist_;
};

#endif // XLORB_H
