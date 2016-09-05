#ifndef USECASEIPC_H
#define USECASEIPC_H

#include "TestHelper.h"

class JinIPC;
class CallBackHost;
class UseCaseIPC : public TestEasyBase
{
public:
    UseCaseIPC();
    virtual ~UseCaseIPC();
    bool run();

protected:
    bool before();
    bool doing();
    void after();

private:
    JinIPC* host;
    CallBackHost *hostCallback;
    JinIPC* join;
    bool engineFlag;
};

#endif // USECASEIPC_H
