#ifndef JINLORD_H
#define JINLORD_H

#include "jintimer.h"
#include "jinnetaddress.h"
#include "jinstring.h"
#include "isinkforrakagent.h"
#include "isinkforlord.h"
#include "jinipc.h"

#define NAME_OF_JINLORD_IPC "jinlord160301"
#define PASSWORD_OF_JINLORD_IPC "bicycle88090"
#define PASSWORD_LENGTH_OF_JINLORD_IPC  (sizeof(PASSWORD_OF_JINLORD_IPC)-1)
class RakAgent;
class JinChart;
class XOutward;
class XInward;

class JinLord :public IXLorb, public ISinkForRakAgent, public JinSinkForIPC
{
public:
    JinLord(const JinNetAddr &bindaddr, const JinNetPort &bindport,
            const JinNetAddr &localAddr4realTarget, const JinString &selfIdentify);
    ~JinLord();

    bool start();
    void stop();
protected:
    //ISinkForRakAgent
    virtual bool OnDatagramFilter(char* netpack, int len, uint32_t naddr, uint16_t nport);

    virtual void OnOutwardConn(bool isOK, const NetID &id, const JinNetAddr &addr, const JinNetPort &port);
    virtual void OnOutwardConnClose(const NetID &id, bool bGrace);
    virtual void OnOutwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                               NetEngineMsg *packet, const int lenFromNetMsg, int head);

    virtual void OnInwardConnClose(const NetID &id, bool bGrace);
    virtual void OnInwardIncoming(const NetID &id, const JinNetAddr &addr, const JinNetPort &port);
    virtual void OnInwardRecv(const NetID &id, const JinNetAddr &addr, const JinNetPort &port,
                               NetEngineMsg *packet, const int lenFromNetMsg, int head);

    //IXLorb
    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0);
    virtual void DoClose(const NetID &id);
    virtual void OnError(LordError err, bool fromOutward);

    //JinSinkForIPC
    virtual void OnIPCMsg(uint16_t id, void* buf, uint32_t len);
private:
    bool isStart_;
    JinNetAddr bindAddr_;
    JinNetPort bindPort_;
    JinNetAddr localAddr4realTarget_;
    JinString selfIdentity_;
#ifdef JDEBUG
    TimerID quitcheck_;  //检查特定文件，发现就触发退出.
#endif

    RakAgent* agent_;
    JinChart* chart_;
    XOutward* outward_;
    XInward* inward_;
    JinIPC* ipc_;
};

#endif // JINLORD_H
