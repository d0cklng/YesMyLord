#include "jinthread.h"
#include "jinsignaledevent.h"
#include "jinmemorycheck.h"
//#include "jinassert.h" 禁止依赖它.
//#include "jinlogger.h" 禁止依赖它.
#include <assert.h>

//#undef _WIN32  //debug use

#if defined(_WIN32)
#   include <windows.h>
#   include <stdio.h>
#   if !defined(_WIN32_WCE)
#       include <process.h>
#   endif
#else
#   include <pthread.h>
#   include <time.h>
#   include <sys/time.h>
#   include <sys/prctl.h>
#endif
#   include <string.h>

#define MAX_ALLOCA_STACK_ALLOCATION 1048576


JinThread::JinThread(int interval,bool pause)
 : interval_(interval)
 , working_(!pause)
 , event_(0)
{
    threadName_[0] = 0;
}

JinThread::~JinThread()
{
    this->stop();
}

void JinThread::setThreadName(const char *threadName)
{
    strncpy(threadName_,threadName,kThreadNameMaxLength);
    threadName_[kThreadNameMaxLength-1] = 0;
}

bool JinThread::start()
{
    assert(event_==0 && interval_>=0);
    //if(!event_)return false;
    event_ = JNew(JinSignaledEvent);  //现在用event_标识线程工作,(虽然internal为0时不需要event_)
    if(!event_->init()){
        JDelete(event_);
        event_ = NULL;
        return false;
    }
    if(0!=JinThread::create(__threadFunc,(void*)this,threadHandle_)){
        event_->close();
        JDelete(event_);
        event_ = NULL;
        return false;
    }
    afterStart(getThreadId(threadHandle_),threadName_);
    //JDLog("|create th:%llu, threadName=[%s]",(uint64_t)threadHandle_,threadName_);
    return true;
}

void JinThread::stop()
{
    if(event_){
        beforeStop(getThreadId(threadHandle_),threadName_);
        //printf("|wait th:%"PRIu64"\n",(uint64_t)threadHandle_);
        interval_ = -1;
        event_->set();
#ifdef _WIN32
        WaitForSingleObject(threadHandle_,INFINITE);
        CloseHandle(threadHandle_);
#else
        pthread_join(threadHandle_,NULL);  //TODO 会不会线程结束很久后 调pthread_join产生问题.
#endif
        event_->close();
        JDelete(event_);
        event_ = NULL;
    }
}



void JinThread::setInterval(int timems)
{
    if(interval_<0 || timems<0)return;
    interval_ = timems;
}

//pause/resume 只影响period调用，不会中断任何语句.
void JinThread::pause()
{
    if(interval_<0 || !working_)return;
    working_ = false;
}

void JinThread::resume()
{
    if(event_==NULL || interval_<0 || working_ )return;
    working_ = true;
    event_->set();

}

void JinThread::msleep(unsigned int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    //翻墙找到了原作者网页，fuck gfw！这都墙墙你妹。为什么不使用msleep、usleep、sleep作者是这么讲的：
    //[Furquan Shaikh said]Using any variant of sleep for pthreads, the behaviour is not guaranteed.
    //All the threads can also sleep since the kernel is not aware of the different threads.
    //Hence a solution is required which the pthread library can handle rather than the kernel.
    //A safer and cleaner solution to use is the pthread_cond_timedwait...
    //另外据网帖反映，msleep这样的函数时间到后，常常未被调度，导致两次sleep时间比预期要多很多甚至3、4倍。
    //[Rak'kar comment]Single thread sleep code thanks to Furquan Shaikh, http://somethingswhichidintknow.blogspot.com/2009/09/sleep-in-pthread.html
    //Modified slightly from the original
    pthread_mutex_t fakeMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;
    struct timespec timeToWait;
    struct timeval now;

    gettimeofday(&now,NULL);

    long seconds = ms/1000;
    long nanoseconds = (ms - seconds * 1000) * 1000000;
    timeToWait.tv_sec = now.tv_sec + seconds;
    timeToWait.tv_nsec = now.tv_usec*1000 + nanoseconds;

    if (timeToWait.tv_nsec >= 1000000000)
    {
            timeToWait.tv_nsec -= 1000000000;
            timeToWait.tv_sec++;
    }

    pthread_mutex_lock(&fakeMutex);
    pthread_cond_timedwait(&fakeCond, &fakeMutex, &timeToWait);
    pthread_mutex_unlock(&fakeMutex);
#endif
}


