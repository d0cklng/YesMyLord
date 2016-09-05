#ifndef USECASEDNS_H
#define USECASEDNS_H

#include "TestHelper.h"
#include "jindnsparser.h"

class UseCaseDNS : public TestEasyBase
{
public:
    UseCaseDNS();
    virtual ~UseCaseDNS();
    bool run();
    static void JinOnDnsParsed(const char* domain, JinNetAddr addr, void* user, uint64_t user64);

    void plus(){++donework;}
protected:
    bool before();
    bool doing();
    void after();
private:
    volatile int donework;
};

#endif // USECASEDNS_H
