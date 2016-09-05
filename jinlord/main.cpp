#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <stdlib.h>
#include "jinlogger.h"
#include "jinlord.h"
#include "jinasyncengine.h"
#include "jinplatformgap.h"
#include "jinassistthread.h"
#include "jinnetaddress.h"
#include "jinmemorycheck.h"
#include "jinfiledir.h"
void print_usage()
{
    printf("\nHelp:\n");
    printf("-s, set self identity to register. default=\"jinlord-test\"\n");  //设置自己的身份串,默认是"jinlord-test", 建议要设置,但每次保持一样.
    printf("-p, set outer port bind. default=8080\n");  //用于raknet连接\dht的端口.固定端口除了可能失败外都是好处!
    printf("-o, set outer addr bind. default=any.\n");  //用于raknet连接的地址, 默认any, 建议默认.
    printf("-i, set inner addr for real target connect. default=any\n"); //用于业务连接到目标用的ip, 默认是any,有特别需求才可能需要指定.
    printf("example:\n");
    printf("jinlord -o 127.0.0.1\n");
    printf("jinlord -o 127.0.0.1 -p 8080\n");
    printf("jinlord -o 127.0.0.1 -p 8080 -s \"jinlord-home\"\n");

}

int main(int argc, char *argv[])
{
    printf("\nOK, XLord Let's go!");
#if defined(JMSVC) && defined(JDEBUG)
    //_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    //_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    ///_CrtSetBreakAlloc(7915);
#endif

    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);

    int ch;
    int ret = 0;
    JinNetAddr addr4rakDHT("0.0.0.0");
    JinNetPort port = 8080;
    JinNetAddr addr2realTarget;
    JinString selfIdentity("jinlord-test");
    char opstring[32];
    strcpy(opstring,"o:p:i:s:");
    while ((ch = getopt(argc,argv,opstring))!=-1)
    {
        switch(ch)
        {
        case 'o':
            addr4rakDHT = JinNetAddr(optarg);
            printf("\nbind addr = %s",addr4rakDHT.toAddr(gDLogBufAddr));
            break;
        case 'p':
            port = JinNetPort((uint16_t)atoi(optarg));
            printf("\nbind port = %hu",port.port());
            break;
        case 'i':
            addr2realTarget = JinNetAddr(optarg);
            printf("\nuse4real target addr = %s",addr2realTarget.toAddr(gDLogBufAddr));
            break;
        case 's':
            selfIdentity = optarg;
            printf("\nself identity = %s", selfIdentity.c_str());
            break;
        case '?':
            print_usage();
            break;
        default:
            print_usage();
            return -1;
        }
    }

    JinFileDir tmpDir;
    tmpDir.setWith(kDirHome);
    JLOG_START(tmpDir.fullPath("JinLord.log"));
    JinMainThread::Get()->init();
    bool bRet = initAsyncEngine();
    if(bRet)
    {
        JinLord lord(addr4rakDHT,port,addr2realTarget,selfIdentity);
        if(lord.start())
        {
            do
            {
                while(driveAEngine()){}
            }
            while(JinMainThread::msleep(3));
            lord.stop();
        }
        else
        {
            ret = -4;
        }
        uninitAsyncEngine();
    }
    else
    {
        ret = -3;
    }
    JinMainThread::Get()->uninit();
    JLOG_STOP();
    //_CrtDumpMemoryLeaks(); 此处不需要，全局和静态的还没释放呢.
    return ret;
}

