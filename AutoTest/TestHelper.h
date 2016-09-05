#ifndef TESTHELPER_H
#define TESTHELPER_H

#include "jinsingleton.h"
#include "jinhashmap.h"
#include <map>
#include <string>
#ifdef JWIN
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/types.h>  //pid_t
#endif

class TestEasyBase
{
public:
    //注意，析构时main已结束，与memcheck顺序不可语气，所以不要在继承类中放测试的非指针成员对象.
    virtual ~TestEasyBase(){}
    virtual bool run() = 0;
    virtual const char* errmsg()
    {
        if(errmsg_[0])return errmsg_;
        else return NULL;
    } //返回空串表示成功没信息.
protected:
    char errmsg_[1024];
};

class TestHelper
{
public:
    TestHelper();
    ~TestHelper();
    static TestHelper* get();
    void run(char** argv, int count);
    void addUseCase(const char *name, TestEasyBase* ins);
public:
    unsigned int getMemory();
protected:
    static bool IsStrEqual(const char * const &a , const char * const &b)
    {
        return (strcmp(a,b)==0);
    }
private:
    JinHashMap<const char *, TestEasyBase*, JinHashDefault<const char*>, IsStrEqual > useCaseList_;
    std::map<TestEasyBase*,std::string> firstMap_;
    static TestHelper * pInstace_;

#ifdef JWIN
    HANDLE ProcHandle;
#else
    pid_t ProcPid;
#endif
};

class TestUseCaseChain
{
public:
    TestUseCaseChain(const char* name, TestEasyBase* ins)
    {
        ins_ = ins;
        TestHelper *helper = TestHelper::get();
        helper->addUseCase(name,ins);
    }

    ~TestUseCaseChain()
    {
        delete ins_; //这里释放的话，释放动作在main()之后.
    }
private:
    TestEasyBase* ins_;
};

#define CHAIN_USE_CASE(name) TestUseCaseChain UC##name(#name,new UseCase##name())
#define CHECK_ASSERT(cond,errmsg) if(!cond){ strcpy(errmsg_,errmsg); return false; }

#endif // TESTHELPER_H
