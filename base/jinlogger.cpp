#include "jinlogger.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "jinsingleton.h"
#include "jinthread.h"
#include "jinfile.h"
#include "jinsignaledevent.h"
#include "jinlinkedlist.h"
#include "jinstring.h"
#include "jindatatime.h"
#include "jinmutex.h"
#include "jinsingleton.h"
#include "jinfiledir.h"

#define MaxLogConcurrent 10
#define MaxLogBufferLength 8192

#ifdef JWIN
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

static char soloLogPath[350] = {0};
#ifdef NO_JIN_LOGGER

#else
class JinLogger : public JinThread
{
public:
    JinLogger() //: timeStart_(0)
    {
        //timeStart_ = JinDateTime::tickCount();
        //if(timeStart_==0)timeStart_=1;  //linux下可能为0
        //JAssert(timeStart_!=0);
        event_.init();
        logRunFlag_ = 1;  //指示运行,不一定就运行了.
        this->setThreadName("JinLogger");
    }
    ~JinLogger()
    {
        this->stop();
    }

    void stop()
    {
        if(logRunFlag_>1)
        {  //有时候没start 就stop,所以要判断.
            JinThread::stop();
            //JAssert(logQueue_.size() == 0);  //如果触发,可以考虑去掉.
            lastPrintSecond = 0;
            lastPrintMinus = 0;
        }
    }

    bool setPath(const char* logFullFilePath)
    {
        file_.setFullPath(logFullFilePath);
        int ret = file_.open(kReadWrite,kCreateAlways);
        if(ret!=0){
            printf("*log file open failed![%s],errorcode=%d\n",logFullFilePath,ret);
        }
        return (ret==0);
    }
    void log(JinString str, long timeAtSe, long timeAtUs)
    {
        log(str.c_str(), timeAtSe, timeAtUs);
    }
    //lqUnit.timeAtSe = JinDateTime::getTime(&lqUnit.timeAtUs);
    void log(const char* txt, long timeAtSe, long timeAtUs)
    {
        logQueueStru lqUnit;
        lqUnit.timeAtSe = timeAtSe;//JinDateTime::getTime(&lqUnit.timeAtUs);
        lqUnit.timeAtUs = timeAtUs;
        lqUnit.logTxt = txt;
        mutex_.lock();
        if(!file_.isOpen()){
            printf(">%ld %s\n",lqUnit.timeAtSe,txt);
            mutex_.unlock();
            return;
        }
        logQueue_.push_back(lqUnit);
        mutex_.unlock();
        event_.set();
    }

protected:
    //如果返回false，线程提前终止，仍会执行uninit()
    bool init()
    {
        if(!file_.isOpen()){
            return false;
        }
        //lastPrintHour = 0;
        lastPrintMinus = 0;
        lastPrintSecond = 0;
        return true;
    }

    void uninit()
    {
        if(file_.isOpen()){
            file_.close();
        }
    }

    void afterStart(uint64_t threadId,const char* threadName)
    {
        JAssert(logRunFlag_==1);
        JDLog("threadid:%llu,threadName=[%s]", threadId, threadName);
        logRunFlag_ = 2;
    }

    void beforeStop(uint64_t threadId,const char* threadName)
    {
        JDLog("threadid:%llu,threadName=[%s]", threadId, threadName);
        if(logRunFlag_>0)logRunFlag_ = -logRunFlag_;
        event_.set(); //+ 如果stop之前logQueue已经size归0，则会卡住，这里额外set一次.
    }

