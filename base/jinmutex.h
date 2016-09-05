#ifndef JINMUTEX_H_201409212132
#define JINMUTEX_H_201409212132

/* *********************************************************
 *
 * 简单互斥体，几乎都是从RakNet-SimpleMutex抄过来
 * RakNet作者用经验告诉我们，使用单生产/消费模型，比到处都线程安全（threadsafe）快百倍
 * 所以要避免用负责的多线程模型，避免用锁
 * 依赖: jinassert
 *
 * *********************************************************/
#if   defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
//#include <sys/types.h>
#include <sys/errno.h>
#endif


class JinMutex
{
public:
    JinMutex();
    ~JinMutex();
    void lock();
    void unlock();
    bool tryLock();
private:
    void init(void);
#ifdef _WIN32
    /// Docs say this is faster than a mutex for single process access
    CRITICAL_SECTION criticalSection_;
#else
    pthread_mutex_t hMutex_;
#endif
};

#endif // JINMUTEX_H
