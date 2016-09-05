#include "usecasetcpsocket.h"
#include <stdint.h>
#include "jintcpsocket.h"
#include "jinlogger.h"
#include "jinrandom.h"

CHAIN_USE_CASE(TcpSocket);

#define SET_CLIENT_COUNT (4)
#define ECHO_SEND_TIMES (10)
#define TCPPORT (7774)

const char *phraseGroup[] =
{
    "1All of our rash guard shirts will give you serious rash protection from your board ",
    "2or wetsuit and serious UV protection while you are in or out of the water with an SPF 50+ rating.",
    "3UV protection.",
    "4Flat seam construction for comfort .",
    "5##~Something()",
    "6Four-way stretch Lycra-enhances their overall comfort, freedom, and flexibility.",
    "",
    "7X",
    "8############include \"jinthread.h\"",
    "9Sun-protected rash guard use for leisure sport with high quality, reasonable price and varied designs."

};

static bool gIsRun = true;


class TestTcpClient : public JinTcpSocket
{
public:
    TestTcpClient()
        : isSrvAccept_(false)
        , sendCount_(ECHO_SEND_TIMES)
        , isDisconnFlag_(false)
        , testIndex_(0)
    {
        sendBuf.alloc(512);
        recvBuf.alloc(512);
    }
    TestTcpClient(HdSocket sock,const JinNetAddr& remoteAddr, const JinNetPort& remotePort)
        : JinTcpSocket(sock,remoteAddr,remotePort)
        , isSrvAccept_(false)
        , sendCount_(ECHO_SEND_TIMES)
        , isDisconnFlag_(false)
        , testIndex_(0)
    {
        sendBuf.alloc(512);
        recvBuf.alloc(512);
    }

    void setTestIndex(int idx)
    {
        testIndex_ = idx;
    }

    void setSrvAccFlag(bool set)
    {
        isSrvAccept_ = set;
        if(set){
            sendCount_ = 0;
        }
    }

    static int sDoneWorkClient;
    static void PlusDoneWorkCount()
    {
        ++sDoneWorkClient;
        if(sDoneWorkClient>=SET_CLIENT_COUNT)
        {
            gIsRun = false;
            awakeAEngine();
        }
    }

    int DoRecv()
    {
        return this->recv(recvBuf,0,500);
    }

    int DoDisconn()
    {
        isDisconnFlag_ = true;
        return this->disconnect();
    }

    void DoClose()
    {
        isDisconnFlag_ = true;
        this->close();
    }

protected:
    virtual void OnConnect(bool success, const JinNetAddr& addr, const JinNetPort& port)
    {
        if(!success)
        {
            JELog("OnConnect faild. addr=%s, port=%hu",addr.toAddr(gDLogBufAddr),port.port());
            PlusDoneWorkCount();
            return;
        }
        JDLog("OnConnect addr=%s, port=%hu, testIndex_=%d",addr.toAddr(gDLogBufAddr),port.port(),testIndex_);

        int ret = kTSucc;
        strcpy(sendBuf.buffc(),phraseGroup[JinRandom::rnd()%10]);
        JDLog("Client do send: %s",sendBuf.buffc());
        ret = this->send(sendBuf,0,strlen(sendBuf.buffc())+1);
        JAssert(ret == kTSucc);
        JDLog("Client post recv: %p",this);
        ret = this->recv(recvBuf,0,500);
        JAssert(ret == kTSucc);
    }

    virtual void OnSend(void *buf,uint32_t byteToSend,uint32_t byteSended)
    {
        JDLog("buf:[%s] %d/%d sended, testIndex_=%d",buf,(int)byteSended,(int)byteToSend,testIndex_);
    }

