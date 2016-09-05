#include "usecasedns.h"
#include "jinnetaddress.h"
#include "jinassistthread.h"

CHAIN_USE_CASE(DNS);

UseCaseDNS::UseCaseDNS()
    : donework(0)
{
    errmsg_[0] = 0;
}

UseCaseDNS::~UseCaseDNS()
{

}

bool UseCaseDNS::run()
{
    bool bRet;
    if(before())
    {
        bRet = doing();
    }
    after();

    return bRet;
}

void UseCaseDNS::JinOnDnsParsed(const char *domain, JinNetAddr addr, void *user, uint64_t user64)
{
    JAssert(strlen(domain)>0);
    JAssert(user);
    int split = user64%10;
    void* rtnparse;
    if(addr.isValid()){
        printf("domain|%s => %s\n",domain,addr.toAddr(gDLogBufAddr));
    }
    else{
        printf("domain:%s  failed!\n",domain);
    }
    switch(split)
    {
    case 0:  //baidu
        JAssert(addr.isV4() && addr.isValid());
        break;
    case 1:  //google
        if(!addr.isValid())
        {
            ((UseCaseDNS*)user)->plus();
            return;
        }
        else
            JAssert(addr.isV4());
        break;
    case 2:  //ipv6 google
#if !defined(JWIN) || (_WIN32_WINNT > 0x0600)
        JAssert(addr.isV6());
        break;
#else
        JAssert(addr.isValid()==false);
        ((UseCaseDNS*)user)->plus();
        return;
#endif
    case 3:  //jinzeyu
        JAssert2(false,"jinzeyu.cn is canceled");
        break;
    }

    JinDNS::Get()->vote(domain,addr,false);
    JinNetAddr nAddr;
    do
    {
        rtnparse = JinDNS::Get()->parse(domain,Av4,NULL,user,user64,nAddr);
        JAssert(rtnparse==0 && nAddr.isValid());
        printf("domain:%s => %s\n",domain,nAddr.toAddr(gDLogBufAddr));
        JinDNS::Get()->vote(domain,nAddr,false);
    }
    while(!(nAddr==addr));

    ((UseCaseDNS*)user)->plus();
}

bool UseCaseDNS::before()
{  //过程中JinDnsParser线程没有start就关掉了,发现了JinThread的问题.
    JinMainThread::Get()->init();
    JinDNS::Create();
#ifdef JDEBUG
    testDnsParseCache();
#endif
    JinDNS::Free();
    return true;
}

bool UseCaseDNS::doing()
{
#ifdef JWIN
    const char* localhost6 = "localhost";
#else
    const char* localhost6 = "ip6-localhost";
#endif
    JinDNS::Create();

    JinNetAddr addr[4];
    void* rt[4];
    rt[3] = JinDNS::Get()->parse("www.jinzeyu.cn",Av6,JinOnDnsParsed,this,(uint64_t)0xFFFFFFFF1*4000+3,addr[3]);
    CHECK_ASSERT(rt[3],"jinzeyu,dns parse post.");
    rt[0] = JinDNS::Get()->parse("www.baidu.com",Av4,JinOnDnsParsed,this,(uint64_t)0xFFFFFFFF1*1000+0,addr[0]);
    CHECK_ASSERT(rt[0],"baidu,dns parse post.");
    rt[1] = JinDNS::Get()->parse("www.google.com",Av4,JinOnDnsParsed,this,(uint64_t)0xFFFFFFFF1*2000+1,addr[1]);
    CHECK_ASSERT(rt[1],"google,dns parse post.");
    rt[2] = JinDNS::Get()->parse(localhost6,Av6,JinOnDnsParsed,this,(uint64_t)0xFFFFFFFF1*3000+2,addr[2]);
    CHECK_ASSERT(rt[2],"localhost,dns parse post.");
    CHECK_ASSERT(JinDNS::Get()->cancel(rt[3])==true,"cancel jinzeyu");

    while(donework<3)
    {
        JinMainThread::msleep(10);
    }

    JinDNS::Free();

    return true;
}

void UseCaseDNS::after()
{
    JinMainThread::Get()->uninit();
}
