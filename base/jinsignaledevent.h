#ifndef JINSIGNALEDEVENT_H
#define JINSIGNALEDEVENT_H
#define WAITUNTILWORLDEND ((unsigned long)(-1))

//#undef _WIN32  //debug use

#if defined(_WIN32)
#   include <windows.h>
#   include <stdio.h>
#   if !defined(_WIN32_WCE)
#       include <process.h>
#   endif
typedef HANDLE CondHandle;
#else
#   include <pthread.h>
#   include <time.h>
#   include <sys/time.h>
typedef pthread_cond_t CondHandle;
#endif

//这个类代码本来是来自RakNet SignaledEvent, 保持了原有的命名方式.
//但是里面的做法感觉太复杂。经过细致理解后做了较大改动，免去了30ms重试检漏信号的做法，减去SimpleMutex的使用.
//具体效果是否正确，效率是否提高还有待检验.
class JinEvent
{
public:
    JinEvent();
    ~JinEvent();

    bool init(bool autoReset=true);
    void close(void);
    void set(void);
    void reset(void);
    bool wait(unsigned long timeoutMs);
protected:
    bool isInit;
#ifdef _WIN32
    CondHandle cond;
#else
    bool isSignaled;
    bool isAutoReset;
#if !defined(ANDROID)
    pthread_condattr_t condAttr;
#endif
    CondHandle cond;
    pthread_mutex_t hMutex;
    pthread_mutexattr_t mutexAttr;
#endif
};

typedef JinEvent JinSignaledEvent;
#endif // JINSIGNALEDEVENT_H
