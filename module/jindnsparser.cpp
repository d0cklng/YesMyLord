#include "jindnsparser.h"
#include "jintimer.h"
#include "jinassert.h"
#include "jinlogger.h"
#include "jinnetstartup.h"
#ifdef JWIN
#include <Ws2tcpip.h>
#else
#include <netdb.h>  //gethostbyname
#endif

/************************************************************************
 *
 * DNS Cache content.不对外暴露，强化封装
 *
 ************************************************************************/
const static uint32_t kMaxHostNameLen = 128;
const static uint32_t kCacheTimeToLive = 3600;  //second
typedef struct StuDnsCacheContent
{
    char hostName[kMaxHostNameLen + 1];
    int8_t weigh[kMaxDnsCacheIpCount+1];  //-1 conn fail, useless; 0 no try, unknown; 1 useful; 2 first record.
    JinNetAddr ipList[kMaxDnsCacheIpCount];
    uint32_t ipCount;
    long int startTime;   /* Set时的时间戳。second */
    struct StuDnsCacheContent* next;  //link with other which has same key.
} DnsCacheContent,*PDnsCacheContent;

//越大越占空间，查找性能越好.为提高效率，务必使用特定的质数如7,53,97,193,289,769,1543...
const static uint32_t kHashKeyPoolSize = 53;
/**
 * 此类下载库初始化和调用，不涉及到多线程操作，不考虑多线程安全问题
 */
class DnsParseCache// : public JinSingleton<DnsParseCache>
{
public:
    DnsParseCache();
    ~DnsParseCache();
    int Get(const char *domain, JinNetAddr ipList[], size_t* pszListSize=NULL); //listValidSize==NULL时按1处理.
    int Set(const char *domain, JinNetAddr ipList[], size_t szListSize);
    int NotifyIpAddressValid(const char *domain, const JinNetAddr& ipAddr, bool valid);
private:
    struct StuDnsCacheContent* find(const char* domain, size_t& idxOfKeyPool);
    struct StuDnsCacheContent* washTimeExpire(size_t idxOfKeyPool,uint32_t timenow);
    struct StuDnsCacheContent* mHashKeyPool[kHashKeyPoolSize];
private:
    unsigned int keyHash(const char *domain, size_t length=0) const;
};
typedef JinSingleton<DnsParseCache> DnsCache;

/* **********************
 * 这里开始才是JinDNSCache
 * *********************/

static const size_t kDnsCacheKeepTimeMs = 5 * 60 * 1000;
JinDNSParser::JinDNSParser(JinTimer* timer)
    : isInit_(false)
    , callbackTimerID_(0)
    //, resultUpdate_(false)
    , timer_(timer)
{
    JinNetStartup::Create();
    DnsCache::Create();
    this->setThreadName("JinDnsParser");
}

JinDNSParser::~JinDNSParser()
{
    mainUninit();
    this->stop();
    DnsCache::Free();
    JinNetStartup::Free();
}

void *JinDNSParser::parse(const char *domain, ParseAddrVer av, JinOnDnsParsed callback,
                          void *user, uint64_t user64, JinNetAddr &addr)
{
    //JAssert(addr.isValid()==false);  //传入的必须是非法的，这样才能正确判断.
    addr = JinNetRawAddr4(0);
    if(!isInit_)
    {
        if(!mainInit()) return NULL;
    }
    if(0 == DnsCache::Get()->Get(domain,&addr))
    {
        JDLog("direct got %s => %s",domain,addr.toAddr(gDLogBufAddr));
        return NULL;
    }

    DnsQuery* query = JNew(DnsQuery);
    if(!query) return NULL;
    query->state = DnsQuery::Queued;
    query->domain = domain;
    query->callback = callback;
    query->av = av;
    query->user = user;
    query->user64 = user64;
    query->addrCount = 0;
    mutex_.lock();
    if(!queueLine_.push_back(query))
    {
        JDelete(query);
        mutex_.unlock();
        return NULL;
    }
    mutex_.unlock();
    if(timer_ && callbackTimerID_ == 0)
    {
        callbackTimerID_ = timer_->start(10,true,JinDNSParser::sTimeout,this);
    }
    signal_.set();
    return query;

}

