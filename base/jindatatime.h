#ifndef JINDATATIME_H
#define JINDATATIME_H
#include "jinformatint.h"
#if defined(_WIN32) && !defined(__GNUC__)  &&!defined(__GCCXML__)
#include <time.h>
struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct timezone *tz);

#else
#include <sys/time.h>
#include <unistd.h>

/* *********************
 *
 * gettimeofday 在一些环境下的移植
 * 从RakNet抄过来的
 *
 * *********************/

// Uncomment this if you need to  --我想这是gettimeofday另一种实现吧
/*
// http://www.halcode.com/archives/2008/08/26/retrieving-system-time-gettimeofday/
struct timezone
{
  int tz_minuteswest;
  int tz_dsttime;
};

#ifdef	__cplusplus

void  GetSystemTimeAsFileTime(FILETIME*);

inline int gettimeofday(struct timeval* p, void* tz )
{
    union {
        long long ns100; // time since 1 Jan 1601 in 100ns units
        FILETIME ft;
    } now;

    GetSystemTimeAsFileTime( &(now.ft) );
    p->tv_usec=(long)((now.ns100 / 10LL) % 1000000LL );
    p->tv_sec= (long)((now.ns100-(116444736000000000LL))/10000000LL);
    return 0;
}

#else
    int gettimeofday(struct timeval* p, void* tz );
#endif
*/
#endif

/* **********************
 *
 * 从RakNet抄了一些代码.其中gettimeofday是Linux才有
 * tickCount是依赖一个叫GetTimeUs的函数,
 *  对于这个函数实现,目前觉得还比较难理解,因为准确度不高啊,更别提精度了.
 *
 * 依赖: jinmutex
 * *********************/
#undef GET_TIME_SPIKE_LIMIT
//#define GET_TIME_SPIKE_LIMIT 1*1000*1000  //控制两次获取时间的间隔不超过这个谬秒单位.
//例如上一次是x us，过了好久再获取，最多也只是x+GET_TIME_SPIKE_LIMIT
typedef uint64_t T64;
typedef uint32_t T32;


class JinDateTime
{
public:
    JinDateTime(); //初始化就让他成为now
    JinDateTime(long epoch);
    ~JinDateTime();

    void setNow();  //把内部时间设为当前,可做更新用.
    void timeMoveYear(int year);  //+正表示未来,-负表示过去
    void timeMoveMonth(int month);
    void timeMoveDay(int day);
    void timeMoveHour(int hour);
    void timeMoveMinute(int minute);
    void timeMoveSecond(int second);
    long epoch();
    const char* toString();
public:
    static T64 tickCount();  //与开机有关的tick，通常是本机自己使用,单位是毫秒.
    static long getTime(long *usec=NULL); //返回Epoch至今秒数，如果关注微妙精度，传入usec

private:
    long mytm_;
    char* toStrCache_;
};



#endif // JINDATATIME_H