    //当需要线程停止时，返回false
    bool period()
    {
        char timeBuf[32]; // = {0x88};
#ifdef JDEBUG
        memset(timeBuf,0x88,32); printf("timeBuf=%p\n",timeBuf);
#endif
        while(event_.wait(WAITUNTILWORLDEND))
        {
            mutex_.lock();
            if(logQueue_.size() == 0){
                //JAssert(!logRunFlag_);
                if(logRunFlag_<=0){
                    mutex_.unlock();
                    break;
                }
            }
            JinLinkedList<logQueueStru> toLogList;
            while(logQueue_.size())
            {
                logQueueStru lqUnit = logQueue_.front();
                logQueue_.pop_front();
                toLogList.push_back(lqUnit);
            }
            mutex_.unlock();


            while(toLogList.size())
            {   //这个引用要小心,如果toLogList.pop_front()那么lqUnit可能立即失效!
                const logQueueStru& lqUnit = toLogList.front();
                //struct timeval
                uint32_t byteToWrite = 0;
                uint32_t byteWrited = 0;
                struct tm *pltime = NULL;
#ifdef JWIN
                time_t tt = lqUnit.timeAtSe;
                pltime = localtime(&tt);
#else
                struct tm local_time;
                pltime = &local_time;
                localtime_r(&lqUnit.timeAtSe, &local_time);
#endif
                if(pltime->tm_sec != lastPrintSecond || pltime->tm_min != lastPrintMinus)
                {
                    uint32_t byteToWrite =
                            sprintf(timeBuf,"\t*[%04d-%02d-%02d|%02d:%02d:%02d]*\r\n",
                                    pltime->tm_year+1900, pltime->tm_mon+1, pltime->tm_mday,
                                    pltime->tm_hour, pltime->tm_min, pltime->tm_sec);
                    //lastPrintHour = pltime->tm_hour;
                    lastPrintMinus = pltime->tm_min;
                    lastPrintSecond = pltime->tm_sec;
                    file_.write(timeBuf,byteToWrite,byteWrited);
                }
                byteToWrite = sprintf(timeBuf,"%04d:%03ld.%03ld]\t",pltime->tm_min*60 + pltime->tm_sec,lqUnit.timeAtUs/1000,lqUnit.timeAtUs%1000);
                file_.write(timeBuf,byteToWrite,byteWrited);
                file_.write(lqUnit.logTxt.c_str(),(uint32_t)lqUnit.logTxt.length(),byteWrited );
                file_.write("\r\n",2,byteWrited);
                toLogList.pop_front();
                //file_.flush();  //细碎flush 效率低.
            }

            if(logRunFlag_<=0)event_.set();
        }
        return false;
    }

private:
    struct logQueueStru
    {
        long timeAtSe;
        long timeAtUs;
        JinString logTxt;
    };
    //int lastPrintHour;
    int lastPrintMinus;
    int lastPrintSecond;
    //T64 timeStart_;
    JinFile file_;
    JinMutex mutex_;  //锁logQueue_
    JinSignaledEvent event_; //激活IO线程.
    JinLinkedList<logQueueStru> logQueue_;
    int logRunFlag_;  //起来就是1,如果实际起线程就是2,如果要关就变负数.
};

static const char* jGetLoggerPath(JinFileDir& jdir, long timeAtSe)
{
    jdir.setWith(kDirTemp);
    char logFileName[64];
    struct tm *pltime = NULL;
#ifdef JWIN
    time_t tt = timeAtSe;
    pltime = localtime(&tt);
#else
    struct tm local_time;
    pltime = &local_time;
    localtime_r(&timeAtSe, &local_time);
#endif
    sprintf(logFileName,"jlog%02d%02d%02d-%02d%02d%02d.txt",
            pltime->tm_year%100, pltime->tm_mon+1, pltime->tm_mday,
            pltime->tm_hour, pltime->tm_min, pltime->tm_sec);
    return jdir.fullPath(logFileName);
}

typedef JinSingleton<JinLogger> SGJinLogger;
JinMutex loggerMutex;
volatile int gLoggerInitState = 0;  //0 not init; 1 init; 2 on uninit;  -1 失败,直接printf
bool jinLogerInit(const char* data, const char* time, const char* logFullFilePath)
{
    if(gLoggerInitState>0)return false;
    JinFileDir logDir;
    long timeAtSe,timeAtUs;
    timeAtSe = JinDateTime::getTime(&timeAtUs);
    if(logFullFilePath==0)
    {
        logFullFilePath = jGetLoggerPath(logDir,timeAtSe);
    }
    printf("\nLogger init, path=%s\n",logFullFilePath);
    strcpy(soloLogPath,logFullFilePath);
    //loggerMutex.init();

    gLoggerInitState = -1;
    JinLogger* log = SGJinLogger::Create();
    if(!log) return false;
    if(!log->setPath(logFullFilePath)){SGJinLogger::Free(); return false;}
    if(!log->start()){SGJinLogger::Free(); return false;}
    JinString logAtFirst("JinLogger start. Build time:");
    logAtFirst += data;
    logAtFirst += " ";
    logAtFirst += time;
    log->log(logAtFirst,timeAtSe,timeAtUs);

    gLoggerInitState = 1;
    return true;
}

