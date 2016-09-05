#include "TestHelper.h"
#include <stdlib.h>
#include "jinlogger.h"
#ifdef JWIN
#include <psapi.h>
//#pragma comment(lib,"psapi.lib")
#else
#include <unistd.h>
#endif

TestHelper * TestHelper::pInstace_ =  NULL;
TestHelper *TestHelper::get()
{
    if (NULL == pInstace_)
    {
        pInstace_ = new TestHelper;
    }
    return pInstace_;
}

void TestHelper::run(char** argv, int count)
{
    //从初始map中取出
    std::map<TestEasyBase*,std::string>::iterator it = firstMap_.begin();
    while(it!=firstMap_.end())
    {
        std::string &name = it->second;
        TestEasyBase* ins = it->first;
        useCaseList_.set(name.c_str(),ins);
        ++it;
    }

    for(int i=0;i<count;i++)
    {
        JPLog("==> Use case (%d/%d): %s",i+1,count,argv[i]);

        TestEasyBase* ins = useCaseList_.get(argv[i],NULL);
        if(!ins){
            JDLog("test-%s not exist!!",argv[i]);
        }
        else if(!ins->run()){
            JDLog("**test-%s failed!! errmsg=%s",argv[i],ins->errmsg());
        }

    }

    JDLog("============================================",NULL);
    //for result show..
    int success=0,failed=0;
    for(int i=0;i<count;i++)
    {
        TestEasyBase* ins = useCaseList_.get(argv[i],NULL);
        if(!ins){
            JDLog("[%d]test-%s [not exist] !!",i,argv[i]);
            ++ failed;
        }
        else
        {
            const char* emsg = ins->errmsg();
            if(emsg && emsg[0]!=0){
                JPLog("[%d]test-%s %s [failed!]",i,argv[i],emsg);
                ++ failed;
            }
            else
            {
                JDLog("[%d]test-%s success!",i,argv[i],emsg);
                ++ success;
            }
        }
    }

    JDLog("AutoTest finished..[%d]success,[%d]failed. Happy Women's Day",success,failed);
}

void TestHelper::addUseCase(const char *name, TestEasyBase *ins)
{
    firstMap_[ins] = name;
}

unsigned int TestHelper::getMemory()
{
#ifdef JWIN
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(ProcHandle, &pmc, sizeof(pmc));
    return (unsigned int)pmc.WorkingSetSize;
#else
    return 0;
#endif
}

TestHelper::TestHelper()
{
#ifdef JWIN
    ProcHandle = GetCurrentProcess();
#else
#endif
}

TestHelper::~TestHelper()
{
    //useCaseList的key是const char* 来自 firstMap，所以必须先释放.
    useCaseList_.clear();
//    std::map<TestEasyBase*,std::string>::iterator it = firstMap_.begin();
//    while(it!=firstMap_.end())
//    {
//        TestEasyBase* ins = it->first;
//        delete ins;
//        ++it;
//    }
    firstMap_.clear();
    if(pInstace_)pInstace_ = 0;
}
