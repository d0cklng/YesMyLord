#include "usecaseudpsocket.h"
#include "jinudpsocket.h"
#include "jinlogger.h"
#include "jinrandom.h"
#include "jintimer.h"
#include "jinsignaledevent.h"

CHAIN_USE_CASE(UdpSocket);

#define SET_CLIENT_COUNT (4)
#define UDP_SENDTO_TIMES (1000)
static bool gIsRun = true;

class TestUdpPeer : public JinUdpSocket
{
public:
    TestUdpPeer()
    {
        sendBuf.alloc(512);
        recvBuf.alloc(512);
        counter = 0;
    }

    ~TestUdpPeer()
    {
        if(testIndex_<0)
        {
            printf("recv bytes is %d. (expect:%d)\n",counter,UDP_SENDTO_TIMES*400);
            JAssert(counter == UDP_SENDTO_TIMES*400);
        }
    }

    void setTestIndex(int idx)
    {
        testIndex_ = idx;
    }

    int doSend()
    {
        memset(sendBuf.buff(),'$',400);
        return sendTo(sendBuf,0,400,"127.0.0.2",7700);
    }

    int doRecv()
    {
        return recvFrom(recvBuf,0,512-sizeof(sockaddr));
    }

    static int sDoneWorkClient;
    static void PlusDoneWorkCount()
    {
        ++sDoneWorkClient;
        if(sDoneWorkClient>SET_CLIENT_COUNT)
        {
            gIsRun = false;
            awakeAEngine();
        }
    }

protected:
    virtual void OnSendTo(void *buf, uint32_t byteToSend,uint32_t byteSended,
                        const JinNetAddr& addr, const JinNetPort& port)
    {
        JAssert(buf == sendBuf.buff());
        if(byteToSend != byteSended)
        {
            JELog("[%d]OnSendTo %d/%d [%s:%hu]",testIndex_,
                  (int)byteToSend,(int)byteSended,
                  addr.toAddr(gDLogBufAddr),port.port());
            PlusDoneWorkCount();
            return;
        }
        if(counter == 0)
        {
            JinNetAddr addr;
            JinNetPort port;
            if(this->getLocalAddr(addr,port))
            {
                JDLog("[%d]udp peer auto bind:[%s:%hu]",testIndex_,
                      addr.toAddr(gDLogBufAddr),port.port());
            }
        }
        ++counter;
        if(counter<UDP_SENDTO_TIMES)
        {
            int ret = kTSucc;
            memset(sendBuf.buff(),'0'+JinRandom::rnd()%10,400);
            ret = this->sendTo(sendBuf,0,400,"127.0.0.2",7700);
            JAssert(ret == kTSucc);
        }
        else
        {
            JDLog("[%d]SendTo done! [%s:%hu]",testIndex_,
                  addr.toAddr(gDLogBufAddr),port.port());
            PlusDoneWorkCount();
        }
    }
    virtual void OnRecvFrom(void *buf, uint32_t byteRecved,
                        const JinNetAddr& addr, const JinNetPort& port)
    {
        JAssert(buf == recvBuf.buff());
        if(byteRecved != 400)
        {
            JELog("OnRecvFrom %d/400 [%s:%hu]",(int)byteRecved,
                  addr.toAddr(gDLogBufAddr),port.port());
        }
        counter += byteRecved;
        int ret = this->recvFrom(recvBuf,0,450);
        JAssert(ret == kTSucc);
        if(counter >= UDP_SENDTO_TIMES*400)
            PlusDoneWorkCount();
        //UNUSED(buf); UNUSED(byteRecved); UNUSED(addr); UNUSED(port);
    }

private:
    int testIndex_;
    JinAsynBuffer sendBuf;
    JinAsynBuffer recvBuf;
    int counter;
};

int TestUdpPeer::sDoneWorkClient = 0;
UseCaseUdpSocket::UseCaseUdpSocket()
{
    errmsg_[0] = 0;
    engineFlag = false;
}

UseCaseUdpSocket::~UseCaseUdpSocket()
{
}

bool UseCaseUdpSocket::run()
{
    bool bRet;
    if(before())
    {
        bRet = doing();
    }
    after();

    return bRet;
}

const char *UseCaseUdpSocket::errmsg()
{
    if(errmsg_[0])return errmsg_;
    else return NULL;
}

static void timeout_cb(TimerID id, void *user, uint64_t user64)
{
    JDLog("timeout before exit: %llu",id);
    JinSignaledEvent* event = (JinSignaledEvent*)user;
    event->set();
}

bool UseCaseUdpSocket::before()
{
    bool bRet;
    bRet = initAsyncEngine();
    CHECK_ASSERT(bRet==true,"initAsyncEngine error");
    engineFlag = true;

    return bRet;
}

bool UseCaseUdpSocket::doing()
{
    int ret = kTSucc;
    TestUdpPeer peerL;
    TestUdpPeer peerC[SET_CLIENT_COUNT];
    ret = peerL.bind("0.0.0.0",7700);
    CHECK_ASSERT(ret == kTSucc,"UdpPeerL bind error");
    ret = peerL.doRecv();
    CHECK_ASSERT(ret == kTSucc,"UdpPeerL first recv error");
    peerL.setTestIndex(-1);

    for(int i=0;i<SET_CLIENT_COUNT;i++)
    {
        peerC[i].setTestIndex(i);
        ret = peerC[i].doSend();
        CHECK_ASSERT(ret == kTSucc,"client first send failed.");
    }

    while(driveAEngine(INFINITE) && gIsRun);

    bool bRet;
    JinSignaledEvent sig;
    bRet = sig.init();
    CHECK_ASSERT(bRet==true,"signal init failed.");
    if(bRet)
    {
        JinTimer timer(10,100,10);
        timer.start(1000,false,timeout_cb,(void*)&sig);

        for(int i=0;i<100;i++)
        {
            timer.poll();
            bRet = sig.wait(20);
            if(bRet)break;
        }

        CHECK_ASSERT(bRet==true,"signal timeout!");
    }

    return true;
}

void UseCaseUdpSocket::after()
{
    if(engineFlag)
    {
        uninitAsyncEngine();
    }
}
