#ifndef USECASEBASE_H
#define USECASEBASE_H

#include "TestHelper.h"

class UseCaseBase : public TestEasyBase
{
public:
    UseCaseBase();
    virtual ~UseCaseBase();
    bool run();
protected:
    void runMiscellaneous();
    void runJinhashmap();
    void runJinLinkedList();
    void runJinString();
    void runJinEvent();
    void runJinDir();
    void runJinCompress();
};

#endif // USECASEBASE_H