JinNetAddr JinDNSParser::ifcache(const char *domain)
{
    JinNetAddr addr;
    if(!isInit_)
    {
        if(!mainInit()) return addr;
    }
    if(0 == DnsCache::Get()->Get(domain,&addr))
    {
        JDLog("direct got %s => %s",domain,addr.toAddr(gDLogBufAddr));
    }
    return addr;
}

bool JinDNSParser::cancel(void *query)
{
    mutex_.lock();
    DnsQuery* tranQuery = (DnsQuery*)query;
    //如果在callbackTmpList回调时进入此处，已经不能通过queueLine_移除，只能
    //通过抹除callback。由于回调和cancel都是同一线程，不会产生更多线程问题.
    tranQuery->callback = NULL;
    JinLinkedList<DnsQuery*>::iterator it = queueLine_.begin();
    while(it!=queueLine_.end())
    {
        DnsQuery* tquery = *it;
        if(tranQuery == tquery)
        {
            queueLine_.erase(it);
            mutex_.unlock();
            JDelete(tquery);
            return true;
        }
        ++it;
    }
    mutex_.unlock();
    return false;
}

void JinDNSParser::vote(const char *domain, const JinNetAddr &ipAddr, bool valid)
{
    DnsCache::Get()->NotifyIpAddressValid(domain,ipAddr,valid);
}

void JinDNSParser::timeout(TimerID id)
{
    JAssert(callbackTimerID_ == id);
    OnAwakeCall((void*)(-1));
}

bool JinDNSParser::mainInit()
{
    if(isInit_)return true;
    if(!signal_.init())return false;
    //if(!cacheMap_.init(32))return false;
    //虽然线程while遇见！isInit_即退出，但是signal_并未set
    if(!this->start())return false;
    isInit_ = true;
    return true;
}

void JinDNSParser::mainUninit()
{
    if(isInit_)
    {
        if(callbackTimerID_){
            timer_->cancel(callbackTimerID_);
            callbackTimerID_ = 0;
        }

        mutex_.lock();{
            JinLinkedList<DnsQuery*>::iterator it = queueLine_.begin();
            while(it!=queueLine_.end())
            {
                DnsQuery* tquery = *it;
                JDelete(tquery);
                ++it;
            }
            queueLine_.clear();
        }
        mutex_.unlock();
        isInit_ = false; //提前设置用于使线程退出.
        signal_.set();
    }
}

bool JinDNSParser::period()
{   //取队列中未处理的进行处理，完后更新队列，但是不增删队列项，不操作cacheMap_
    bool bContinueResolve = false;
    do
    {
        if(!bContinueResolve){
            signal_.wait(WAITUNTILWORLDEND);
        }
        bContinueResolve = false;
        JinBuffer domain;
        ParseAddrVer av;
        mutex_.lock();{  //段落--取domain用于解析.
            DnsQuery* query = NULL;
            JinLinkedList<DnsQuery*>::iterator it = queueLine_.begin();
            while(it != queueLine_.end())
            {
                query = *it;
                ++it;
                if(query->state != DnsQuery::Queued)
                {
                    query = NULL;
                    continue;
                }
                break;
            }
            if(query){
                domain = query->domain;
                av = query->av;
                bContinueResolve = true;
            }
        }
        mutex_.unlock();
        if(domain.length()==0)continue;
        size_t addrRtnCount = kMaxDnsCacheIpCount;
        JinNetAddr addr[kMaxDnsCacheIpCount];
        bool bResolveSucc = parseDomain(domain.cstr(),addr,addrRtnCount,av);

        mutex_.lock();{  //段落--更新queueLine_
            JinLinkedList<DnsQuery*>::iterator it = queueLine_.begin();
            while(it != queueLine_.end())
            {
                DnsQuery* c = *it;
                if(c->state == DnsQuery::Queued &&
                        c->domain.length() == domain.length() &&
                        memcmp(c->domain.buff(),domain.buff(),domain.length())==0)
                {
                    if(bResolveSucc)
                    {
                        c->addrCount = addrRtnCount;
                        for(size_t i=0;i<addrRtnCount;i++)
                        {
                            c->addr[i] = addr[i];
                        }
                        c->state = DnsQuery::Done;
                    }
                    else
                    {
                        c->state = DnsQuery::Failed;
                    }
                }
                ++it;
            }
        }
        mutex_.unlock();

        //domain.length()>0 不管结果，都表示解析过了.
        if(!timer_ && domain.length()) this->awakeMainThread(NULL);

    }
    while(isInit_);
    return false;
}

