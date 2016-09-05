#include "usecasebase.h"
#include "jindatatime.h"
#include "jinhashmap.h"
#include "jinlogger.h"
#include "jinstring.h"
#include "jinlinkedlist.h"
#include "jinthread.h"
#include "jinsignaledevent.h"
#include "jincompress.h"
#include "jinfiledir.h"
#include "jinnetstartup.h"
#include "jinbuffer.h"
#include "jinendian.h"
#include "jinkey160.h"
#include <stdlib.h>
#include <map>
#ifndef JWIN
#include <unistd.h>
#endif

CHAIN_USE_CASE(Base);

UseCaseBase::UseCaseBase()
{
    errmsg_[0] = 0;
}

UseCaseBase::~UseCaseBase()
{

}

bool UseCaseBase::run()
{
    runMiscellaneous();
    runJinString();
    runJinLinkedList();
    runJinhashmap();
    runJinEvent();
    runJinDir();
    runJinCompress();
    return true;
}

void UseCaseBase::runMiscellaneous()
{
    uint16_t port = 7750;
    port = jhtons(port);
    JAssert(port == 17950);
    port = jntohs(port);
    JAssert(port == 7750);
    uint32_t ip = 65535000;
    ip = jhtonl(ip);
    JAssert(ip == 419227395);
    ip = jntohl(ip);
    JAssert(ip == 65535000);

    if(true)
    {
        JinBuffer jb;
        jb = "abcd";
        JinBuffer jb2(jb);
        jb2.buff()[1] = 'e';
        JinBuffer jb3 = jb2;
        jb3.buff()[2] = 'f';
        JAssert(strcmp((const char*)jb.buff(),"aefd")==0);
        jb2.clear();

        char* leak = (char*)JMalloc(10);
        strcpy(leak,"leak_test");
        char* beEatten = (char*)JMalloc(20);
        strcpy(beEatten,"be eatten.");
        JinBuffer jb4;
        jb4.eat(beEatten);
        jb2 = jb4;
        jb2.buff()[0]=0;
        JAssert(strlen(jb4)==0);
    }

    //printf("========\n");

    JinBuffer *jb5 = 0;
    jb5 = JNew(JinBuffer);
    *jb5 = "aaaaaaaaaaaaa";
    JDelete(jb5);

    jb5 = JNewAry(JinBuffer,10);
    jb5[0] = "00000000000";
    jb5[2] = "22222222222";
    jb5[3] = "33333333333";
    jb5[9] = "99999999999";
    jb5[4] = "44444444444";
    JDelAry(jb5);
    JAssert3(jb5!=0,"jb5 not null,test JAssert",jb5);
/*
    hash_state hs;
    unsigned char bufout[20];
    unsigned char encout[42];
    unsigned long enclen = 42;
    //FakeHash("1111111111",bufout);
    md5_init(&hs);
    md5_process(&hs,(const unsigned char*)"jinzeyu&liumingzhu",18);
    md5_done(&hs,bufout);

    base32_encode(bufout,16,encout,&enclen);
    JAssert(strcmp((const char*)encout,"U4L7AOYRP2UAE7NC6EPWTBHUNE======")==0);
    //printf((const char*)encout);
*/
}

