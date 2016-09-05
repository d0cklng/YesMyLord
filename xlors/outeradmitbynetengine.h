#ifndef OUTERADMITBYNETENGINE_H
#define OUTERADMITBYNETENGINE_H
#include "ioutercontact.h"
#include "jintimer.h"
#include "clientinfoallocator.h"
#include "ISinkForNetEngine.h"
#include "jinstring.h"

//这个实现，就是登记到服务器，然后等别人来连我，和云3云4相似.
class OuterAdmitByNetEngine : public IOuterAdmit
        , public ISinkForNetEngine
{
public:
    OuterAdmitByNetEngine(ISinkForOuterAdmit* cb, const JinString &selfIdentify);
    ~OuterAdmitByNetEngine();

    virtual bool Start(const JinNetAddr &addr);
    virtual void Stop();
    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0);
    virtual bool DoClose(const NetID &id);
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

    JinHashMap<NetID,ClientInfo*> allConnected_;
private:
    JinNetAddr outside_;
    INetEngine *admit_;
    JinTimer* timer_;
    TimerID timerid_;
    NetID netid_;
    NetID jinzeyucn_;
    ConnID connjzycn_;
    TimerID reconntid_;
    JinString  selfIdent_;
};

#endif // OUTERADMITBYNETENGINE_H