bool JinDNSParser::parseDomain(const char *host, JinNetAddr addr[], size_t &len, ParseAddrVer addrVer)
{
    JDLog("host=%s, addrVer=%d",host,(int)addrVer);
#if defined(JWIN) && (_WIN32_WINNT < 0x0600)
/*   目前已知gethostbyname,不知道WINNT>0x0600能不能用getaddrinfo
    char tmp_buff[1024] = {0};
    struct hostent tmp_hostent;
    int  tmp_errono = 0;
    struct hostent* phostent = NULL;
    gethostbyname_r(host, &tmp_hostent, tmp_buff, sizeof(tmp_buff), &phostent, &tmp_errono);
*/
    struct hostent *phostent = gethostbyname(host);

    if (phostent)
    {
        size_t index = 0,count = 0;
        for(index = 0; phostent->h_addr_list[index]&& index < len && index < kMaxDnsCacheIpCount ; index ++)
        {
            //addr[index] = ((struct sockaddr_in *)(phostent->h_addr_list[index]))->sin_addr;
            if(AF_INET == phostent->h_addrtype && (addrVer&Av4))
            {
                addr[count++] = *((struct in_addr *)(phostent->h_addr_list[index]));
            }
            else if(AF_INET6 == phostent->h_addrtype && (addrVer&Av6))
            {
                addr[count++] = *((struct in6_addr *)(phostent->h_addr_list[index]));
            }
        }
        len = count;
    }
    return (phostent != NULL && len > 0);
#else
    int ret = 0;
    size_t i = 0;
    struct addrinfo hints, *res, *idx;
    memset(&hints, 0, sizeof(struct addrinfo));
    //if(!getv6) hints.ai_family = AF_INET;  //不指定就可能会有v6地址.
    if((addrVer&Av46) != Av46){
        if(addrVer&Av6)hints.ai_family = AF_INET6;
        if(addrVer&Av4)hints.ai_family = AF_INET;
    }
    //hints.ai_family = AF_UNSPEC;  //?
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;  //?
    ret = getaddrinfo(host, NULL, &hints, &res);
    if(ret == 0)
    {
        for (i=0,idx = res; idx != NULL && i<len && i<kMaxDnsCacheIpCount; idx = idx->ai_next, ++i)
        {
            if(AF_INET == idx->ai_family)
            {
                addr[i] = ((struct sockaddr_in *)(idx->ai_addr))->sin_addr;
            }
            else if(AF_INET6 == idx->ai_family)
            {
                addr[i] = ((struct sockaddr_in6 *)(idx->ai_addr))->sin6_addr;
            }
        }
        len = i;
        freeaddrinfo(res);
        JDLog("host=%s, ret=%d, count=%d",host,ret,len);
    }
    else
    {
        JELog("getaddrinfo %s, ret=%d",host,ret);
    }
    return (0 == ret && len>0);
#endif
}


