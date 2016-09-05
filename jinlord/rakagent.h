#ifndef RAKAGENT_H
#define RAKAGENT_H
#include "INetEngine.h"
#include "clientinfoallocator.h"
#include "ISinkForNetEngine.h"
#include "jinchartdef.h"
#include "jinupnpc.h"
#include "jindatatime.h"
#include "jintimer.h"

class ISinkForRakAgent;
class RakAgent : public ISinkForNetEngine, public IChartUdpSocket
{
public:
    RakAgent(ISinkForRakAgent* callback, unsigned area); //上层根据dht id分area
    ~RakAgent();

    bool start(const JinNetAddr &addr, const JinNetPort &port);
    void openFilter();
    //连出连接,这是唯一的,如果已有旧的会被干掉.
    bool connTarget(const NetID &id, unsigned area,
                    JinNetAddr &addr, JinNetPort &port); //area是目标所属分区.
    //bool connTarget(const JinNetAddr &addr, const JinNetPort &port);
    //关闭到目标的连接. 实际有关的返回true
    void closeTarget();
    void stop();

    bool send(NetPacket *packet, int DataLenInPack, SendPriority priority,PackStyle packStyle,char orderingChannel,NetID targetID,bool boardcast=false,int head=1);
    void close(NetID id);

    //ISinkForNetEngine
    virtual int OnNetMsg(INetEngine* src,NetID id,
                         struct sockaddr_in addr4,
                         NetEngineMsg *packet,
                         const int lenFromNetMsg,
                         int head);
    virtual bool OnDatagram(char* netpack, int len,
                            uint32_t naddr, NetPort nport);
    //IChartUdpSocket
    virtual void DoRecvFrom();
    virtual bool SendOut(const unsigned char* buffer, size_t len,
                         const JinNetAddr &addr, const JinNetPort &port);
    virtual uint16_t SockPort();
protected:
    void HandleApplicationData(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                               NetEngineMsg *packet, const int lenFromNetMsg, int head);
    void HandleConnectionAccept(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                                NetEngineMsg *packet, const int lenFromNetMsg);
    void HandleConnectionFailed(const NetID &id, NetEngineMsg *packet);
    void HandleConnectionIncoming(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                                  NetEngineMsg *packet, const int lenFromNetMsg);
    void HandleAlreadyConnected(const NetID &id, JinNetAddr &addr, JinNetPort &port,
                                NetEngineMsg *packet, const int lenFromNetMsg);
    void HandleConnectionClose(const NetID &id, bool bGrace);
    TIMEOUT_HANDLER_DECLARE();

private:
    ISinkForRakAgent* callback_;
    unsigned zoneArea_;
    INetEngine* rak_;
    bool isStart_;
    JinUPnPc upnpc_;
    JinNetAddr addr_;
    JinNetPort port_;
    NetID netid_;

private:
    ConnID outTargetConnID_;   //有的时候代表正在连接.
    NetID outTargetTheOnly_;  //唯一连出连接的目标,是远端xlorb的.
    unsigned outTargetArea_;
    enum State{
        RA_IDLE,
        RA_CONN, //没coServer id的情况直连.
        RA_WAIT, //直连失败又没coServer id等待.
        RA_CONNEX, //通过coServer 复合连接.
        RA_LINK} outTargetState_;  //连目标的状态.
    JinTimer* timer_;  //不提供的话使用JinMainThread的那个.
    void onCoServerConn(int idx, bool isSucc);

    //连接jinzeyucn机制
    //自己连到属于自己的二级域名,如果要连的目标位于别的二级,要先连过去,放一段时间.
    //那么如果同时连的目标其实是相同的NetID岂不是要混乱?
    //关闭连接的时候引起混乱?
    static const int CoServerScale = 16;
    struct CoServerContact
    {
        NetID id;    //有id的是已连接.
        ConnID cid;  //有cid的是正在连接.
        T64 actTime; //都没有的是idle,省掉state变量.
        int* ref;   //不同服务器相同id要做引用.
        //enum State{CSC_IDLE,CSC_CONN,CSC_CACT} state;
    } srvContact[CoServerScale];
    int refBuf_internal[CoServerScale]; //被ref使用.
    TimerID connCoServerTimerID_;
    void openCoServerConn(int idx);
    void closeCoServerConn(int idx);
    void handleCoServerConnOk(bool isAlready,const NetID &id, const ConnID &cid);
    void handleCoServerConnFailed(const ConnID &cid);

};

#endif // RAKAGENT_H
