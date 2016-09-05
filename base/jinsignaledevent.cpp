#include "jinsignaledevent.h"
#include "jinsharedata.h"
#include "jinassert.h"
#include "jinmemorycheck.h"

JinEvent::JinEvent()
    : isInit(false)
    #ifndef _WIN32
    , isSignaled(false)
    , isAutoReset(true)
    #endif
{ }
JinEvent::~JinEvent()
{
    if(isInit)this->close();
}

bool JinEvent::init(bool autoReset)
{
#ifdef _WIN32
    cond = CreateEvent(0, autoReset?FALSE:TRUE, FALSE, 0);
    if(cond != INVALID_HANDLE_VALUE)isInit = true;
#else
    int res = 0;
    isAutoReset = autoReset;
#   if !defined(ANDROID)
    res |= pthread_condattr_init( &condAttr );
    res |= pthread_cond_init(&cond, &condAttr);
#   else
    res |= pthread_cond_init(&cond, 0);
#   endif
    res |= pthread_mutexattr_init( &mutexAttr	);
    res |= pthread_mutex_init(&hMutex, &mutexAttr);
    if(res==0)isInit = true;
#endif
    return isInit;
}

void JinEvent::close(void)
{
    if(!isInit)return;
#ifdef _WIN32
    CloseHandle(cond);
#else
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&hMutex);
#   if !defined(ANDROID)
    pthread_condattr_destroy( &condAttr );
#   endif
    pthread_mutexattr_destroy( &mutexAttr );
#endif
    isInit = false;
}

void JinEvent::set(void)
{
    JAssert(isInit);
#ifdef _WIN32
    ::SetEvent(cond);
#else
    pthread_mutex_lock(&hMutex);
    isSignaled = true;
    pthread_mutex_unlock(&hMutex);
    pthread_cond_signal(&cond);
#endif
}

void JinEvent::reset(void)
{
    JAssert(isInit);
#ifdef _WIN32
    ::ResetEvent(cond);
#else
    pthread_mutex_lock(&hMutex);
    isSignaled = false;
    pthread_mutex_unlock(&hMutex);
#endif
}

//RakNet signaledEvent的实现中，认为pthread_cond_wait的动作无法确定时机，在线程并发的情况下
//很可能漏掉信号。例如正要wait前线程被调度睡眠，另一个线程发射信号的时候并没有进入wait，当wait的时候信号已经错过。
//出于这种动机，RakNet的给出了一个很繁琐的实现，会重复去检查，而检查主要用于查漏掉的信号。
//如果需要线程苏醒检查的话，那还不如用sleep呢，不够完美。
//经过研究了解到，cond_wait实际上会以原子操作的方式，在进入等待时把mutex释放，苏醒后再次将mutex锁上。
//利用这一点，其实是个简单的问题，不存在漏信号的情况。同时这样的机制使得多个线程wait成为可能。
//（一旦wait，释放mutex，下一个wait线程可以进入)
//true表示由事件激发，false表示由timeout或abort导致.
bool JinEvent::wait(unsigned long timeoutMs)
{
    JAssert(isInit);
#ifdef _WIN32
    if(timeoutMs == (unsigned long)(-1))timeoutMs = INFINITE;
    return (WAIT_OBJECT_0==WaitForSingleObject(cond,timeoutMs));
#else
    pthread_mutex_lock(&hMutex);
    if(isSignaled){
        isSignaled = false;
        pthread_mutex_unlock(&hMutex);
        return true;
    }
    else{
        int rc;
        if(timeoutMs == (unsigned long)(-1)){
            rc = pthread_cond_wait(&cond, &hMutex); // = unlock => wait => lock again
        }
        else{
            struct timeval tp;
            rc =  gettimeofday(&tp, NULL);
            if(rc!=0)return false;
            timespec ts;
            ts.tv_sec  = tp.tv_sec;
            ts.tv_nsec = tp.tv_usec * 1000 + timeoutMs*1000000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec++;
            }
            rc = pthread_cond_timedwait(&cond, &hMutex, &ts); // = unlock => wait => lock again
        }
        if(isAutoReset)isSignaled = false;
        else pthread_cond_signal(&cond);
        pthread_mutex_unlock(&hMutex);
        return (rc==0)?true:false;
    }
#endif
}