void JinDNSParser::OnAwakeCall(void *user)
{
    UNUSED(user);
    //bool bCacheArrange = true;
    //把queueLine_的内容先上锁弄出来解锁，然后慢慢回调处理.
    JinLinkedList<DnsQuery*> callbackTmpList;
    mutex_.lock();{
        JinLinkedList<DnsQuery*>::iterator it = queueLine_.begin();
        //if(queueLine_.size() == 0) bCacheArrange = false;
        while(it!=queueLine_.end())
        {
            DnsQuery* query = *it;
            if(query->state == DnsQuery::Queued){
                //bCacheArrange = false;
                ++it;
                continue;
            }
            else
            {
                it = queueLine_.erase(it);
                callbackTmpList.push_back(query);
            }
        }
        if(timer_ && queueLine_.size() == 0)
        {
            timer_->cancel(callbackTimerID_);
            callbackTimerID_ = 0;
        }
    }
    mutex_.unlock();

    JinLinkedList<DnsQuery*>::iterator itcb = callbackTmpList.begin();
    while(itcb!=callbackTmpList.end())
    {
        DnsQuery* tquery = *itcb;
        if(tquery->state == DnsQuery::Done)
        {
            JAssert(tquery->addrCount>0);
            JDLog("%s => %s[%u]",tquery->domain.cstr(),tquery->addr[0].toAddr(gDLogBufAddr),tquery->addrCount);
            DnsCache::Get()->Set(tquery->domain.cstr(),tquery->addr,tquery->addrCount);
        }
        //经常要想一个问题，如果回调的时候删掉了对象.
        //在cancel函数中，把callback置0避免了调用.
        //更早的操作会从queueLine_中移除.
        //--尽管如此，还是不能在回调中删除JinDNSCache本身.
        if(tquery->callback)
        {
            JinNetAddr rtnAddr;
            if(tquery->addrCount>0)rtnAddr = tquery->addr[0];
            tquery->callback(tquery->domain.cstr(),rtnAddr,tquery->user,tquery->user64);
        }
        tquery->callback = NULL; tquery->user = NULL; tquery->user64 = 0;
        JDelete(tquery);
        ++itcb;
    }
    callbackTmpList.clear();
}

void JinDNSParser::sTimeout(TimerID id, void *user, uint64_t user64)
{
    UNUSED(user64);
    JinDNSParser *dnscache = (JinDNSParser*)user;
    dnscache->timeout(id);
}


/* ===================== DNS cache ==================
 * 刚开始Get返回都是首记录,权值是2.如果没有2,那么会轮换返回. 如果首记录权1,则只在权1中轮换
 * 被通知可用的ipAddr会变成权1,否则是权-1.没尝试过的是0
 *
 ==================================================== */

DnsParseCache::DnsParseCache()
{
    memset(mHashKeyPool,0,sizeof(PDnsCacheContent)*kHashKeyPoolSize);
}

DnsParseCache::~DnsParseCache()
{
    long int timesnow = JinDateTime::getTime();
    timesnow = timesnow + kCacheTimeToLive + 1;
    for(size_t i = 0;i<kHashKeyPoolSize;++i)
    {
        if(mHashKeyPool[i]==NULL) continue;
#ifdef DEBUG
        PDnsCacheContent content = washTimeExpire(i,timesnow);
        JAssert(content == NULL);
#else
        washTimeExpire(i,timesnow);
#endif
    }
}

