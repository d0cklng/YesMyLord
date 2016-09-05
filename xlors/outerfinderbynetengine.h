#ifndef OUTERFINDERBYNETENGINE_H
#define OUTERFINDERBYNETENGINE_H
#include "ioutercontact.h"
#include "jintimer.h"
#include "clientinfoallocator.h"
#include "ISinkForNetEngine.h"
#include "xlors_share.h"
#include "jinstring.h"
#include "jinupnpc.h"


//这个实现，就是登记到服务器，然后等别人来连我，和云3云4相似.
class OuterFinderByNetEngine : public IOuterFinder
        , public ISinkForNetEngine
{
public:
    OuterFinderByNetEngine(ISinkForOuterFinder* cb, const JinString &selfIdentify);
    ~OuterFinderByNetEngine();

    virtual bool Start(const JinNetAddr &addr);
    virtual void Stop();
    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0);
    //virtual bool DoClose(const NetID &id);

    virtual void Find(const char* identity);
    virtual const char* CurrentTarget();
protected:
    virtual int OnNetMsg(INetEngine* src,NetID id,
                         struct sockaddr_in addr4,
                         NetEngineMsg *packet,
                         const int lenFromNetMsg,
                         int head);
    static void sHandleTimeout(TimerID id, void *user, uint64_t user64);
    void handleTimeout(TimerID id);
    bool OnXlorsConn();
    void OnXlorsRecv(NetEngineMsg *packet, const int lenFromMainID);

    void tryDoFinderWork();
    JinHashMap<NetID,ClientInfo*> allConnected_;
private:
    JinNetAddr outside_;
    INetEngine *finder_;
    JinTimer* timer_;
    TimerID timerid_;
    NetID netid_;
    NetID jinzeyucn_;
    ConnID connjzycn_;
    NetID targetid_;
    ConnID conntarget_;
    TimerID reconntid_;
    JinString selfIdent_;
    char targetidentify_[kIdentityLength];
    enum FinderState{WaitXlorsConn,  //等待连接完成.
                     XlorsAlreadyConn, //连接完成,没有其他工作要做.
                     TargetFinding,   //正在找.
                     TargetConnecting,  //正在连目标.
                     Xlors_server_depend = 10,
                     TargetOnWork} state_;
    JinUPnPc upnpc_;
};

#endif // OUTERFINDERBYNETENGINE_H
