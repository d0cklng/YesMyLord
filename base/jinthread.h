#ifndef JINTHREAD_H
#define JINTHREAD_H

//#undef _WIN32  //debug use
/* *******************************
 *
 * 线程实现，主要针对周期工作线程
 * 依赖: jinsignaledevent jinmemorycheck
 *
 * *******************************/
#if defined(_WIN32)
#   include <windows.h>
#else
#   include <pthread.h>
#endif

#include "jinformatint.h"
#include "jinsignaledevent.h"

#if defined(_WIN32_WCE)
#define THREADFUNCRTN DWORD WINAPI
#define THREADFUNCPARAM LPVOID arguments
#define THREADFUNCADDR LPTHREAD_START_ROUTINE start_address, void *arglist, HANDLE &threadHandle, int priority
#define THREADHANDLE HANDLE
#elif defined(_WIN32)
#define THREADFUNCRTN unsigned __stdcall
#define THREADFUNCPARAM void* arguments
#define THREADFUNCADDR unsigned __stdcall start_address( void* ), void *arglist, HANDLE &threadHandle, int priority
#define THREADHANDLE HANDLE
#else
#define THREADFUNCRTN void*
#define THREADFUNCPARAM void* arguments
#define THREADFUNCADDR void* start_address( void* ), void *arglist, pthread_t &threadHandle, int priority
#define THREADHANDLE pthread_t
#endif


#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) (void)x
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

class JinThread
{
public:
    //如果interval设为0（默认）表示只进一次period //pause不影响init
    explicit JinThread(const int interval=0,const bool pause=false);
    virtual ~JinThread();
    void setThreadName(const char* threadName=NULL);
    //@12.24 bugfix:构造函数中创建线程，线程马上调用了类的虚函数导致崩溃.
    virtual bool start();
    virtual void stop();
    //设定period调用间隔，单位ms。如果设为0将退出线程，如果还在运行则执行完period退出.
    void setInterval(int timems); //初始化失败或传入<0时，函数无效.
    //pause/resume 只影响period调用，不会中断任何语句.
    void pause();
    void resume();
    static void msleep(unsigned int ms);
    static uint64_t getThreadId();  //无参数版，获取调用者本身线程.
#ifdef _WIN32
    static uint64_t getThreadId(HANDLE hThread);  //ret DWORD
#else
    static uint64_t getThreadId(pthread_t hThread);
#endif
    //可以跳过JinThread使用create创建纯粹线程.
    static THREADFUNCRTN __threadFunc(THREADFUNCPARAM);
    static int create(THREADFUNCADDR=0);
protected:
    void threadFunc();
    //如果返回false，线程提前终止，仍会执行uninit().
    virtual bool init(){return true;}
    //当需要线程停止时，返回false.
    virtual bool period()=0;

    virtual void uninit(){}

    virtual void afterStart(uint64_t threadId,const char* threadName)
    {UNUSED(threadId);UNUSED(threadName);}
    virtual void beforeStop(uint64_t threadId,const char* threadName)
    {UNUSED(threadId);UNUSED(threadName);}
private:
    int interval_; //period interval. 如果小于0表示不会执行，在初始化失败时设置.interval_==0只执行一次线程.
    bool working_;
    JinSignaledEvent *event_;
#ifdef _WIN32
    HANDLE threadHandle_;
#else
    pthread_t threadHandle_;
#endif
    const static int kThreadNameMaxLength = 16;
    char threadName_[kThreadNameMaxLength];
};

#endif // JINTHREAD_H