int DnsParseCache::Get(const char *domain, JinNetAddr ipList[], size_t *pszListSize)
{
    if(domain==NULL || strlen(domain)>kMaxHostNameLen) return -10;
    size_t idxOfKeyPool=kHashKeyPoolSize;
    PDnsCacheContent content = find(domain,idxOfKeyPool);
    JAssert(idxOfKeyPool<kHashKeyPoolSize);
    if(content == NULL)
    {
        JDLog("get %s not found.",domain);
        if(pszListSize) *pszListSize = 0;
        return -1;
    }
    else
    {
        size_t rtnAvaSize = pszListSize?(*pszListSize):1;
        size_t rtnRealSize = 0;
        for(size_t i = 0;i<rtnAvaSize&&i<content->ipCount;++i)
        {
            ipList[i] = content->ipList[i];
            rtnRealSize = i+1;
        }
        if(pszListSize) *pszListSize = rtnRealSize;
        //如果weigh[0]是1或2，不调整顺序返回
        int8_t& idxOfOrderTry = content->weigh[kMaxDnsCacheIpCount];
        JDLog("get %s hit.rtnAvaSize=%u, rtnRealSize=%u, ipCount=%u, idxOfOrderTry=%hhd, weigh[0]=%hhd",
                  domain,rtnAvaSize,rtnRealSize,content->ipCount,idxOfOrderTry,content->weigh[0]);
        if(content->ipCount > (unsigned)idxOfOrderTry)
        {  //idxOfOrderTry,用于指导不同策略返回不同值.
            if(content->weigh[0] <= 0)
            {
                if(idxOfOrderTry>0 && content->weigh[idxOfOrderTry]<=0)
                {  //注意:只调整返回值，未调整内部次序
                    JinNetAddr swf = ipList[0];
                    ipList[0] = content->ipList[idxOfOrderTry];
                    if((unsigned)idxOfOrderTry<rtnAvaSize)ipList[idxOfOrderTry] = swf;
                }
            }
            if(content->weigh[0] == 1)
            {  //轮换提供已被验证可连接（weigh1）的ip
                if(content->weigh[idxOfOrderTry] != 1)
                { //idxOfOrderTry对应位置发生变化，已经不是1了，提前重置.
                    idxOfOrderTry = 0;
                }
                if(idxOfOrderTry>0)
                {
                    JinNetAddr swf = ipList[0];
                    ipList[0] = content->ipList[idxOfOrderTry];
                    if((unsigned)idxOfOrderTry<rtnAvaSize)ipList[idxOfOrderTry] = swf;
                }
            }
            ++idxOfOrderTry;
            if((unsigned)idxOfOrderTry>=content->ipCount)
            {
                idxOfOrderTry = 0;
            }
        }
    }
    return 0;
}

int DnsParseCache::Set(const char *domain, JinNetAddr ipList[], size_t szListSize)
{
    if(domain==NULL || strlen(domain)>kMaxHostNameLen ||
            szListSize==0 || szListSize>0xFF) return -10;
    size_t idxOfKeyPool=kHashKeyPoolSize;
    PDnsCacheContent content = find(domain,idxOfKeyPool);
    JAssert(idxOfKeyPool<kHashKeyPoolSize);
    if(content==NULL)
    {
        JDLog("insert new record:%s|%s poolIndex=%u,ipCount=%u",
                  domain,ipList[0].toAddr(gDLogBufAddr),(unsigned)idxOfKeyPool,(unsigned)szListSize);
        content = JNew(DnsCacheContent);
        if(!content) return -1;
        strcpy(content->hostName,domain);
        memset(content->weigh,0,sizeof(int8_t)*kMaxDnsCacheIpCount);
        content->weigh[0] = 2;  //first record
        content->weigh[kMaxDnsCacheIpCount] = 0;
        content->startTime = JinDateTime::getTime();
        for(size_t i=0;i<kMaxDnsCacheIpCount&&i<szListSize;++i)
        {
            content->ipList[i] = ipList[i];
            content->ipCount = i+1;
        }
        if(mHashKeyPool[idxOfKeyPool] == NULL)
        {
            content->next = NULL;
            mHashKeyPool[idxOfKeyPool] = content;
        }
        else
        {
            JDLog("record before %s",mHashKeyPool[idxOfKeyPool]->hostName);
            content->next = mHashKeyPool[idxOfKeyPool];
            mHashKeyPool[idxOfKeyPool] = content;
        }

    }
    else
    {
        long int timesnow = JinDateTime::getTime();
        JDLog("duplicate set %s|%s, now=%ld,start=%u",
                  domain,ipList[0].toAddr(gDLogBufAddr),timesnow,content->startTime);
        if(timesnow - content->startTime >5) //do not change.
        {
            //refresh anyway.
            memset(content->weigh,0,sizeof(int8_t)*kMaxDnsCacheIpCount);
            content->weigh[0] = 2;  //first record
            content->weigh[kMaxDnsCacheIpCount] = 0;
            content->startTime = JinDateTime::getTime();
            for(size_t i=0;i<kMaxDnsCacheIpCount&&i<szListSize;++i)
            {
                content->ipList[i] = ipList[i];
                content->ipCount = (uint32_t)i+1;
            }
        }
    }
    return 0;
}

