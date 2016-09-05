#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <stdlib.h>
#include "xlorb.h"
#include "jinlogger.h"
#include "jinplatformgap.h"
#include "jinassistthread.h"
#include "jinnetaddress.h"
#include "jinmemorycheck.h"
#include "jinfiledir.h"

//xlorb, b看起来是一根插入，喻示联络到内部网络.
void print_usage()
{
    printf("\nHelp:\n");
    printf("-o, set outer bind addr, default=any.\n");
    printf("-i, set inner bind addr, default=any.\n");
    printf("example:\n");
    printf("xlorb -o 10.168.14.252 -i 10.10.192.188\n");
}

int main(int argc, char *argv[])
{
    printf("\nOK, XLorb Let's go!\n");
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
    JinNetAddr inside("0.0.0.0");
    JinNetAddr outside("0.0.0.0");
    JinString selfIdentity("xlorb-debug");
    char opstring[32];
    strcpy(opstring,"o:i:s:");
    while ((ch = getopt(argc,argv,opstring))!=-1)
    {
        switch(ch)
        {
        case 'o':
            outside = JinNetAddr(optarg);
            printf("\noutside = %s",outside.toAddr(gDLogBufAddr));
            break;
        case 'i':
            inside = JinNetAddr(optarg);
            printf("\ninside = %s",inside.toAddr(gDLogBufAddr));
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
    JLOG_START(tmpDir.fullPath("XLorb.txt"));
    JinMainThread::Get()->init();
    bool bRet = initAsyncEngine();
    if(bRet)
    {
        XLorb lorb(inside,outside,selfIdentity);
        if(lorb.start())
        {
            do
            {
                while(driveAEngine()){}
            }
            while(JinMainThread::msleep(3));
            lorb.stop();
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
