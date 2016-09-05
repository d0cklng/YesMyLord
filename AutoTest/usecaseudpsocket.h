#ifndef USECASEUDPSOCKET_H
#define USECASEUDPSOCKET_H

#include "TestHelper.h"

class UseCaseUdpSocket : public TestEasyBase
{
public:
    UseCaseUdpSocket();
    virtual ~UseCaseUdpSocket();
    bool run();
    const char* errmsg();

protected:
    bool before();
    bool doing();
    void after();

private:
    bool engineFlag;
};

#endif // USECASEUDPSOCKET_H