int DnsParseCache::NotifyIpAddressValid(const char *domain, const JinNetAddr &ipAddr, bool valid)
{
    if(domain==NULL || strlen(domain)>kMaxHostNameLen) return -10;
    JDLog("notify %s ip=%s valid=%c",domain,ipAddr.toAddr(gDLogBufAddr),valid?'Y':'N');
    size_t idxOfKeyPool=kHashKeyPoolSize;
    PDnsCacheContent content = find(domain,idxOfKeyPool);
    JAssert(idxOfKeyPool<kHashKeyPoolSize);
    if(content==NULL)
    {
        JDLog("cache not found: %s", domain);
        return -1;
    }
    size_t countOfStartW1 = 0;  //这个变量记录weigh1和2开头的记录数，如果有记录要提升，可以找到插入点
    for(size_t i=0;i<content->ipCount;++i)
    {
        if(i==countOfStartW1 && content->weigh[i]>=1) ++countOfStartW1;
        if(content->ipList[i] == ipAddr)
        {
            int8_t newWeigh = valid?1:-1;
            int8_t oldWeigh = content->weigh[i];
            if(oldWeigh == 2){ oldWeigh = 0; }
            size_t x = i;
            if(newWeigh > oldWeigh && i > countOfStartW1)
            { //把记录插到所有weigh1的后面
                JDLog("swf %u|%s-->%u|%s for upgrade.",x,content->ipList[x].toAddr(gDLogBufAddr),
                          countOfStartW1,content->ipList[countOfStartW1].toAddr(gDLogBufAddr2));
                while(x>countOfStartW1)
                {
                    content->ipList[x] = content->ipList[x-1];
                    content->weigh[x] = content->weigh[x-1];
                    --x;
                }
                content->ipList[x] = ipAddr;
                content->weigh[x] = newWeigh;

                int8_t& idxOfOrderTry = content->weigh[kMaxDnsCacheIpCount];
                if(content->weigh[idxOfOrderTry]!=1) idxOfOrderTry=0;
            }
            else if(oldWeigh > newWeigh && i+1 < content->ipCount)
            { //把记录挪到队尾.
                JDLog("swf %u|%s-->%u|%s f downgrade.",x,content->ipList[x].toAddr(gDLogBufAddr),
                          content->ipCount-1,content->ipList[content->ipCount-1].toAddr(gDLogBufAddr2));

                int8_t& idxOfOrderTry = content->weigh[kMaxDnsCacheIpCount];
                if(x<(size_t)idxOfOrderTry && idxOfOrderTry>0) --idxOfOrderTry;

                while(x<content->ipCount-1)
                {
                    content->ipList[x] = content->ipList[x+1];
                    content->weigh[x] = content->weigh[x+1];
                    ++x;
                }
                content->ipList[x] = ipAddr;
                content->weigh[x] = newWeigh;

            }
            else
            {
                content->weigh[x] = newWeigh;
            }
            break;
        }
    }
    return 0;
}


PDnsCacheContent DnsParseCache::find(const char *domain, size_t& idxOfKeyPool)
{
    JAssert(domain && strlen(domain)<=kMaxHostNameLen && domain[0]!=0);
    idxOfKeyPool = keyHash(domain) % kHashKeyPoolSize;
    PDnsCacheContent content = mHashKeyPool[idxOfKeyPool];
    if(content)
    {
        long int timesnow = JinDateTime::getTime();
        content = washTimeExpire(idxOfKeyPool,timesnow);
    }
    while(content)
    {
        if(strcmp(content->hostName,domain)==0) return content;
        content = content->next;
    }
    return NULL;
}

PDnsCacheContent DnsParseCache::washTimeExpire(size_t idxOfKeyPool, uint32_t timenow)
{  //返回值并非被移除的项.
    PDnsCacheContent prev = NULL;
    PDnsCacheContent content = mHashKeyPool[idxOfKeyPool];
    while(content)
    {
        if(timenow - content->startTime > kCacheTimeToLive)
        {
            JDLog("cache %s wash out.idx=%u, start=%u, now=%u",
                      content->hostName,idxOfKeyPool,content->startTime,timenow);
            if(prev) prev->next = content->next;
            if(content==mHashKeyPool[idxOfKeyPool])
            {
                mHashKeyPool[idxOfKeyPool] = content->next;
                JDelete(content);
                content = mHashKeyPool[idxOfKeyPool];
            }
            else
            {
                JDelete(content);
                content = prev?prev->next:NULL;
            }
        }
        else
        {
            prev = content;
            content = content->next;
        }
    }
    return mHashKeyPool[idxOfKeyPool];
}