#ifdef _WIN32
uint64_t JinThread::getThreadId(HANDLE hThread)
{
    return (uint64_t)GetThreadId(hThread);
}
uint64_t JinThread::getThreadId()
{
    return (uint64_t)GetCurrentThreadId();
}

#else
uint64_t JinThread::getThreadId(pthread_t hThread)
{
    //TODO 在mac下可能有问题，要留意...
    return (uint64_t)hThread;
}
uint64_t JinThread::getThreadId()
{
    return (uint64_t)pthread_self();
}
#endif



THREADFUNCRTN JinThread::__threadFunc(THREADFUNCPARAM)
{
    JinThread *t = (JinThread*)arguments;
    t->threadFunc();
    return 0;  //TODO ?? 线程函数返回啥.
}

int JinThread::create(THREADFUNCADDR)
{
#ifdef _WIN32
    //HANDLE threadHandle;
    unsigned threadID = 0;
#if defined (_WIN32_WCE)
    threadHandle = CreateThread(NULL,MAX_ALLOCA_STACK_ALLOCATION*2,start_address,arglist,0,(DWORD*)&threadID);
    SetThreadPriority(threadHandle, priority);
#else
    threadHandle = (HANDLE) _beginthreadex( NULL, MAX_ALLOCA_STACK_ALLOCATION*2, start_address, arglist, 0, &threadID );
#endif
    SetThreadPriority(threadHandle, priority);

    if (threadHandle==0){	return 1;	}
    else{
        //CloseHandle( threadHandle );
        return 0;
    }
#else
    //pthread_t threadHandle;
    // Create thread linux
    pthread_attr_t attr;
    sched_param param;
    param.sched_priority=priority;
    pthread_attr_init( &attr );
    pthread_attr_setschedparam(&attr, &param);

    pthread_attr_setstacksize(&attr, MAX_ALLOCA_STACK_ALLOCATION*2);
    //@12.24 不要detach，我们总是等待线程结束.
    //pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    int res = pthread_create( &threadHandle, &attr, start_address, arglist );
    //RakAssert(res==0 && "pthread_create in RakThread.cpp failed.")
    return res;
#endif
}

#ifdef JMSVC
const DWORD MS_VC_EXCEPTION=0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#endif
//如果一开始working=false 不要紧period()
//如果interval!=0 不要进wait
void JinThread::threadFunc()
{
    if(threadName_[0])
    {
#ifdef JMSVC
        DWORD dwThreadID = GetCurrentThreadId();
        //detail see https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = threadName_;
        info.dwThreadID = dwThreadID;
        info.dwFlags = 0;
        __try
        {
            RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
        }
#endif

#ifndef WIN32
        prctl(PR_SET_NAME, threadName_);
#endif
    }

    if(init())
    {
        //bool doThread = true;
        do
        {
            if(working_){
                if(!period())break;  //period返回false，退出线程
            }
            if(interval_>0){
                event_->wait(working_?interval_:((unsigned long)(-1)));
                //接下来interval_<0是因为通告退出，
            }
        }
        while(interval_>0);


        //do{
        //    if(interval_>0){
        //        event_->wait(working_?interval_:0x7FFFFFFF);
        //    }
        //    if(interval_<0 || !period())break;  //interval_<0是因为通告退出，  period返回false，退出线程.
        //}
        //while(interval_>0);
    }
    uninit();
}
