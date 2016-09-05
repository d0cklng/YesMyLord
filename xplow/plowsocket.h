#ifndef PLOWSOCKET_H
#define PLOWSOCKET_H
#include "jinasyncdef.h"
#include "jintcpsocket.h"
#include "jinhashmap.h"
#include "jinstring.h"

class PlowInSocket;
class PlowOutSocket;
enum PlowAbortReason
{
    kParInnerRecv=10,
    kParInnerSend1,
    kParInnerConn,
    kParInnerSend,
    kParInnerDisconn,
    kParOuterRecv=20,
    kParOuterSend,
    kParOuterDisconn,
};

class IPlowConnAbort
{
public:
    virtual void OnConnAbort(JinTcpSocket* outer, JinTcpSocket* inner, int reason) = 0;
};

class PlowInSocket : public JinTcpSocket
{
public:
    PlowInSocket(JinTcpSocket* out);
    virtual ~PlowInSocket();
    int send(JinAsynBuffer& buffer, uint32_t byteToSend);
    void setCallback(IPlowConnAbort *cb){cb_ = cb;}
    void doRecv();  //开始接收,后续自动续着不用干预.
protected:
    virtual void OnConnect(bool success, const JinNetAddr& addr, const JinNetPort& port);
    virtual void OnRecv(void *buf, uint32_t byteRecved);
    virtual void OnDisconnect(bool byRemote);
private:
    JinString cacheBeforeConn;
    JinTcpSocket* out_;
    bool isConnOk_;
    IPlowConnAbort* cb_;
    JinAsynBuffer rbuf_;
};

class PlowOutSocket : public JinTcpSocket
{
public:
    PlowOutSocket(HdSocket sock, const JinNetAddr& remoteAddr, const JinNetPort& remotePort);
    virtual ~PlowOutSocket();
    void setInside(PlowInSocket* in);
    void setCallback(IPlowConnAbort *cb){cb_ = cb;}
    void doRecv();  //开始接收,后续自动续着不用干预.
protected:
    virtual void OnRecv(void *buf, uint32_t byteRecved);
    virtual void OnDisconnect(bool byRemote);
private:
    PlowInSocket* in_;
    IPlowConnAbort* cb_;
    JinAsynBuffer rbuf_;
};

class PlowListenSocket : public JinTcpSocket, public IPlowConnAbort
{
public:
    PlowListenSocket();
    virtual ~PlowListenSocket();
    void setAddr(JinNetAddr inside, JinNetPort srvport, JinNetAddr srvaddr);
protected:
    virtual void OnAccept(HdSocket accepted, const JinNetAddr& remoteAddr, const JinNetPort& remotePort);
    virtual void OnConnAbort(JinTcpSocket* outer, JinTcpSocket* inner, int reason);
    void handlePendDestroy();
private:
    JinNetAddr inside_; //内部socket使用的地址.
    JinNetPort srvport_; //目标server的端口.
    JinNetAddr srvaddr_; //目标server的地址.
    JinHashMap<PlowOutSocket*,PlowInSocket*> plowMap_;
    JinLinkedList<JinTcpSocket*> pendDestroy_;
};

#endif // PLOWSOCKET_H
