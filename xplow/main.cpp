#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <stdlib.h>
#include "jinasyncengine.h"
#include "jinthread.h"
#include "jinlogger.h"
#include "jindatatime.h"
#include "jinplatformgap.h"
#include "jinnetaddress.h"
#include "xplow.h"

//这里面的概念是 o= out port   i=inner port
// a最里, z最远  m中间. 分别是三处的地址.
// 这个程序TODO  正常退出..
void print_usage()
{
    printf("Help:\n");
    printf("-o, set outer listen port.\n");
    printf("-i, set inner server port.\n");
    printf("-a, set inner server addr, default=localhost.\n");
    printf("-m, set inner addr bind, default=any.\n");
    printf("-z, set outer addr bind, default=any.\n");
    printf("example:\n");
    printf("xplow -o 10080 -i 80\n");
    printf("xplow -o 13389 -i 3389 -a 10.168.14.252 -m 10.168.137.49 -z 121.40.214.181\n");
}

int main(int argc, char *argv[])
{
    printf("OK, Let's go!\n");
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
    JinNetPort outport;
    JinNetPort srvport;
    JinNetAddr outside("0.0.0.0"); //不区分指定地址可能端口重合.
    JinNetAddr inside("0.0.0.0");
    JinNetAddr srvaddr("127.0.0.1");
    char opstring[32];
    strcpy(opstring,"o:i:a:m:z:");
    while ((ch = getopt(argc,argv,opstring))!=-1)
    {
        switch(ch)
        {
        case 'o':
            outport = JinNetPort((uint16_t)atoi(optarg));
            printf("outport = %hu",outport.port());
            break;
        case 'i':
            srvport = JinNetPort((uint16_t)atoi(optarg));
            printf("srvport = %hu",srvport.port());
            break;
        case 'a':
            srvaddr = JinNetAddr(optarg);
            printf("srvaddr = %s",srvaddr.toAddr(gDLogBufAddr));
            break;
        case 'm':
            inside = JinNetAddr(optarg);
            printf("inside = %s",inside.toAddr(gDLogBufAddr));
            break;
        case 'z':
            outside = JinNetAddr(optarg);
            printf("outside = %s",outside.toAddr(gDLogBufAddr));
            break;
        case '?':
            break;
        default:
            print_usage();
            return -1;
        }
    }

    if(outport.port() == 0 || srvport.port() == 0)
    {
        print_usage();
        return -2;
    }

    JLOG_START(NULL);
    bool bRet = initAsyncEngine();
    if(bRet)
    {
        XPlow plow(outside,outport,inside,srvport,srvaddr);
        if(plow.start())
        {
            while(driveAEngine(INFINITE)) {}
            plow.stop();
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
    JLOG_STOP();
    //_CrtDumpMemoryLeaks(); 此处不需要，全局和静态的还没释放呢.
    return ret;
}
