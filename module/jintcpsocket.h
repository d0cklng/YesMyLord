#ifndef JINTCPSOCKET_H
#define JINTCPSOCKET_H
#include "jinformatint.h"
#include "jinerrorcode.h"
#include "jinasyncdef.h"
#include "jinnetaddress.h"

#ifdef JWIN
#define MAXListenConn SOMAXCONN
#else
#define MAXListenConn (5)
#endif

class JinTcpSocket : public IAsynObj
{
public:
    JinTcpSocket();
    JinTcpSocket(HdSocket sock, const JinNetAddr& remoteAddr, const JinNetPort& remotePort,
                 const JinNetAddr& localAddr = JinNetAddr(), const JinNetPort& localPort = JinNetPort());
    virtual ~JinTcpSocket();
    int bind(const JinNetAddr& addr, const JinNetPort& port);
    int listen(int backlog = MAXListenConn);
    int connect(const JinNetAddr &addr, const JinNetPort& port);
    int accept();
    int send(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend);
    int recv(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv);
    int disconnect();
    void close();  //不会再引发回调，异步引擎内部消化.

    //为了和Asyn的HDNULL协调,INVALID_SOCKET作为中间态,会被立即改成SOCKINV
    inline bool isSocket(){return (hsocket_!=SOCKINV);}
    //获取socket所绑的地址，必须在bind之后，或者connect之后.
    bool getLocalAddr(JinNetAddr& addr, JinNetPort& port);
    //获取对端的地址，必须在connect成功之后，或者是用第二种构造函数创建的
    //accept型socket总是返回false
    bool getRemoteAddr(JinNetAddr& addr, JinNetPort& port);
protected:
    int socket();
    virtual void OnAccept(HdSocket accepted, const JinNetAddr& remoteAddr, const JinNetPort& remotePort)
    {UNUSED(accepted); UNUSED(remoteAddr); UNUSED(remotePort);}
    virtual void OnConnect(bool success, const JinNetAddr& addr, const JinNetPort& port)
    {UNUSED(success); UNUSED(addr); UNUSED(port);}
    virtual void OnSend(void *buf, uint32_t byteToSend,uint32_t byteSended)
    {UNUSED(buf); UNUSED(byteToSend); UNUSED(byteSended);}
    virtual void OnRecv(void *buf, uint32_t byteRecved)
    {UNUSED(buf); UNUSED(byteRecved);}
    virtual void OnDisconnect(bool byRemote)
    {UNUSED(byRemote);}

    virtual void OnMainPull(bool isSuccess, HdObject handle, AsynOpType type, void *buf, uint64_t opSize,
                            uint64_t tranSize, void* user, uint64_t user64);  //主线程或者调用线程拉

private:
    int lastError_;
    //AsynOp aop_;
    JinNetAddr localAddr_;
    JinNetPort localPort_;
    HdSocket accSock_;  //记录已投递、用于接收连接的预创建socket
    bool isBind_;   //is bind
    bool isEstablish_;  //is connection establish
    JinNetAddr remoteAddr_;
    JinNetPort remotePort_;
};

#endif // JINTCPSOCKET_H
