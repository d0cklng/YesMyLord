#ifndef USECASECRYPTO_H
#define USECASECRYPTO_H

#include "TestHelper.h"

class UseCaseCrypto : public TestEasyBase
{
public:
    UseCaseCrypto();
    virtual ~UseCaseCrypto();
    bool run();
protected:
    int testRSA(void);
    int testECC(void);
    int testRealUse();
    int testJinPublicKey();
    int testPublicKeyMore();
};

#endif // USECASECRYPTO_H
