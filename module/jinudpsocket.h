#ifndef JINUDPSOCKET_H
#define JINUDPSOCKET_H
#include "jinformatint.h"
#include "jinerrorcode.h"
#include "jinasyncdef.h"
#include "jinnetaddress.h"

class JinUdpSocket : public IAsynObj
{
public:
    JinUdpSocket();
    virtual ~JinUdpSocket();
    int bind(const JinNetAddr& addr, const JinNetPort& port);
    int sendTo(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend,
               const JinNetAddr& addr, const JinNetPort& port);
    int recvFrom(JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv);
    void close();  //不会再引发回调，异步引擎内部消化.

    //为了和Asyn的HDNULL协调,INVALID_SOCKET作为中间态,会被立即改成SOCKINV
    inline bool isSocket(){return (hsocket_!=SOCKINV);}
    //获取socket所绑的地址，必须在bind之后，或者sendTo至少一次之后.
    bool getLocalAddr(JinNetAddr& addr, JinNetPort& port);
protected:
    int socket();
    virtual void OnSendTo(void *buf, uint32_t byteToSend,uint32_t byteSended,
                        const JinNetAddr& addr, const JinNetPort& port)
    {UNUSED(buf); UNUSED(byteToSend); UNUSED(byteSended); UNUSED(addr); UNUSED(port);}
    virtual void OnRecvFrom(void *buf, uint32_t byteRecved,
                        const JinNetAddr& addr, const JinNetPort& port)
    {UNUSED(buf); UNUSED(byteRecved); UNUSED(addr); UNUSED(port);}

    virtual void OnMainPull(bool isSuccess, HdObject handle, AsynOpType type, void *buf, uint64_t opSize,
                            uint64_t tranSize, void* user, uint64_t user64);  //主线程或者调用线程拉
private:
    int lastError_;
    //AsynOp aop_;
    bool isBind_;   //is bind

};

#endif // JINUDPSOCKET_H
