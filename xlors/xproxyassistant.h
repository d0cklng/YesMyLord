#ifndef XPROXYASSISTANT_H
#define XPROXYASSISTANT_H
#include "jinstring.h"
#include "jinassert.h"
#include "jinnetaddress.h"

enum LordProxyType{
    PTErrorSOCK5=-5,
    PTErrorSOCK4=-4,
    PTErrorHTTPS=-2,
    PTErrorHTTP=-1,
    PTContinueRecv=0,
    PTHTTP=1,
    PTHTTPS=2,
    PTFORWARD=3,  //一种正向搭桥.
    PTSOCK4=4,
    PTSOCK5=5,
};

/* SOCK5 主要的返回码
   '00' succeeded
   '01' general SOCKS server failure
   '02' connection not allowed by ruleset
   '03' Network unreachable
   '04' Host unreachable
   '05' Connection refused
   '06' TTL expired
   '07' Command not supported
   '08' Address type not supported
   '09' to X'FF' unassigned */
/* SOCK4 主要的返回码
    90: request granted
    91: request rejected or failed
    92: request rejected becasue SOCKS server cannot connect to
        identd on the client
    93: request rejected because the client program and identd
        report different user-ids.  */

enum IncomeSockState{WaitTarget,  //等待提供目标信息
                     HandShakeOK,    //Sock5有.
                     Larger_IS_AlreadySend_OpenTo=5,
                     WaitOpenTo,
                     WaitOpenToLargePost, //大数据POST时的特别状态
                     Larget_IS_Already_OpenTo=10,
                     ProxyToLargePost,   //大数据POST时的特别状态
                     ProxyTo,
                     _UNUSE_DoneOpenTo=20};
enum ProxyResponseCode
{
    PRC_OK,
    PRC_BAD_REQUEST,
    PRC_NOT_SUPPORT,
    PRC_SERVER_ERROR,
    PRC_BAD_GATEWAY,
    PRC_GATEWAY_TIMEOUT,
};
class XLorsSocket;
class ProxyClientInfo
{
public:
    IncomeSockState state;
    LordProxyType proxyType;
    JinString cached;
    JinString target;
    JinNetPort port;
    int contentLengthLeft;  //还有多少content-length没收完.
    XLorsSocket* sock;
};
class IProxyAssistCallBack
{
public:
    virtual void ProxyDoSendData(ProxyClientInfo* info,const char *buf, uint32_t len) = 0;
    virtual void ProxyDoOpenTo(ProxyClientInfo* info, bool change) = 0;
    virtual void ProxyDoReplyClient(ProxyClientInfo* info,const char *buf, uint32_t len, bool bClose=true) = 0;
};
class XProxyAssistant
{
public:
    XProxyAssistant(IProxyAssistCallBack* callback)
        : cb_(callback){JAssert(cb_);}
    ~XProxyAssistant(){}

    bool OnProxyRecv(const char *buf, uint32_t byteRecved,
                     ProxyClientInfo *info);  //返回true继续recv
    void ReplySpecialCode(ProxyClientInfo *info,ProxyResponseCode code);
protected:
    bool HandleProxyHead(ProxyClientInfo *info);
    bool HandleProxyHeadHTTP(ProxyClientInfo *info);
    bool HandleProxyHeadSOCK4(ProxyClientInfo *info);
    bool HandleProxyHeadSOCK5(ProxyClientInfo *info);
    LordProxyType typeDetect(JinString& cached_const);
    void ReplyClient(ProxyClientInfo *info,LordProxyType type, int code); //responseBuff_
    int GetHttpContentLength(const JinString &head, size_t &idx); //-1失败 0没找到 >0length
    bool GetHttpHeadHost(const JinString &head, LordProxyType proxytype, JinString &target, JinNetPort &port);
    int ChangeGETProxyHead(JinString &head, const JinString &target);  //改装代理请求头, 返回<0失败 1成功.
private:
    IProxyAssistCallBack* cb_;
    char responseBuff_[128];
};

#endif // XPROXYASSISTANT_H

