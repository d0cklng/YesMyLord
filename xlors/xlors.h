#ifndef XPLOW_H
#define XPLOW_H
#include "jinnetaddress.h"
#include "INetEngine.h"
#include "ISinkForNetEngine.h"
#include "jintimer.h"
#include "jinhashmap.h"
#include "jindatatime.h"
struct ClientMoreInfo;
#ifndef ClientInfoMoreType
typedef ClientMoreInfo *InfoMoreType;
#define ClientInfoMoreType
#endif
#include "clientinfoallocator.h"
#include "xlors_share.h"

class JinPackParser;
class XLors : public ISinkForNetEngine
{
public:
    XLors(JinNetAddr addr, JinNetPort port);
    ~XLors();
    bool start();
    void stop();
protected:
    static void sHandleTimeout(TimerID id, void *user, uint64_t user64);
    void handleTimeout(TimerID id);
    virtual int OnNetMsg(INetEngine* src,NetID id,
                         struct sockaddr_in addr4,
                         NetEngineMsg *packet,
                         const int lenFromNetMsg,
                         int head);
    void doClose(const NetID &id);
    bool doSend(NetPacket* pack, int DataLenInPack, NetID id);
protected:
    bool OnRecv(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                NetEngineMsg *packet, const int lenFromMainID, int head);
    bool OnIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port);
    void OnClose(const NetID &id, bool grace);

    bool OnOuterLogin(ClientInfo *info, JinPackParser *parser);
    bool ReplyLogin(ClientInfo* info, XlorsErrorCode ec, const char* eMsg=NULL);
    bool OnOuterFind(ClientInfo *info, JinPackParser *parser);
    bool ReplyFind(ClientInfo* info, ClientInfo *found,
                   XlorsErrorCode ec, const char* eMsg=NULL);
    void CheckQuit();
    static bool IsIdentityEqual(const char * const &a , const char * const &b)
    {
        return (strcmp(a,b)==0);
    }
    JinHashMap<NetID,ClientInfo*> allConnected_;
    JinHashMap<const char*, ClientInfo*, JinHashDefault<const char*>, IsIdentityEqual > allLogin_;
    ClientInfoAllocator allocator_;
    char liteBuf[32];
private:
    JinNetAddr outside_;
    JinNetPort outport_;
    INetEngine *server_;
    JinTimer* timer_;
    TimerID timerid_;
    TimerID quitcheck_;
    NetID netid_;
    bool isStart_;
};

#endif // XPLOW_H
