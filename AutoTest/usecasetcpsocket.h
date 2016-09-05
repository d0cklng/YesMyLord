#ifndef USECASETCPSOCKET_H
#define USECASETCPSOCKET_H

#include "TestHelper.h"

class UseCaseTcpSocket : public TestEasyBase
{
public:
    UseCaseTcpSocket();
    virtual ~UseCaseTcpSocket();
    bool run();
    //const char* errmsg();

protected:
    bool before();
    bool doing();
    void after();

private:
    bool engineFlag;
};

#endif // USECASETCPSOCKET_H
