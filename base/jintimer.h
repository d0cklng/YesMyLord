#ifndef JINTIMER_H
#define JINTIMER_H

#include <stddef.h>

#include "jinhashmap.h"
#include "jinmempool.h"
#include "jinsingleton.h"

/* ********************
 *
 * 单线程粗精确度定时器
 * 这个定时器的优点是可以比较宽松的poll它，能够支持大量并发timer
 * 这个实现源自迅雷下载库XL_TIMER的实现，原实现为C风格
 * 我在它的基础上改装成C++并在改装过程中做了简化优化，并借助union来充分利用mempool
 *
 * *******************/
#include <map>

typedef uint64_t TimerID;
typedef void (*timeout_callback)(TimerID id,void *user,uint64_t user64);
#define INVALIDTIMERID 0
typedef struct TimerX
{
    TimerID id;      //TriggerMsg使用.
    uint32_t interval;   //<--共用.
    int32_t hindex;   //TriggerMsg使用.
    //union{
   //     uint32_t interval;    //<--CycleNode使用 TriggerMsg在start前使用.
    //    int32_t hindex;     //TriggerMsg在start后使用.
    //};
    struct TimerX *next;  //<--共用.
    union{
        struct TimerX* infoListHead;  //<--CycleNode使用.
        void* user;     //TriggerMsg使用
    };
    union{
        struct TimerX* infoListTail;  //<--CycleNode使用.
        timeout_callback callback;    //TriggerMsg使用.
    };
    uint64_t user64;    //TriggerMsg使用.
    bool autoRestart;   //TriggerMsg使用.

}CycleNode,TriggerMsg;

extern uint64_t getTickCount();

class JinTimer
{
public:
    /// pridict传入预期数量规模以优化性能, cyclePeriod/granularity≈pridict
    /// cyclePeriod=1000越大，算法越快越废内存,granularity越小，精度越高越废内存.
    JinTimer(int pridict=100, int cyclePeriod=1000, int granularity=10);
    ~JinTimer();
    TimerID start(uint32_t interval, bool autoRestart, timeout_callback cb, void* user=NULL, uint64_t user64=0);
    bool cancel(TimerID id);
    size_t count(){return timers_.size();}
    void poll();
protected:
    TimerID start(TriggerMsg* tmsg);
    bool insert(uint32_t timeout, TriggerMsg *data);
    size_t popExpire(int32_t index, int32_t layer);
    size_t popAllExpire();
    bool eraseTimer(TriggerMsg *msg);
    //int32_t erase_from_timer_with_valid_index(void *comparator_data, data_comparator comp_fun, int32_t timer_index, void **data);
    //int32_t erase_from_timer_with_timeout(void *comparator_data, data_comparator comp_fun, void **data);
    //int32_t erase_from_timer_with_all_index(void *comparator_data, data_comparator comp_fun, void **data);
    //int32_t erase_from_timer(void *comparator_data, data_comparator comp_fun, int32_t timer_index, void **data);

    void refresh();
    //int32_t poll_timer();
private:
    int32_t granularity_;
    int32_t cycleNodes_;

    TimerID serial_;  //派发timerid
    CycleNode* *nodes_;//[cycleNodes_]; 主水平node行
    //LIST m_infinite_list;
    uint64_t lastNode_  ;         // 上一次指向那个时间槽.
    uint64_t distance_  ;          // 和上一个周期的距离.
    uint64_t curTimespot_  ;
    JinMemPool<struct TimerX> memPool_;
    //uint64_t threadID_;
    //std::map<void*, int> timeout_;
    std::map<TimerID,TriggerMsg*> timeout_;
    std::map<TimerID,TriggerMsg*> timers_;
};

//typedef JinSingleton<JinTimer> GlobalTimer;  //需要从main层面予以维护。

#define TIMEOUT_HANDLER_DECLARE() void handleTimeout(TimerID id);\
    static void sHandleTimeout(TimerID id, void *user, uint64_t user64)
#define TIMEOUT_HANDLER_IMPLEMENT(T) void T::sHandleTimeout(TimerID id, void *user, uint64_t user64)\
{  T* p##T = (T*)user;   p##T->handleTimeout(id);  }\
void T::handleTimeout(TimerID id)
#endif // JINTIMER_H