    virtual void OnRecv(void *buf, uint32_t byteRecved)
    {
        if(byteRecved == 0)
        {
            JELog("OnRecv error stop. testIndex_=%d",testIndex_);
            if(!isSrvAccept_){
                PlusDoneWorkCount();
            }
            return;
        }

        JDLog("OnRecv [%s] testIndex_=%d",buf,testIndex_);
        if(isSrvAccept_)  //echo reply
        {
            int ret = kTSucc;
            JAssert(buf == recvBuf.buff());
            memcpy(sendBuf.buff(),buf,byteRecved);
            JDLog("accSock send back: %s",sendBuf.buffc());
            if(!isDisconnFlag_)
            {
                ret = this->send(sendBuf,0,byteRecved);
                JAssert(ret == kTSucc);
                JDLog("accSock post recv: %p",this);
                ret = this->recv(recvBuf,0,500);
                JAssert(ret == kTSucc);
            }

            //this->disconnect();
        }
        else  //client new send.
        {
            if(--sendCount_>=0)
            {
                int ret = kTSucc;
                strcpy(sendBuf.buffc(),phraseGroup[JinRandom::rnd()%10]);
                JDLog("Client do another send : %s",sendBuf.buffc());
                if(!isDisconnFlag_)
                {
                    ret = this->send(sendBuf,0,strlen(sendBuf.buffc())+1);
                    if(ret != kTSucc)
                    {
                        JELog("send occur error:%d, isSrvAccept_=%d, testIndex_=%d",ret,isSrvAccept_,testIndex_);
                        this->DoDisconn();
                        return;
                    }

                    JDLog("Client post recv: %p",this);
                    ret = this->recv(recvBuf,0,500);
                    if(ret != kTSucc)
                    {
                        JELog("recv occur error:%d, isSrvAccept_=%d, testIndex_=%d",ret,isSrvAccept_,testIndex_);
                        this->DoDisconn();
                        return;
                    }
                }
                return;
            }

            PlusDoneWorkCount();

        }

    }

    virtual void OnDisconnect(bool byRemote)
    {
        JDLog("OnDisconnect isSrvAccept_=%d sendCount_=%d byRemote=%d testIndex=%d",
              (int)isSrvAccept_,sendCount_,byRemote,testIndex_);
        if(!isSrvAccept_){
            PlusDoneWorkCount();
        }
    }
private:
    bool isSrvAccept_;
    int sendCount_;
    JinAsynBuffer sendBuf;
    JinAsynBuffer recvBuf;
    //char innerBuf[1024];
    bool isDisconnFlag_;
    int testIndex_;

};
int TestTcpClient::sDoneWorkClient = 0;



class TestTcpServer : public JinTcpSocket
{
public:
    TestTcpServer()
        : clientIndex(0)
    {

    }

    ~TestTcpServer()
    {
        for(int i=0;i<clientIndex;i++)
        {
            TestTcpClient* tc = clients[i];
            delete tc;
        }
        clientIndex = 0;
    }

    //virtual void OnRecv(void *buf, uint32_t byteRecved)
    //{
    //    UNUSED(buf); UNUSED(byteRecved);
    //}
    void OnAccept(HdSocket accepted, const JinNetAddr& remoteAddr, const JinNetPort& remotePort)
    {
        JDLog("obj=%"PRIu64", OnAccept: %s, %hu",(uint64_t)accepted,remoteAddr.toAddr(gDLogBufAddr),remotePort.port());
        TestTcpClient *ttc = new TestTcpClient(accepted, remoteAddr, remotePort);
        JAssert(ttc->isSocket());
        ttc->setSrvAccFlag(true);
        //int ret;
        JDLog("AccSock[%d] DoRecv",clientIndex);
        int ret = ttc->DoRecv();
        JAssert(ret == kTSucc);

        ttc->setTestIndex(-clientIndex-1);
        clients[clientIndex++] = ttc;
        JAssert(clientIndex<=SET_CLIENT_COUNT);

        if(clientIndex == 2)
        {
            JDLog("do disconn client[0]");
            clients[0]->DoDisconn(); //disconnect,peaceful!
            //clients[0]->DoClose();
        }
        if(clientIndex == 3)
        {
            JDLog("do disconn client[1]");
            clients[1]->DoClose();  //close, violent!
        }

        ret = this->accept();
        JAssert(ret == kTSucc);
    }

private:
    TestTcpClient *clients[SET_CLIENT_COUNT];
    int clientIndex;
};


UseCaseTcpSocket::UseCaseTcpSocket()
{
    errmsg_[0] = 0;
    engineFlag = false;
}

