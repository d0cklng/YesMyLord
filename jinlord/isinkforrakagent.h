#ifndef ISINKFORRAKAGENT_H
#define ISINKFORRAKAGENT_H
#include "jinnetaddress.h"
#include "INetEngine.h"

struct NetEngineMsg;
class ISinkForRakAgent
{
public:
    //包被筛选的话return false表示raknet不会再处理.
    virtual bool OnDatagramFilter(char* netpack, int len, uint32_t naddr, JinNetRawPort nport) = 0;

    //连出连接: 连接成功失败' 连接关闭' 收到数据.
    virtual void OnOutwardConn(bool isOK, const NetID &id, const JinNetAddr &addr, const JinNetPort &port) = 0;
    virtual void OnOutwardConnClose(const NetID &id, bool bGrace) = 0;
    virtual void OnOutwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                               NetEngineMsg *packet, const int lenFromNetMsg, int head) = 0;
    //连入连接:  连接关闭' 连入连接' 收到数据.
    virtual void OnInwardConnClose(const NetID &id, bool bGrace) = 0;
    virtual void OnInwardIncoming(const NetID &id, const JinNetAddr &addr, const JinNetPort &port) = 0;
    virtual void OnInwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                               NetEngineMsg *packet, const int lenFromNetMsg, int head) = 0;

};


#endif // ISINKFORRAKAGENT_H

