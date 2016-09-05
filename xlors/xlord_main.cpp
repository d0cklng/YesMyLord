#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include <stdlib.h>
#include "xlord.h"
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
    printf("-o, set outer addr bind. default=any.\n");
    printf("-p, set outer port bind. default=8080\n");
    printf("-s, set self identity to register.\n");
    printf("-f, set identity to find.\n");
    printf("example:\n");
    printf("xlord -o 127.0.0.1\n");
    printf("xlord -o 127.0.0.1 -p 8080\n");
    printf("xlord -o 127.0.0.1 -p 8080 -f \"jin-test\"\n");

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
    JinNetAddr addr("0.0.0.0");
    JinNetPort port = 8080;
    JinString identity;
    JinString selfIdentity("xlord-debug");
    char opstring[32];
    strcpy(opstring,"o:p:f:s:");
    while ((ch = getopt(argc,argv,opstring))!=-1)
    {
        switch(ch)
        {
        case 'o':
            addr = JinNetAddr(optarg);
            printf("\nbind addr = %s",addr.toAddr(gDLogBufAddr));
            break;
        case 'p':
            port = JinNetPort((uint16_t)atoi(optarg));
            printf("\nbind port = %hu",port.port());
            break;
        case 'f':
            identity = optarg;
            printf("\nidentity = %s", identity.c_str());
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

    if(identity.length() == 0)
    {
        print_usage();
        return -2;
    }

    JinFileDir tmpDir;
    tmpDir.setWith(kDirHome);
    JLOG_START(tmpDir.fullPath("XLord.txt"));
    JinMainThread::Get()->init();
    bool bRet = initAsyncEngine();
    if(bRet)
    {
        XLord lord(addr,port,selfIdentity,identity);
        if(lord.start())
        {
            //lord.addForwarding(10080,"10.10.16.252",80);
            //lord.addForwarding(8888,"121.40.214.181",3389);
            //lord.addForwarding(7775,"localhost",7774);
            //lord.addForwarding(8989,"121.40.214.181",3389);
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