UseCaseTcpSocket::~UseCaseTcpSocket()
{

}

bool UseCaseTcpSocket::run()
{
    bool bRet;
    if(before())
    {
        bRet = doing();
    }
    after();

    return bRet;
}

//const char *UseCaseTcpSocket::errmsg()
//{
//    if(errmsg_[0])return errmsg_;
//    else return NULL;
//}

bool UseCaseTcpSocket::before()
{
    bool bRet;
    //simple test of JinNetAddr

    JinNetAddr na("127.0.0.1.1");
    JinNetAddr nb("127 .0.0.1");   // <= inet函数能解析成功.
    JinNetAddr nc("127.0.1");      // <= inet函数能解析成功.
    JinNetAddr nd("127.0.0.256");
    CHECK_ASSERT(na.isValid()==false,"na should be invalid!");
    //CHECK_ASSERT(nb.isValid()==false,"nb should be invalid!");
    //CHECK_ASSERT(nc.isValid()==true,"nc should be invalid!");
    CHECK_ASSERT(nd.isValid()==false,"nd should be invalid!");

    JinNetAddr ne("127.01.0.1");
    JinNetAddr nf("0.0.0.0");
    JinNetAddr ng("202.96.134.133");
    JinNetAddr nh("10.10.1.101");
    //CHECK_ASSERT(ne.isValid()==true,"ne should be valid!");
    CHECK_ASSERT(nf.isValid()==false,"nf should be valid!");
    CHECK_ASSERT(ng.isValid()==true,"ng should be valid!");
    CHECK_ASSERT(nh.isValid()==true,"nh should be valid!");

    JinNetAddr nx = JinNetAddr::fromRawIP(ng.rawIP());
    CHECK_ASSERT(strcmp(nx.toAddr(gDLogBufAddr),"202.96.134.133")==0,"JinNetAddr trans nx error!");
    CHECK_ASSERT(strcmp(na.toAddr(gDLogBufAddr),"0.0.0.0")==0,"JinNetAddr trans na error!");

    bRet = initAsyncEngine();
    CHECK_ASSERT(bRet==true,"initAsyncEngine error");
    engineFlag = true;

    JinAsynBuffer bf;
    JinAsynBuffer bfSize(128);
    JinAsynBuffer bfCopy(bfSize);
    CHECK_ASSERT(bf.alloc(100),"JinAsynBuffer alloc failed!");
    bf = bfCopy;
    CHECK_ASSERT(bf.ref() == 3,"JinAsynBuffer ref error!");
    JinAsynBuffer bfEat;
    bfEat.eat(JMalloc(64));
    bf = bfEat;
    bfSize = bfEat;
    bfCopy = bfEat;
    CHECK_ASSERT(bf.ref() == 4,"JinAsynBuffer ref error.2!");

    return bRet;
}

bool UseCaseTcpSocket::doing()
{
    int ret = kTSucc;
    TestTcpServer tcpServer;
    TestTcpClient tcpClient[SET_CLIENT_COUNT];

    ret = tcpServer.bind("127.0.0.1",TCPPORT);
    CHECK_ASSERT(ret == kTSucc,"Server bind error");
    ret = tcpServer.listen(2);
    CHECK_ASSERT(ret == kTSucc,"Server listen error");
    ret = tcpServer.accept();
    CHECK_ASSERT(ret == kTSucc,"Server accept error");

    //tcpClient[SET_CLIENT_COUNT].connect("59.60.191.82",TCPPORT);
    //tcpClient[SET_CLIENT_COUNT+1].connect("112.124.61.231",80);

    for(int i=0;i<SET_CLIENT_COUNT;i++)
    {
        tcpClient[i].setTestIndex(i);
        ret = tcpClient[i].connect("127.0.0.1",TCPPORT);
        CHECK_ASSERT(ret == kTSucc,"client connect server directly failed.");
    }

    while(driveAEngine(INFINITE) && gIsRun);

    return true;
}

void UseCaseTcpSocket::after()
{
    if(engineFlag)
    {
        uninitAsyncEngine();
    }
}
