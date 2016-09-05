#include "usecaseipc.h"
#include "jinipc.h"
#include "jinlogger.h"
#include "jinthread.h"

CHAIN_USE_CASE(IPC);
class CallBackHost : public JinSinkForIPC
{
public:
    CallBackHost(JinIPC* obj)
        : obj_(obj)
    {}
public:
    void OnIPCMsg(uint16_t id, void* buf, uint32_t len)
    {
        JDLog("IPC.host %hu echo,%s",id,(char*)buf);
        bool ret = obj_->sendIPCMsg(buf,len);
        JAssert(ret);
    }
private:
    JinIPC* obj_;
};

class CallBackJoin : public JinSinkForIPC
{
public:
    CallBackJoin()
        : count(0)
    {}
public:
    void OnIPCMsg(uint16_t id, void* buf, uint32_t len)
    {
        JDLog("IPC.join %hu echo,%s",id,(char*)buf);
        ++ count;
    }
    int count;
};

UseCaseIPC::UseCaseIPC()
{
    errmsg_[0] = 0;
    engineFlag = false;
}

UseCaseIPC::~UseCaseIPC()
{

}

bool UseCaseIPC::run()
{
    bool bRet;
    if(before())
    {
        bRet = doing();
    }
    after();

    return bRet;
}

bool UseCaseIPC::before()
{
    bool bRet = initAsyncEngine();
    CHECK_ASSERT(bRet==true,"initAsyncEngine error");
    engineFlag = true;

    host = new JinIPC("UseCaseIPC");
    host->startHost("password.x",10);
    hostCallback = new CallBackHost(host);
    host->setCallback(hostCallback);
    return true;
}

bool UseCaseIPC::doing()
{
    bool ret = false;
    CallBackJoin callbackjoin;
    join = new JinIPC("UseCaseIPCA");
    JAssert(join);
    ret = join->startJoin("password.x",10);
    JAssert(!ret);
    delete join;
    join = new JinIPC("UseCaseIPC");
    JAssert(join);
    join->setCallback(&callbackjoin);
    ret = join->startJoin("password.x.wrong",16);
    JAssert(!ret);
    ret = join->startJoin("password.x",10);
    JAssert(ret);

    char buf[128];
    memset(buf,'3',128);
    for(int i=0;i<100;i++)
    {
        buf[100-i] = 0;
        ret = join->sendIPCMsg(buf,100-i+1);
        JAssert(ret);
    }

    while(driveAEngine(INFINITE) && callbackjoin.count!=100);

    delete join;
    join = NULL;
}

void UseCaseIPC::after()
{
    if(host) delete host;
    host = NULL;
    if(hostCallback) delete hostCallback;
    hostCallback = NULL;

    if(join) delete join;
    join = NULL;

    if(engineFlag)
    {
        uninitAsyncEngine();
    }
}

