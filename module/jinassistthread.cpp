#include "jinassistthread.h"
#include "jinthread.h"
#include "jinassert.h"
#include "jintimer.h"

JinAssistThread::~JinAssistThread()
{
    JinMainThread::Get()->doRemove(this);
}

void JinAssistThread::awakeMainThread(void *user)
{
    JinMainThread::Get()->doAwake(this,user);
}

//=======================================//
//======= next coming JinMainThread =====//
//=======================================//
//这个类设计为先于main构造，不要打日志或内存检测.
JinMainThread globalMainThread;
JinMainThread::JinMainThread()
    : data_(NULL)
    , quitFlag_(false)
{
    threadID_ = JinThread::getThreadId();
    instance() = this;
    ++ ref();
}

JinMainThread::~JinMainThread()
{
    JAssert(data_==NULL);
    //这里必须是最后一个free的地方,
    JAssert2(instance(),"JinMainThread must exist!");
    //如果其他地方有create行为，必须先free掉.
    JAssert2(ref()==1,"~JinMainThread at last.");
    instance() = 0;
    -- ref();
}

bool JinMainThread::init()
{
    JAssert(JinThread::getThreadId() == threadID_);
    data_ = JNew(MainData);
    if(!data_)return false;

    bool ret = true;
    ret &= data_->event_.init();
    ret &= data_->awakeMap_.init(16);
    data_->timer_ = NULL;

    if(!ret)
    {
        JDelete(data_);
    }
    return ret;
}

void JinMainThread::uninit()
{
    JAssert(JinThread::getThreadId() == threadID_);
    if(data_)
    {
        if(data_->timer_) JDelete(data_->timer_);
        JDelete(data_);
        data_ = NULL;
    }
}

void JinMainThread::changeMainThreadToCurrent()
{
    JAssert(JinThread::getThreadId() != threadID_);
    threadID_ = JinThread::getThreadId();
}

bool JinMainThread::msleep(unsigned long ms)
{
    return JinMainThread::Get()->doSleep(ms);
}

void JinMainThread::notifyQuit()
{
    JinMainThread::Get()->setQuitFlag(true);
}

JinTimer *JinMainThread::getTimer()
{
    JAssert(data_);
    if(data_->timer_ == NULL){
        data_->timer_ = JNew(JinTimer);
    }
    return data_->timer_;
}

void JinMainThread::doAwake(JinAssistThread *byWho, void *user)
{
    JAssert(data_);
    JAssert(JinThread::getThreadId() != threadID_);
    data_->mutex_.lock();
    data_->awakeMap_.set(byWho,user);
    data_->mutex_.unlock();

    data_->event_.set();
}

bool JinMainThread::doSleep(unsigned long ms)
{
    JAssert2(data_,"must init at begin of you program");
    JAssert2(JinThread::getThreadId() == threadID_,
             "Only can be called by MainThread!, if you known what you are doing, try ChangeMainThreadToCurrent() at first.");

    if(data_->timer_){
        data_->timer_->poll();
    }

    data_->event_.wait(ms);

    size_t mapSize = 0;
    JinAssistThread* assist = NULL;
    void* user;
WakeProcedure:
    data_->mutex_.lock();
    mapSize = data_->awakeMap_.size();
    if(mapSize)
    {
        JinHashMap<JinAssistThread*,void*>::iterator it = data_->awakeMap_.begin();
        assist = it.first();
        user = it.second();
        data_->awakeMap_.erase(it);
        --mapSize;
    }
    data_->mutex_.unlock();
    //提取出信息出锁后再回调，避免回调中又调用了这个类的功能。可重入特性.
    if(assist){
        assist->OnAwakeCall(user);
    }
    if(mapSize){
        goto WakeProcedure;
    }

    return !quitFlag_;
}

void JinMainThread::doRemove(JinAssistThread *byWho)
{
    JAssert(data_);
    JAssert2(JinThread::getThreadId() == threadID_,
             "Only can be called by MainThread!");

    data_->mutex_.lock();
    data_->awakeMap_.remove(byWho);
    data_->mutex_.unlock();
}
