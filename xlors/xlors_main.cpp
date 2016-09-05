#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <stdlib.h>
#include "xlors.h"
#include "xlors_share.h"
#include "jinlogger.h"
#include "jinplatformgap.h"
#include "jinassistthread.h"
#include "jinnetaddress.h"
#include "jinmemorycheck.h"
#include "jinfiledir.h"

//这里面的概念是 o= out port   i=inner port
// a最里, z最远  m中间. 分别是三处的地址.
// 这个程序TODO  正常退出..
void print_usage()
{
    printf("Help:\n");
    printf("-o, set outer listen port, default=%hu.\n",kLorsDefaultPort);
    printf("-z, set outer addr bind, default=any.\n");
    printf("example:\n");
    printf("xlors -o 7703 \n");
    printf("xlors -o 7703 -z 121.40.214.181\n");
}

int main(int argc, char *argv[])
{
    printf("OK, XLors Let's go!\n");
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
    JinNetPort outport(kLorsDefaultPort);
    JinNetAddr outside("0.0.0.0");
    char opstring[32];
    strcpy(opstring,"o:z:");
    while ((ch = getopt(argc,argv,opstring))!=-1)
    {
        switch(ch)
        {
        case 'o':
            outport = JinNetPort((uint16_t)atoi(optarg));
            printf("outport = %hu",outport.port());
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

    //if(outport.port() == 0)
    //{
        //print_usage();
        //return -2;
    //}
    JinFileDir tmpDir;
    tmpDir.setWith(kDirHome);
    JLOG_START(tmpDir.fullPath("XLors.txt"));
    JinMainThread::Get()->init();
    //bool bRet = initAsyncEngine();
    //if(bRet)
    //{
    XLors lors(outside,outport);
    if(lors.start())
    {
        while(JinMainThread::msleep(10)){}
        lors.stop();
    }
    else
    {
        ret = -4;
    }
    //uninitAsyncEngine();
    //}
    //else
    //{
    //    ret = -3;
    //}
    JinMainThread::Get()->uninit();
    JLOG_STOP();
    //_CrtDumpMemoryLeaks(); 此处不需要，全局和静态的还没释放呢.
    return ret;
}
