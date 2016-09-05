#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC_NEW
#include <stdlib.h>
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#include "TestHelper.h"
#include "jinthread.h"
#include "jinlogger.h"
#include "jindatatime.h"
#include "jinfiledir.h"
#include <stdlib.h>

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
    JinFileDir fdir;
    JinDirRtn rtn = fdir.setWith(kDirHome);  //使用运行目录放
    if(rtn != kRtnSuccess)
    {
        JLOG_START(NULL);
    }
    else
    {
        JLOG_START(fdir.fullPath("autotest.log"));
    }

    TestHelper *helper = TestHelper::get();
    helper->run(&argv[1],argc-1);
    JinThread::msleep(2000);  //for log write.
    delete helper;

    JLOG_STOP();
    //_CrtDumpMemoryLeaks(); 此处不需要，全局和静态的还没释放呢.
    argc++;
}
