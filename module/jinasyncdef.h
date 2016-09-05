#ifndef JINASYNCDEF_H
#define JINASYNCDEF_H

#include "jinlinkedlist.h"

#ifdef _WIN32
#define AE_USING_IOCP
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
# ifndef INFINITE
#  define INFINITE ((uint32_t)(-1))
# endif
#endif

#ifndef HDNULL
# ifdef JWIN
#  define HDNULL NULL
# else
#  define HDNULL 0
# endif
#endif

#ifdef AE_USING_IOCP
typedef HANDLE HdFile;
typedef SOCKET HdSocket;
typedef HANDLE HdObject;  //=all above
//#define HDNULL NULL
#define SOCKINV INVALID_SOCKET
typedef int socklen_t;
#else
typedef int HdFile;
typedef int HdSocket;
typedef int HdObject;  //=all above
//#define HDNULL 0
#define SOCKINV (-1)
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) (void)x
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

struct JinAsynData;
class JinAsyncEngine;

enum AsynOpType{kOpQuitIoWorker=-1,
               kFileOpen=10, kFileRead, kFileWrite, kFileClose,
               kSockAccept=20, kSockConnect, kSockSend, kSockRecv, kSockDisconnect,
               kSockSendTo=30, kSockRecvFrom };

struct JinAsynHandleDetail;
typedef JinAsynHandleDetail* AsynHandle;

class IAsynObj
{
public:

    //异步发生的时候，工作线程直接调用. 可以做一些不用上锁的独立工作提高效率。
    virtual void OnWorkPush(AsynOpType type, char *buf, uint64_t opSize, uint64_t tranSize,
                            void* user, uint64_t user64)
    {UNUSED(type);UNUSED(buf);UNUSED(opSize);UNUSED(tranSize);UNUSED(user);UNUSED(user64);}
     //主线程或者调用线程拉。线程源自调用driveEngine的调用者。
    virtual void OnMainPull(bool isSuccess, HdObject handle, AsynOpType type, void *buf, uint64_t opSize,
                            uint64_t tranSize, void* user, uint64_t user64) = 0;
protected:
    friend class JinAsyncEngine;
    IAsynObj()
        : handle_(HDNULL)
        , aop_(NULL)
    {}
    union{
        HdObject handle_;
        HdFile hfile_;
        HdSocket hsocket_;
    };
    AsynHandle aop_;
private:
};

//通过引用计数来传递管理
class JinAsynBuffer
{
public:
    JinAsynBuffer()
        : ref_(NULL)
        , buffer_(NULL)
    {}
    ~JinAsynBuffer()
    {
        //JAssert(*ref_>=0 && *ref_<20);  //debug
        this->deRef();
    }

    JinAsynBuffer(size_t size) //alloc buff
        : ref_(NULL)
        , buffer_(NULL)
    {
        this->alloc(size);
    }

    JinAsynBuffer(const JinAsynBuffer& o)
    {
        buffer_ = o.buffer_;
        ref_ = o.ref_;
        if(ref_){ ++ (*ref_);}
    }

    JinAsynBuffer& operator=(const JinAsynBuffer& o)
    {
        this->deRef();
        buffer_ = o.buffer_;
        ref_ = o.ref_;
        if(ref_){ ++ (*ref_);}
        return *this;
    }

    bool alloc(size_t size)
    {
        this->deRef();
        ref_ = (int*)JMalloc(sizeof(int));
        if(ref_)
        {
            buffer_ = (char*)JMalloc(size);
            if(buffer_){
                *ref_ = 1;
                return true;
            }
            else{
                JFree(ref_);
                ref_ = NULL;
            }
        }
        return false;
    }

    inline void* buff(size_t pos=0){ return (void*)(buffer_+pos);}
    inline char* buffc(size_t pos=0){return buffer_+pos;}

    //buffer must alloc by 'JMalloc'
    bool eat(void *buffer)
    {
        this->deRef();
        if(!ref_)
        {
            ref_ = (int*)JMalloc(sizeof(int));
            if(ref_ == NULL) return false;
        }
        buffer_ = (char*)buffer;
        *ref_ = 1;
        return true;
    }

    int ref() const
    {
        return ref_?(*ref_):0;
    }

protected:
    void deRef()
    {
        if(ref_ == NULL) return;
        JAssert(*ref_>0);
        if(--(*ref_) <= 0)
        {
            JFree(buffer_);
            JFree(ref_);
        }
        ref_ = NULL;
        buffer_ = NULL;
    }

private:
    int *ref_;
    char *buffer_;  //便于调试器观察内容.
};

/// 初始化异步引擎。在启动程序初始化时来一次.
extern bool initAsyncEngine();

/// 推动引擎，拉取异步信息触发回调
extern bool driveAEngine(uint32_t timeout=0);

/// 反初始化释放引擎。退出程序时确定不要了调用一次.
extern void uninitAsyncEngine();

/// 唤醒作用，解除函数陷入.
extern void awakeAEngine();

extern int GetErrorCode(bool issocket=true);
#ifndef JWIN
extern int closesocket(HdSocket sock);
#endif
#endif // JINASYNCDEF_H


