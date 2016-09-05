#include "jinmutex.h"
#include "jinassert.h"

JinMutex::JinMutex()
{
    init();
}

JinMutex::~JinMutex()
{
#ifdef _WIN32
    DeleteCriticalSection(&criticalSection_);
#else
    pthread_mutex_destroy(&hMutex_);
#endif
}

#ifdef _WIN32
#ifdef _DEBUG
#include <stdio.h>
#endif
#endif

void JinMutex::lock(void)
{
// 	if (isInitialized==false)
// 		Init();
//    isLock = true;
#ifdef _WIN32
    EnterCriticalSection(&criticalSection_);
#else
    int error = pthread_mutex_lock(&hMutex_);
    (void) error;
    JAssert3(error==0,"pthread_mutex_lock.error",&error);
#endif
}

void JinMutex::unlock(void)
{
#ifdef _WIN32
    LeaveCriticalSection(&criticalSection_);
#else
    int error = pthread_mutex_unlock(&hMutex_);
    (void) error;
    JAssert3(error==0,"pthread_mutex_unlock.error",&error);
#endif
//    isLock = false;
}

bool JinMutex::tryLock()
{
#ifndef WIN32
    int error = pthread_mutex_trylock(&hMutex_);  //EAGAIN
    JAssert3(error==0||error==EBUSY,"pthread_mutex_trylock.error",&error);
    return (error==0);
#else
    JAssert2(false,"not implement.");
    this->lock();  //TODO  未实现 在windows下不要调用.
    return true;
#endif
}

void JinMutex::init(void)
{
#ifdef _WIN32
    InitializeCriticalSection(&criticalSection_);
#else
    int error = pthread_mutex_init(&hMutex_, 0);
    JAssert3(error==0,"pthread_mutex_init.error",&error);
#endif
}