void UseCaseBase::runJinhashmap()
{
    if(true)
    {
        bool ret;
        int retv=0;
        JinHashMap<const char*,int> jhm;
        retv = jhm.get("NULL",0);
        JAssert(retv == 0);
        const char* ch1 = "jhm.insert";
        const char* ch2 = "jhm.insert2";
        const char* ch3 = "jhm.insert3";
        const char* ch4 = "jhm.insert4";
        const char* ch5 = "jhm.insert5";
        const char* ch6 = "jhm.dupli6";
        //void *v1=(void *)1;void *v2=(void *)2;void *v3=(void *)3;
        ret = jhm.insert(ch1,1);  JAssert(ret==true);
        ret = jhm.insert(ch1,2);  JAssert(ret==false);
        ret = jhm.insert(ch2,8);  JAssert(ret==true);
        ret = jhm.set(ch2,2);    JAssert(ret==true);
        ret = jhm.set(ch3,3);
        ret = jhm.set(ch4,4);
        ret = jhm.set(ch5,5);
        ret = jhm.set(ch6,6);
        ret = jhm.remove(ch6);  JAssert(ret==true);
        ret = jhm.remove(ch6);  JAssert(ret==false);

        ret = jhm.has(ch1);  JAssert(ret==true);
        ret = jhm.has(ch2);  JAssert(ret==true);
        ret = jhm.has(ch6);  JAssert(ret==false);
        int dv = 0;
        retv = jhm.get(ch1,dv);  JAssert(retv == 1);
        retv = jhm.get(ch2,dv);  JAssert(retv == 2);
        retv = jhm.get(ch6,dv);  JAssert(retv == 0);
        JAssert(jhm.size() == 5);


        int idx=0;
        char buf[10];
        JinHashMap<const char*,int>::iterator itm = jhm.begin();
        while(itm!=jhm.end())
        {
            int num = (int)itm.second();
            buf[idx++]=0x30+(char)num;
            ++itm;
        }
        buf[idx++] = '\0';
        jhm.clear(); JAssert(jhm.size() == 0);
        //JAssert2(strcmp(buf,"12345")==0,"JinHashMap");
        JinString str(buf);
        JAssert(str.find("1")!=STRNPOS);
        JAssert(str.find("2")!=STRNPOS);
        JAssert(str.find("3")!=STRNPOS);
        JAssert(str.find("4")!=STRNPOS);
        JAssert(str.find("5")!=STRNPOS);
        JAssert(jhm.size()==0);
    }


    //TODO 获知内存消耗量!.
    bool runMapTimeTest = false;
    for(int testCount = 10;testCount<=100000 && runMapTimeTest;testCount*=10)
    {
        unsigned int smem[2];
        T64 stdTick[5];
        {
            smem[0] = TestHelper::get()->getMemory();
            std::map<uint64_t, int> stdmap;
            stdTick[0] = JinDateTime::tickCount();
            for(int i=1;i<=testCount;i++)
            {
                stdmap[i*10000] = i;
            }
            stdTick[1] = JinDateTime::tickCount();
            smem[1] = TestHelper::get()->getMemory();
            for(int i=1;i<=testCount;i++)
            {
                JAssert(stdmap.find(i*10000)!=stdmap.end());
            }
            stdTick[2] = JinDateTime::tickCount();
            for(int i=1;i<=testCount;i++)
            {
                stdmap.erase(i*10000);
            }
            stdTick[3] = JinDateTime::tickCount();
            JAssert(stdmap.size()==0);
        }
        stdTick[4] = JinDateTime::tickCount();

        unsigned int jmem[2];
        T64 jinTick[5];
        {
            jmem[0] = TestHelper::get()->getMemory();
            JinHashMap<uint64_t, int> jinmap;
            //jinmap.init(testCount);  预测空间 减少rebuild内部结构的时间.
            jinTick[0] = JinDateTime::tickCount();
            for(int i=1;i<=testCount;i++)
            {
                jinmap.set(i*10000,i);
            }
            jinTick[1] = JinDateTime::tickCount();
            jmem[1] = TestHelper::get()->getMemory();
            for(int i=1;i<=testCount;i++)
            {
                JAssert(jinmap.find(i*10000)!=jinmap.end());
            }
            jinTick[2] = JinDateTime::tickCount();
            for(int i=1;i<=testCount;i++)
            {
                jinmap.remove(i*10000);
            }
            jinTick[3] = JinDateTime::tickCount();
            JAssert(jinmap.size()==0);
        }
        jinTick[4] = JinDateTime::tickCount();

        JELog("std-map [%d][%u] \t s%llu \t f%llu \t d%llu \t c%llu",testCount,smem[1]-smem[0],
              stdTick[1]-stdTick[0],stdTick[2]-stdTick[1],stdTick[3]-stdTick[2],stdTick[4]-stdTick[3]);
        JELog("jin-map [%d][%u] \t s%llu \t f%llu \t d%llu \t c%llu",testCount,jmem[1]-jmem[0],
              jinTick[1]-jinTick[0],jinTick[2]-jinTick[1],jinTick[3]-jinTick[2],jinTick[4]-jinTick[3]);
    }
}

