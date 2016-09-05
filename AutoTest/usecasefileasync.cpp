#include "usecasefileasync.h"
#include <stdint.h>
#include <stdlib.h>
#include "jinfileasync.h"
#include "jinrandom.h"
#include "jinlogger.h"
#include "jinthread.h"

#ifdef WIN32
#define SPLASH '\\'
#else
#define SPLASH '/'
#endif

CHAIN_USE_CASE(FileAsync);


class TestNonExistFile : public JinFileAsync
{
    void OnOpen(bool isSucc, int errcode)
    {
        JDLog("TestNonExistFile OnOpen succ=%d, errcode=%d",(int)isSucc,errcode);
        JAssert(isSucc == false);
    }
};

class TestFile : public JinFileAsync
{
public:
    TestFile(UseCaseFileAsync* sink)
        : mSink(sink)
        , writeOpDone(0)
        , readOpDone(0)
    {
        bufRead.alloc(2600+100);
        filebuf.alloc(2600);
        JAssert(bufRead.ref()>0);
        JAssert(filebuf.ref()>0);
    }
    ~TestFile()
    {

    }

    void build()
    {
        JAssert(this->isOpen());
        //unsigned char filebuf[2560];
        for(int i=0;i<2600;i++)
        {
            *(char*)filebuf.buff(i) = 'a' + i/100;
        }

        //uint64_t pos = 0;
        //a-z每个字母100个
        //int i=0;
        for(int i=100;i>=0;i--)  //测试并发101个异步写
        {
            //uint64_t blen = 256*(i%10+1);
            this->write(filebuf,0,2600*i,2600);  //写101遍 262,600
            //pos += blen; //a100b100c100...a100b100c100...100遍
        }

    }

    void randRead()
    {
        //seedMT((uint32_t)(int64_t)(void*)this);
        for(int i=0;i<26;i++)  //测试26个并发随机读
        {
            this->read(bufRead,i*100,2600*i+100*i,100);
        }
    }

    static int closeOpDone;
    static int openOpDone;
protected:

    virtual void OnRead(uint64_t fromPos, void *buf, uint64_t toRead, uint64_t readed)
    {
        JDLog("OnRead fromPos%llu,toRead%llu,readed%llu",fromPos,toRead,readed);
        if(readOpDone<26)
        {
            JAssert(readed == toRead);
            JAssert(memcmp(buf,filebuf.buff(fromPos%2600),100)==0);
        }
        //JAssert(memcmp(filebuf,buf,256)==0);
        ++readOpDone;
        if(readOpDone==26)  //正常读写完毕.
        {  //再投递一个越界读.
            //pos不能完全越界,会导致GetQueuedCompletionStatus返回FALSE
            this->read(bufRead,2600,262600-3,88);
        }
        if(readOpDone==27)
        {
            //此时通知退出，下一轮操作会被全部取消,在此之前,再投递26个随机读
            mSink->onQuit();
            /*
            for(int i=0;i<26;i++)  //再投递，预期会被全部取消掉,OnRead不再触发.至于实际执行了多少,关心不到了.
            {
                this->read(bufRead+i*100,2600*i+100*i,100);
            }*/
        }

    }

    virtual void OnWrite(uint64_t toPos, void *buf, uint64_t toWrite, uint64_t wrote)
    {
        UNUSED(buf);
        JDLog("onWrite toPos%llu,toWrite%llu,wrote%llu",toPos,toWrite,wrote);
        JAssert(wrote == toWrite);
        ++writeOpDone;
        if(writeOpDone == 100)
        {
            randRead();
        }
    }
    virtual void OnOpen(bool isSucc, int errcode)
    {
        ++openOpDone;
        JDLog("OnOpen[%s] %s, errcode=%d",isSucc?"succ":"fail",fullPath(),errcode);
        JAssert(isSucc);
        if(openOpDone>5) this->close(true);
        //if(isSucc)this->close(true);
    }

    virtual  void OnClose()
    {
        ++closeOpDone;
        JDLog("OnClose %s",fullPath());
        //if(closeOpDone>10)mSink->onQuit();
        if(closeOpDone==11)mSink->onQuit();
    }

private:
    UseCaseFileAsync *mSink;
    int writeOpDone;
    int readOpDone;
    //char bufRead[2600+100];
    JinAsynBuffer bufRead;
    JinAsynBuffer filebuf;
    //char filebuf[2600];
};

