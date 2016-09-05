#ifndef JINDNSCACHE_H
#define JINDNSCACHE_H

#include "jinnetaddress.h"
#include "jinformatint.h"
#include "jinthread.h"
#include "jinsingleton.h"
#include "jinhashmap.h"
#include "jinlinkedlist.h"
#include "jinbuffer.h"
#include "jindatatime.h"
#include "jintimer.h"
#include "jinsignaledevent.h"
#include "jinmutex.h"
#include "jinassistthread.h"

//目前实现没有用特别的方法，只是在线程里排队调用gethostbyname避免阻塞
static const size_t kMaxDnsCacheIpCount = 12;
typedef void (*JinOnDnsParsed)(const char* domain, JinNetAddr addr, void* user, uint64_t user64);
enum ParseAddrVer{Av6=2,Av4=4,Av46=6};
class JinDNSParser :
        protected JinThread,
        public JinAssistThread
{
public:
    //如果指定这个timer,类将在timer中设置触发点，调用者需要喂养timer
    //如果不提供timer,类将使用JinMainThread & JinAssistThread机制.
    //主线程必须使用JinMainThread::doSleep进行休眠.
    JinDNSParser(JinTimer* timer=NULL);
    virtual ~JinDNSParser();

    //如果返回NULL,用addr.isValid判断是否成功; 否则代表异步去了,返回值用来取消异步.
    void* parse(const char* domain, ParseAddrVer av, JinOnDnsParsed callback,
                void* user, uint64_t user64, JinNetAddr &addr);

    JinNetAddr ifcache(const char* domain);

    //仅当parse返回非0，addr isValid==false时，其返回值可以这里做参数用来取消
    //且如果回调过结果了，上述的返回值也不能再用了。
    bool cancel(void* cache);
    void vote(const char *domain, const JinNetAddr& ipAddr, bool valid);
    void timeout(TimerID id);
protected:
    bool mainInit(); //init at main thread.
    void mainUninit();
    //virtual bool init();
    //当需要线程停止时，返回false.
    virtual bool period();  //vv下面函数是线程内执行的
    virtual bool parseDomain(const char* host, JinNetAddr addr[], size_t& len, ParseAddrVer addrVer);
    //调用中避免删掉本对象自身.非要删的话应该也没问题，
    //设计时有所考虑，但不可大意.
    void OnAwakeCall(void *user);
    //virtual void uninit();
    static void sTimeout(TimerID id, void *user, uint64_t user64);
private:
    struct DnsQuery
    {
        enum DcState{Queued,Failed,Done} state;
        JinBuffer domain;
        JinNetAddr addr[kMaxDnsCacheIpCount];
        size_t addrCount;
        JinOnDnsParsed callback;
        ParseAddrVer av;
        uint64_t user64;
        void* user;
    };

    volatile bool isInit_;
    //JinHashMap<const char*, dnsCache*> cacheMap_;
    //JinLinkedList<dnsCache*> queueLine_;
    JinLinkedList<DnsQuery*> queueLine_;
    TimerID callbackTimerID_;
    JinSignaledEvent signal_;
    JinMutex mutex_;
    //volatile bool resultUpdate_;
    JinTimer* timer_;
};

#ifdef JDEBUG
extern void testDnsParseCache();
#endif
typedef JinSingleton<JinDNSParser> JinDNS;
#endif // JINDNSCACHE_H