unsigned int DnsParseCache::keyHash(const char *domain, size_t length) const
{
    unsigned int hash = 0;
    size_t sz = length?length:strlen(domain);
    const unsigned char* ch = (const unsigned char*)domain;
    while ( sz-- > 0 )
    {
        hash = hash * 131 + *ch;
        ++ch;
    }
    return (hash & 0x7FFFFFFF);
}









#ifdef JDEBUG
void testDnsParseCache()
{
    const static uint32_t baseip = 0x1010101;
    const char *testDomain = "test.cn";
    int ret = 0;
    JinNetAddr ipAddr[10];
    for(int i=0;i<10;i++)
    {
        ipAddr[i] = baseip * i;
    }
    ret = DnsCache::Get()->Set(testDomain,ipAddr,10);
    JinNetAddr rtnAddr;
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);   JAssert(rtnAddr.addr4().s_addr == 0);
    rtnAddr=(uint32_t)99999999;   //测试首记录未被投票时，是否始终是首记录
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);   JAssert(rtnAddr.addr4().s_addr == 0);

    rtnAddr=baseip*7;     //测试好记录是否插在其他好记录或首记录之后
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);
    rtnAddr=baseip*0;     //测试首记录已经在好记录范围内，不移动
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);  //0,7
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);   JAssert(rtnAddr.addr4().s_addr == 0);
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*7);
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*0);
    rtnAddr=baseip*8;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);  //0,7,8
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*7);
    rtnAddr=baseip*0;     //测试idx=2临界时移除[0]记录
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false); //7,8
    rtnAddr=baseip*9;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);  //7,8,9
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*8);
    rtnAddr=baseip*9;    //测试idx=2临界时移除[2]临界记录
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false); //7,8
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*7);
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*8);
    rtnAddr=baseip*1;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);  //7,8,1
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*1);

    rtnAddr=baseip*2;   //测试idx当前位发生变化，但是不影响其值，没有归0
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false);  //7,8,1____3456___0,9,2
    rtnAddr=baseip*3;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false);  //7,8,1____456__0,9,2,3
    rtnAddr=baseip*6;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,true);  //7,8,1,6____45__0,9,2,3
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*6);
    rtnAddr=baseip*1;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false);
    rtnAddr=baseip*6;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false);
    rtnAddr=baseip*7;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false);
    rtnAddr=baseip*8;
    DnsCache::Get()->NotifyIpAddressValid(testDomain,rtnAddr,false); // ________45__09231678
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*4);
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*5);
    ret = DnsCache::Get()->Get(testDomain,&rtnAddr);  JAssert(rtnAddr.addr4().s_addr == baseip*0);


    size_t ct=12;
    ret = DnsCache::Get()->Get(testDomain,ipAddr,&ct);  JAssert(ct==10);

    JAssert(ipAddr[0].addr4().s_addr == baseip*9);  //9<==>4 change
    JAssert(ipAddr[1].addr4().s_addr == baseip*5);
    JAssert(ipAddr[2].addr4().s_addr == baseip*0);
    JAssert(ipAddr[3].addr4().s_addr == baseip*4);
    JAssert(ipAddr[4].addr4().s_addr == baseip*2);
    JAssert(ipAddr[5].addr4().s_addr == baseip*3);
    JAssert(ipAddr[6].addr4().s_addr == baseip*1);
    JAssert(ipAddr[7].addr4().s_addr == baseip*6);
    JAssert(ipAddr[8].addr4().s_addr == baseip*7);
    JAssert(ipAddr[9].addr4().s_addr == baseip*8);
    JAssert(ret == 0);

}
#endif