int TestFile::closeOpDone = 0;
int TestFile::openOpDone = 0;



UseCaseFileAsync::UseCaseFileAsync()
    : isRun_(true)
{
    errmsg_[0] = 0;
}

//UseCaseFileAsync::~UseCaseFileAsync()
//{

//}

void UseCaseFileAsync::liteTest()
{
    bool ret = true;
    const char *pathvar;
    pathvar = getenv("TEMP");
    if(pathvar==0)pathvar = "/tmp";
    JinString path(pathvar);
    if(!path.endWith('/') && !path.endWith("\\"))
    {
        path = path + SPLASH;
    }
    path += "testfile.hex";

    if(ret)
    {
        JinFile::remove(path.c_str());

        TestFile* file = JNew(TestFile,this);
        file->setFullPath(path.c_str());  //测试OpenExisting和CreateAlways
        int iret=0;
        iret = file->open(kReadWrite,kOpenExisting); JAssert(iret != 0);
        iret = file->open(kReadWrite,kCreateAlways); JAssert(iret == 0);
        file->build();

        while(driveAEngine(INFINITE) && isRun_);

        //while(isRun_)
        //{
        //    driveAEngine(false);
        //    JinThread::msleep(30);
        //}
        if(file)JDelete(file);  //还有异步io的时候直接删文件考验
        uninitAsyncEngine();  //马上删引擎考验

        JDLog("\nEngine will restart..",NULL);
    }
}


bool UseCaseFileAsync::run()
{
    bool ret;
    ret = initAsyncEngine();
    JAssert(ret==true);


    //bool ret = true;
    const char *pathvar;
    pathvar = getenv("TEMP");
    if(pathvar==0)pathvar = "/tmp";
    JinString path(pathvar);
    if(!path.endWith('/') && !path.endWith("\\"))
    {
        path = path + SPLASH;
    }
    path += "testfile.hex";

    TestNonExistFile* nExistFile = JNew(TestNonExistFile);
    nExistFile->setFullPath(path + ".noexist");
    int iret;
    iret = nExistFile->open(kReadWrite,kOpenExisting,true);
    JAssert(iret==0);

    if(ret)
    {
        JinFile::remove(path.c_str());

        TestFile* file = JNew(TestFile,this);
        file->setFullPath(path.c_str());  //测试OpenExisting和CreateAlways
        iret = file->open(kReadWrite,kOpenExisting);
        CHECK_ASSERT(iret != 0,"file should exist and open failed.");
        iret = file->open(kReadWrite,kCreateAlways);
        CHECK_ASSERT(iret == 0,"file should open with overwrite");
        file->build();

        while(driveAEngine(INFINITE) && isRun_);

        if(file)JDelete(file);  //还有异步io的时候直接删文件考验
        JDelete(nExistFile);
        uninitAsyncEngine();  //马上删引擎考验

        JDLog("\nEngine will restart..",NULL);
    }

    if(ret)
    {
        isRun_ = true;
        TestFile* tf[10];
        for(int i=0;i<10;i++)
        {
            tf[i] = JNew(TestFile,this);
        }
        initAsyncEngine();  //马上又开引擎考验
        path = pathvar;
        if(!path.endWith('/') && !path.endWith("\\"))
        {
            path = path + SPLASH;
        }
        path += "testfile.x";
        for(int i=0;i<10;i++)
        {
            path.unsafe_str()[path.length()-1] = '0'+i;
            tf[i]->setFullPath(path.c_str());
            iret = tf[i]->open(kReadWrite,kOpenAlways,true);
            CHECK_ASSERT(iret == 0,"file should open always.");
        }

        while(isRun_ && TestFile::openOpDone!=10)
        {
            while(driveAEngine()){}
            JinThread::msleep(30);
        }

        for(int i=0;i<10;i++)
        {
            tf[i]->close(true);
        }
        JDLog("\nuninit engine.",NULL);
        uninitAsyncEngine();

        int rmret = 0;
        for(int i=0;i<10;i++)
        {
            if(rmret == 0)
            {
                rmret = JinFile::remove(tf[i]->fullPath());
            }
            JDelete(tf[i]);
        }
        CHECK_ASSERT(rmret == 0,"remove failed!");
    }


    return true;
}

const char *UseCaseFileAsync::errmsg()
{
    if(errmsg_[0])return errmsg_;
    else return NULL;
}

void UseCaseFileAsync::onQuit()
{
    isRun_ = false;
    awakeAEngine();
}

