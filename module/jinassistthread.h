#ifndef JINASSISTTHREAD_H
#define JINASSISTTHREAD_H

#include "jinformatint.h"
#include "jinsingleton.h"
#include "jinsignaledevent.h"
#include "jinmutex.h"
#include "jinhashmap.h"
//这个类虽然叫Thread,但其实不是线程。
//它的设计背景是：
//一个单纯辅助线程处理阻塞调用或者其他费时操作，当它完成任务后需要主线程将
//结果取走，那么如何让主线程取走，以前的办法是轮询，不够聪明也不够及时
//JinAssistThread设计思路：
//定义一个JinMainThread类和全局对象，在先于main的构造中关联主线程，
//主线程以可唤醒方式进行Sleep。JinAssistThread则是唤醒主线程的呼喊者
//主线程被唤醒后立即执行JinAssistThread对应的方法。完成后根据情况决定是否继续睡眠.
//.
class JinMainThread;
class JinAssistThread
{
public:
    friend class JinMainThread;
protected:
    //允许多次awake，只会触发一次，user被最后一次覆盖.
    //也可能被多个线程awake,触发顺序不能预估.
    void awakeMainThread(void *user);
    JinAssistThread(){}
    virtual ~JinAssistThread(); //必须由主线程释放以策安全.
    virtual void OnAwakeCall(void *user) = 0;

};


class JinTimer;
//这个类被设计为用JinMainThread::get的方式使用，无需create和free.
class JinMainThread : public JinSingleton<JinMainThread>
{
public:
    JinMainThread();
    ~JinMainThread();
    bool init();
    void uninit();
    friend class JinAssistThread;
    //将MainThread改成当前调用者这个线程。为安全起见，如果调，要尽早调用
    //针对的是用线程启动整个库的方式
    void changeMainThreadToCurrent();
    //这个睡眠是可以被唤醒的，同时睡眠之前会推动其中的timer工作
    //若返回false表示被标记为退出.
    static bool msleep(unsigned long ms);
    //标记退出,如果使用msleep将得到false返回
    static void notifyQuit();
    //获取timer，可以当作一个全局timer
    //这个timer受限于主线程sleep的频率和时长
    //因此推荐msleep用>=10 <=30
    JinTimer* getTimer();
protected:
    void doAwake(JinAssistThread* byWho, void* user);
    bool doSleep(unsigned long ms);
    void setQuitFlag(bool set){quitFlag_=set;}
    //线程被销毁前，如果有doAwake，需要清理一下避免回调使用到被释放的对象
    void doRemove(JinAssistThread* byWho);

private:
    uint64_t threadID_; //用于验证.
    struct MainData
    {
        JinSignaledEvent event_;
        JinMutex mutex_;
        JinHashMap<JinAssistThread*,void*> awakeMap_;
        JinTimer *timer_;
    } *data_;
    bool quitFlag_;
};

#endif // JINASSISTTHREAD_H