void UseCaseBase::runJinLinkedList()
{
    JinLinkedList<int> jll;
    JAssert(jll.isNull());
    jll.push_back(3);
    jll.push_back(4);
    jll.push_back(5);
    jll.push_front(2);
    jll.push_front(1);

    JinLinkedList<int>::iterator it = jll.begin();
    if(true)
    {
        int idx=0;
        char buf[10];
        while(it!=jll.end())
        {
            int num = *it;
            buf[idx++]=0x30+(char)num;
            ++it;
        }
        buf[idx++] = '\0';
        JAssert2(strcmp(buf,"12345")==0,"JinLinkedList");

        jll.pop_back();
        jll.pop_back();
        jll.push_front(0);
        jll.pop_front();
        jll.pop_front();
        JAssert(jll.size()==2);

        jll.clear();
        JAssert(jll.size()==0);
    }
}

struct internal
{
    int other;
    JinString jstr;
};

void UseCaseBase::runJinString()
{
    JinString hash = JinString::fromReadableHex("BBB5539A3EF2040485A39125CEB1624541AA73BE",40);
    JinKey160 hashKey((const unsigned char*)hash.c_str());
    JAssert(memcmp(hash.c_str(),hashKey.rawBuf(),20)==0);


    JinLinkedList<internal> toLogList;
    {
        JinString logStr;
        logStr = logStr + "[" + __FUNCTION__ + ":" + __LINE__ + "]\t" + "hello world!" +
                "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
        internal inter;
        inter.other = 0;
        inter.jstr = logStr;
        toLogList.push_back(inter);
        inter.other = 1;
        toLogList.push_back(inter);
        inter.other = 2;
        toLogList.push_back(inter);
    }
    for(int i=0;i<3;i++)
    {
        internal& lqUnit = toLogList.front();
        JAssert(lqUnit.jstr.find("hello world!")!=STRNPOS);
        JAssert(lqUnit.other == i);
        toLogList.pop_front();
    }


    JinString strs,strm,strjzy;
    strs = "abcdefghijklmn";
    strm = "MMMMM";
    strjzy = "jzyajzybjzyajzyz";
    JinString subjzy("jzyajzyz");

    //JinString str("abcdefghijklmnMMMMMjzyajzybjzyajzyzMMMMMabcdefghijklmn")
    JinString str;
    str += strs;
    str = str + strm + strjzy + strm + strs;

    JAssert(str.find(subjzy) ==  strs.length() + strm.length() + 8);
    JAssert(str.find(strs) == 0);
    JAssert(str.find(strm) == strs.length());
    JAssert(str.find(strs,str.find(strm)) == strs.length() + strm.length()*2 + strjzy.length());
    JAssert(str.find("jzyajzybjzyc")==STRNPOS);
}

class ThreadForJinEvent:public JinThread
{
public:
    ThreadForJinEvent(int n,JinSignaledEvent* e):num(n),event(e){}
    virtual bool period()
    {
        JDLog("I'm %d ready",num);
        event->wait(WAITUNTILWORLDEND);
        JDLog("I'm %d out",num);
        return false;
    }
private:
    int num;
    JinSignaledEvent* event;

};

class ThreadForShareDetach:public JinThread
{
public:
    ThreadForShareDetach(int n,JinSignaledEvent* e,JinString *str)
        :num(n),event(e),mStr(str){}
    virtual bool period()
    {
        JinString strInside = *mStr;
        JDLog("ThreadForShareDetach %d ready -%"PRISIZE,num,strInside.length());
        event->wait(WAITUNTILWORLDEND);
        JDLog("ThreadForShareDetach %d out",num);
        return false;
    }
private:
    int num;
    JinSignaledEvent* event;
    JinString *mStr;

};

