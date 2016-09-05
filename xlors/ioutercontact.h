#ifndef IOUTERFINDER_H
#define IOUTERFINDER_H

#include "jinnetaddress.h"
#include "jinassert.h"
#include "INetEngine.h"
#include "xlors_share.h"
#include "jinstring.h"

static const int kReconnJinzeyuTime = 10000;  //首次10s
static const int kReconnJinzeyuTimeMore = 30000; //以后30s


#ifdef XLORD
class ISinkForOuterFinder
{
public:
    virtual void OnFind(const NetID &id, JinNetAddr &addr, JinNetPort &port) = 0;
    virtual void OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                        NetEngineMsg *packet, const int lenFromNetMsg, int head) = 0;
    virtual void OnClose(const NetID &id, bool grace) = 0;
};
//用来找目标。目前的设想是一种通过jinzeyu.cn用raknet
//一种通过dht。
class IOuterFinder
{
public:
    static IOuterFinder* CreateFinder(ISinkForOuterFinder* cb, const JinString &selfIdentify);
    static void DestroyFinder(IOuterFinder* oa);
public:
    virtual bool Start(const JinNetAddr &addr) = 0;
    virtual void Stop() = 0;

    //寻找并联络具有特定identity的目标
    virtual void Find(const char* identity) = 0;
    virtual const char* CurrentTarget() = 0;

    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0) = 0;
    //virtual bool DoClose(const NetID &id) = 0;

protected:
    IOuterFinder(ISinkForOuterFinder* cb)
        : callback_(cb), isStart_(false){ }
    virtual ~IOuterFinder(){ JAssert(isStart_==false); }
protected:
    ISinkForOuterFinder *callback_;
    bool isStart_;
};
#endif

#ifdef XLORB
class ISinkForOuterAdmit
{
public:
    virtual bool OnIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port) = 0;
    virtual bool OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                        NetEngineMsg *packet, const int lenFromNetMsg, int head) = 0;
    virtual void OnClose(const NetID &id, bool grace) = 0;
};

//用来被找到
class IOuterAdmit
{
public:
    static IOuterAdmit* CreateAdmit(ISinkForOuterAdmit* cb, const JinString &selfIdentify);
    static void DestroyAdmit(IOuterAdmit* oa);
public:
    virtual bool Start(const JinNetAddr &addr) = 0;
    virtual void Stop() = 0;
    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0) = 0;
    virtual bool DoClose(const NetID &id) = 0;
protected:
    IOuterAdmit(ISinkForOuterAdmit* cb)
        : callback_(cb), isStart_(false){ }
    virtual ~IOuterAdmit(){ JAssert(isStart_==false); }

    ISinkForOuterAdmit *callback_;
    bool isStart_;
};
#endif

#endif // IOUTERFINDER_H
