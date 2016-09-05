#ifndef XLORD_H
#define XLORD_H
#include "jinnetaddress.h"
#include "INetEngine.h"
#include "jintimer.h"
#include "jindatatime.h"
#include "ioutercontact.h"
#include "xlors_share.h"
#include "xproxyassistant.h"
#ifndef CallbackInfoType
typedef ProxyClientInfo *CallbackInfo;
#define CallbackInfoType
#endif
#include "xlors_socket.h"
struct XLordOuterInfo;
class XLorsSocket;
class JinPackParser;
typedef JinLinkedList<XLorsSocket*> SocketList;

class XLordForwardInfo
{
public:
    XLorsSocket listen;
    JinString targetAddr;
    uint16_t targetPort;
};


class XLord : public ISinkForOuterFinder, public ISinkForLorsSocket
        ,public IProxyAssistCallBack
{
public:
    XLord(const JinNetAddr &bindaddr, const JinNetPort &bindport,
          const JinString &selfIdentify, const JinString &identity);
    ~XLord();
    bool start();
    void stop();
    //增加一个固定forward到目的地址的绑定端口,用于不支持代理的程序通道.
    bool addForwarding(uint16_t bindport,const JinString &targetaddr,uint16_t targetport);
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


    //ISinkForOuterFinder
    virtual void OnFind(const NetID &id, JinNetAddr &addr, JinNetPort &port);
    virtual void OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                        NetEngineMsg *packet, const int lenFromNetMsg, int head);
    virtual void OnClose(const NetID &id, bool grace);
    void CheckQuit();
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
    XLordOuterInfo *info_;
    XLorsSocket* listen_;
    JinHashMap<uint16_t, ProxyClientInfo*> allIncoming_;
    JinHashMap<uint16_t, XLordForwardInfo*> allForwarding_; //用于forward的端口绑定.
    IOuterFinder* outerFinder_;
    JinNetAddr bindAddr_;
    JinNetPort bindPort_;
    JinString identity_;
    JinString selfIdentity_;
    bool isStart_;
    TimerID quitcheck_;
    XProxyAssistant proxyAssist_;
};

#endif // XLORB_H
