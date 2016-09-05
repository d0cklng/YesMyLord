#ifndef USECASESTAR_H
#define USECASESTAR_H

#include "TestHelper.h"
#include "jintimer.h"
//#include "jinkey160.h"
#include "jinchart.h"

class JinChart;
class UseCaseStar : public TestEasyBase
{
public:
    UseCaseStar();
    virtual ~UseCaseStar();
    bool run();
    void timeout(TimerID id);
    static bool OnStarFound(const JinKey160& key, JinNetAddr addr, JinNetPort port, void* user);
protected:
    bool before();
    bool doing();
    void after();

    static void sTimeout(TimerID id, void *user, uint64_t user64);
private:
    bool testBuckets();
    bool testStore();
    bool testKrpc();

    bool engineFlag;
    int walkonStart;
    JinChart *chart;
    JinChart *walkon;
    TimerID exitTimer;
    TimerID searchTimer;
    TimerID walkonTimer;
    uint16_t tranID;
};

#endif // USECASESTAR_H
