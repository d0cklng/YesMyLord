#ifndef JINASYNCENGINE_H
#define JINASYNCENGINE_H
#include "jinformatint.h"
#include <stddef.h>
#include "jinasyncdef.h"
#include "jinsingleton.h"
#include "jinlinkedlist.h"
#include "jinmempool.h"
#include "jinmutex.h"
#include "jinsignaledevent.h"
#include "jinhashmap.h"
#include "jinioport.h"
#include "jinfile.h"

struct JinAsynData
{
    OVERLAPPED overlap; //must at first
    AsynOpType type;
    //IAsynObj *obj;
    HdObject handle;
#ifndef AE_USING_IOCP
    AsynHandle aop;
#endif
    //char *buf;
    JinAsynBuffer *buffer;
    uint32_t opSize;    //要操作的大小
    uint32_t tranSize;  //成功操作的大小
    void* user;
    uint64_t user64;
};

typedef JinLinkedList<JinAsynData*> DataList;
class ExtraIOWorker;
#define WORKERCOUNT 2



class JinAsyncEngine
{
public:
    JinAsyncEngine();
    ~JinAsyncEngine();
    bool init();  //0表示不要worker，那么没有OnWorkPush。小于0表示取默认推荐值。

    AsynHandle attach(IAsynObj *obj);  //使用已经开启handle的asynobject调用此函数获取操作手柄。
    void detach(AsynHandle aop);  //脱离关系，引擎不再回调，调用者可以删除自己。
    //bool fileCreate(AsynHandle aop, )

    //!fileOpen 特殊,不要调用attach,而且open成功后自动
    AsynHandle fileOpen(IAsynObj *obj, const char* path, JFAccessFlag acc, JFOpenFlag ope, void* user=NULL);
    //bool fileOpen(AsynHandle aop, const char* path, JFAccessFlag acc, JFOpenFlag ope, void* user=NULL);
    bool fileRead(AsynHandle aop, uint64_t from, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t toRead, void* user=NULL);
    bool fileWrite(AsynHandle aop, uint64_t to, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t toWrite, void* user=NULL);
    bool fileClose(AsynHandle aop,  void* user=NULL);
#ifdef AE_USING_IOCP
    bool postAccept(AsynHandle aop, HdSocket accSock);
#else
    bool postAccept(AsynHandle aop);
#endif
    bool postConnect(AsynHandle aop,uint32_t rawIP, uint16_t rawPort);
    bool postDisconnect(AsynHandle aop);
    bool postSend(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend);
    bool postRecv(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv);

    bool postSendTo(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend,
                    uint32_t rawIP, uint16_t rawPort);
    /// 特别注意，bufPos+byteMaxRecv位置的buffer将被用于存放来源方地址，所以buffer有效空间至少多留sizeof(sockaddr)
    bool postRecvFrom(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv);

    //返回true表示成功拿到了消息，
    //对于timeout无限，可以马上再来一次直到返回false意味着循环退出
    //对于timeout为0，可以马上再来一次直到返回false，msleep一段再试
    bool driveEngine(uint32_t timeout=0);  //以合适的时间间隔，由主线程调用推动。实质是提取已备的消息。
    void awakePost();  //造成driveEngine返回true，解除陷入
public:
    void uninit();  //析构时自动调用
protected:
    //void freeAsynData(JinAsynData *data, const char *file, unsigned int line);
    void checkOpCountMayDoFree(AsynHandle aop);
private:
    HdPort hdPort_;
    JinMemPool<JinAsynData> dataPool_;
    //JinHashMap<JinAsynObjUnit*> objUnits_;

    DataList extraWorkList_;
    JinSignaledEvent extraWorkSignal_;
    JinMutex extraWorkMutex_;
    ExtraIOWorker *workers_[WORKERCOUNT];
    //int workerCount_;
    bool uninitFlag_;
};

typedef JinSingleton<JinAsyncEngine> AE;

#ifdef JWIN
BOOL winConnectEx(SOCKET s,const struct sockaddr *name,int namelen,
                  //PVOID lpSendBuffer,DWORD dwSendDataLength, LPDWORD lpdwBytesSent,
                  LPOVERLAPPED lpOverlapped);

BOOL winDisconnectEx(SOCKET hSocket,LPOVERLAPPED lpOverlapped,DWORD dwFlags,DWORD reserved);

BOOL winAcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket,
                 PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
                 DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
                 LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped );

VOID winGetAcceptExSockaddrs(SOCKET sListenSocket,
                             PVOID lpOutputBuffer,DWORD dwReceiveDataLength,
                             DWORD dwLocalAddressLength,DWORD dwRemoteAddressLength,
                             struct sockaddr **LocalSockaddr,LPINT LocalSockaddrLength,
                             struct sockaddr **RemoteSockaddr,LPINT RemoteSockaddrLength);
#endif

#endif // JINASYNCENGINE_H