void jinLogger(int line, const char* fpath, const char* func,
               const char* format, ...)
{
    long timeAtSe,timeAtUs;
    timeAtSe = JinDateTime::getTime(&timeAtUs);

    va_list ap;
    va_start(ap, format);
    char logBuf[MaxLogBufferLength];
    int used = 0;
#if 1
    used += vsnprintf(logBuf+used, MaxLogBufferLength-used, format, ap);
    used += snprintf(logBuf+used, MaxLogBufferLength-used, "\t\t[%s:%d][%s]",func,line,fpath);
#else
    used += snprintf(logBuf+used,MaxLogBufferLength-used,"[%s][%s:%d]\t",fpath,func,line);
    used += vsnprintf(logBuf+used, MaxLogBufferLength-used, format, ap);
#endif
    va_end(ap);

    logBuf[MaxLogBufferLength-1]=0;
    if(gLoggerInitState < 0)
    {
        printf(">%ld %s\n",timeAtSe,logBuf);
    }
    else
    {
        loggerMutex.lock();
        if(gLoggerInitState != 1){
            loggerMutex.unlock();
            jinSoloLog(line,NULL,NULL,logBuf);
            return;
        }
        JinLogger* log = SGJinLogger::Get();
        log->log(logBuf,timeAtSe,timeAtUs);
        loggerMutex.unlock();
    }
}

void jinErrorOut(int line, const char* fpath, const char* func,
                 const char* format, ...)
{
    long timeAtSe,timeAtUs;
    timeAtSe = JinDateTime::getTime(&timeAtUs);

    va_list ap;
    va_start(ap, format);
    char logBuf[MaxLogBufferLength];
    int used = sprintf(logBuf,"[ERR][%s][%s:%d]\t",fpath,func,line);
    vsnprintf(logBuf+used, MaxLogBufferLength-used, format, ap);
    va_end(ap);
    logBuf[MaxLogBufferLength-1]=0;

    loggerMutex.lock();
    if(gLoggerInitState != 1)
    {
        loggerMutex.unlock();
        jinSoloLog(line,NULL,NULL,logBuf);
        return;
    }
    JinLogger* log = SGJinLogger::Get();
    log->log(logBuf,timeAtSe,timeAtUs);
    loggerMutex.unlock();
}


void jinLogerStop()
{
    long timeAtSe,timeAtUs;
    timeAtSe = JinDateTime::getTime(&timeAtUs);

    loggerMutex.lock();
    if(gLoggerInitState != 1)
    {
        loggerMutex.unlock();
        return;
    }
    gLoggerInitState = 2;
    JinLogger* log = SGJinLogger::Get();
    loggerMutex.unlock();

    log->log("JinLogger now stop.",timeAtSe,timeAtUs);
    SGJinLogger::Free();
    gLoggerInitState = 0;
}
#endif

void jinSoloLog(int line, const char *fpath, const char *func, const char *format, ...)
{
    long timeAtSe=0,timeAtUs=0;
    if(soloLogPath[0] == 0)
    {
        JinFileDir dir;
        long timeAtSe,timeAtUs;
        timeAtSe = JinDateTime::getTime(&timeAtUs);
        const char* logPath = jGetLoggerPath(dir,timeAtSe);
        strcpy(soloLogPath,logPath);
    }
    if(timeAtSe==0)
    {
        timeAtSe = JinDateTime::getTime(&timeAtUs);
    }

    va_list ap;
    va_start(ap, format);
    char logBuf[MaxLogBufferLength];
    int used = 0;
#if 1
    used += vsnprintf(logBuf+used, MaxLogBufferLength-used, format, ap);
    if(fpath!=NULL && func!=NULL)
    {
        used += snprintf(logBuf+used, MaxLogBufferLength-used, "\t\t[%s:%d][%s]",func,line,fpath);
    }
#else
    if(fpath!=NULL && func!=NULL)
    {
        used += snprintf(logBuf+used,MaxLogBufferLength-used,"[%s][%s:%d]\t",fpath,func,line);
    }
    used += vsnprintf(logBuf+used, MaxLogBufferLength-used, format, ap);
#endif
    va_end(ap);
    logBuf[MaxLogBufferLength-1]=0;

    FILE* fp = fopen(soloLogPath,"a+");
    if(!fp)return;

    struct tm *pltime = NULL;
#ifdef JWIN
    time_t tt = timeAtSe;
    pltime = localtime(&tt);
#else
    struct tm local_time;
    pltime = &local_time;
    localtime_r(&timeAtSe, &local_time);
#endif
    fprintf(fp,"%02d-%02d-%02d %02d:%02d:%02d_%03ld.%03ld \t",
            pltime->tm_year%100, pltime->tm_mon+1, pltime->tm_mday,
            pltime->tm_hour, pltime->tm_min, pltime->tm_sec,
            timeAtUs/1000,timeAtUs%1000);

    fprintf(fp,"%s\n",logBuf);
    fclose(fp);
}
