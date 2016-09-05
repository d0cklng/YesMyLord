#ifndef USECASEFILEASYNC_H
#define USECASEFILEASYNC_H

#include "TestHelper.h"

class UseCaseFileAsync : public TestEasyBase
{
public:
    UseCaseFileAsync();
    //virtual ~UseCaseFileAsync();
    bool run();
    const char* errmsg();


    void liteTest();
    void unitTest();

    void onQuit();
private:
    bool isRun_;
};

#endif // USECASEFILEASYNC_H
