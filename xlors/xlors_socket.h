#ifndef XLORSSOCKET_H
#define XLORSSOCKET_H

#include "jinasyncdef.h"
#include "jintcpsocket.h"
#include "jinhashmap.h"
#include "jinstring.h"

//enum PlowAbortReason
//{
//    kParInnerRecv=10,
//    kParInnerSend1,
//    kParInnerConn,
//    kParInnerSend,
//    kParInnerDisconn,
//    kParOuterRecv=20,
//    kParOuterSend,
//    kParOuterDisconn,
//};
class XLorsSocket;
#ifndef CallbackInfoType
typedef void *CallbackInfo;
#define CallbackInfoType
#endif
class ISinkForLorsSocket
{
public:
    virtual void OnConnect(XLorsSocket* sock, CallbackInfo cbinfo, bool success, const JinNetAddr& addr, const JinNetPort& port) = 0;
    virtual void OnRecv(XLorsSocket* sock, CallbackInfo cbinfo, const char *buf, uint32_t byteRecved) = 0;
    virtual void OnDisconnect(XLorsSocket* sock, CallbackInfo cbinfo, bool byRemote) = 0;

    virtual void OnAccept(XLorsSocket* sock, CallbackInfo cbinfo, HdSocket accepted,
                          const JinNetAddr& remoteAddr, const JinNetPort& remotePort){}
};

//相比PlowSocket，是合体了PlowIn和PlowOut
//这个对象不操心new delete,所以不要在里面JNew
class XLorsSocket : public JinTcpSocket
{
public:
    XLorsSocket();  //常规构造，需要进行连接后工作.
    void setTunnelID(uint16_t tunnid){tunnid_ = tunnid;}
    uint16_t tunnelID(){ return tunnid_;}
    void setCallbackInfo(CallbackInfo info){ cbinfo_ = info; }
    int doConnect(JinString target, JinNetPort port); //支持域名.
    int doDisconnect();  //处理域名,若正在解析,处理和tcpsocket不一样.
    static void sOnDnsParsed(const char* domain, JinNetAddr addr, void* user, uint64_t user64);
    void OnDnsParsed(const char* domain, JinNetAddr addr, uint64_t user64);
    //accept构造，已经是连接态.
    XLorsSocket(HdSocket sock, const JinNetAddr& remoteAddr, const JinNetPort& remotePort);
    ~XLorsSocket();
    void setCallback(ISinkForLorsSocket *cb){cb_ = cb;}
    int send(JinAsynBuffer& buffer, uint32_t byteToSend);
    void doRecv();
protected:
    virtual void OnConnect(bool success, const JinNetAddr& addr, const JinNetPort& port);
    virtual void OnRecv(void *buf, uint32_t byteRecved);
    virtual void OnDisconnect(bool byRemote);

    virtual void OnAccept(HdSocket accepted, const JinNetAddr& remoteAddr, const JinNetPort& remotePort)
    {   cb_->OnAccept(this,cbinfo_,accepted,remoteAddr,remotePort);  }
private:
    bool isConnOk_;
    void *dnsParseDoing_;
    uint16_t tunnid_;  //其实就是存一下 不需map找麻烦.
    CallbackInfo cbinfo_;
    //当前这个类对象，一种可能是连用户，一种是inner连目标。
    //当与用户协作，创建时即已连接，需要send时即可send，无需cache
    //当与target协作，创建后还需连到目标，所以如果send需cache
    JinString cacheBeforeConn;
    ISinkForLorsSocket* cb_;
    JinAsynBuffer rbuf_;
#ifdef JDEBUG
    bool OnlyOneRecvOnceFlag;  //确保只有一个recv被投递的调试字段
#endif
};


#endif // XLORSSOCKET_H