void UseCaseBase::runJinEvent()
{
    const static int jecount = 10;
    const static int sdcount = 5;
    JinSignaledEvent event, devent;
    ThreadForJinEvent* ts[jecount];
    ThreadForShareDetach* td[sdcount];
    bool ret = event.init();
    JAssert(ret);
    ret = devent.init(false);  //auto reset test.
    JAssert(ret);
    for(int i=0;i<jecount;i++)
    {
        ts[i] = new ThreadForJinEvent(i,&event);
        ret = ts[i]->start();
        JAssert(ret);
    }

    for(int i=0;i<jecount;i++)
    {
        event.set();
        JinThread::msleep(abs(i-5)*jecount);
    }

    for(int i=0;i<jecount;i++)
    {
        delete ts[i];
    }

    //==================
    JinString *strInside = new JinString("concurrent detach test!");
    for(int i=0;i<sdcount;i++)
    {
        td[i] = new ThreadForShareDetach(i,&devent,strInside);
        ret = td[i]->start();
        JAssert(ret);
    }
    JinShareData* sd = (JinShareData*)*((JinStringData**)strInside);
    while(sd->ref()<=sdcount)
    {
        JinThread::msleep(100);
    }
    delete strInside;
    devent.set();
    for(int i=0;i<sdcount;i++)
    {
        delete td[i];
    }
}
const char* testPathList[] =
{
    "../../../../../../../..\\..///",
    "../../..\\..///",
    ".",
    "./",
    ".\\",
    ".x",
    "./xx/yy/c",
    "xx/xx",
    "xx.xx..xx",
    "/xx",
    "/xx/",
    "//xx/",
    "//xx//",
    "中国/文件夹/xx"
    "xx/./yy/../zz",
    "xx/../yy/a",
    "/xx/../../yy/b",
    "yy#&@xx"
    "yy$u'aa'/3/4",
    "yy$u'aa``~/3./.4",
    "c:",
    "c:\\",
    "..",
    "..xx",
    "/aa/..",
    "/aa/../bb",
    "/aa/..a/bb",
    "/aa/.a/bb",
    "~",
    "~xx",
    "~/xx",
    "~/../xx",
    "xx~/yy",
};
void UseCaseBase::runJinDir()
{
    size_t count = sizeof(testPathList)/sizeof(const char*);
#ifdef JWIN
    JinFileDir log("C:\\.config\\log.txt",true);
#else
    JinFileDir log("/.config/log.txt",true);
#endif
    log.cd("data");
    log.cd("local");
    log.cd("tmp");
    JDLog("dir = %s ",log.fullPath("log.txt"));
    log.cdUp();log.cdUp();log.cdUp();
#ifdef JWIN
    JAssert(memcmp(log.fullPath(),"C:\\.config\\",11)==0);
#else
    JAssert(memcmp(log.fullPath(),"/.config/",9)==0);
#endif

    JinFileDir home,temp,curr,exe;
    curr.setWith(kDirCurr);
    JDLog("curr = %s ",curr.fullPath());
    home.setWith(kDirHome);
    JDLog("home = %s ",home.fullPath());
    temp.setWith(kDirTemp);
    JDLog("temp = %s ",temp.fullPath());
    exe.setWith(kDirExe);
    JDLog(" exe = %s ",exe.fullPath());

    char fullpath[512];
    for(size_t i=0;i<count;i++)
    {
        JinFileDir dir(testPathList[i]);
        const char* strbydir = dir.fullPath();
#ifdef JWIN
        char* strbyreal = _fullpath(fullpath,testPathList[i],512);
#else
        char* strbyreal = realpath(testPathList[i],fullpath);
#endif
        JDLog("<%02u> org : %s\nby-dir : %s\nby-full: %s\n",
              i,testPathList[i],
              strbydir?strbydir:"<*er*>",
              strbyreal?strbyreal:"<fail>");
//#ifndef JWIN
//        if(strbyreal) free(strbyreal);
//#endif
    }
}

void UseCaseBase::runJinCompress()
{
    JinCompress jc(kTypeQuickLz);
    char buf[4096];
    for(int i=0;i<30;i++)
    {
        JinRoBuf rbuf = jc.compress(testPathList[i],strlen(testPathList[i]));
        memcpy(buf,rbuf.buf,rbuf.len);
        if(i>10)
        {
            rbuf = jc.decompress(buf,rbuf.len);
            JAssert(rbuf.len == strlen(testPathList[i]));
            JAssert(memcmp(rbuf.buf,testPathList[i],strlen(testPathList[i]))==0);
        }
    }
}


