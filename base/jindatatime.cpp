#include "jindatatime.h"

// ####################################################################### //
#if defined(_WIN32)
#include <windows.h>
// To call timeGetTime
// on Code::Blocks, this needs to be libwinmm.a instead
#ifdef MSVC
#pragma comment(lib, "Winmm.lib")
#endif
#endif


#if defined(_WIN32)
DWORD mProcMask;
DWORD mSysMask;
HANDLE mThread;

#else
//#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <malloc.h>
T64 sg_InitialTime;
#endif

static bool initialized=false;

#if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0
#include "jinmutex.h"
T64 lastNormalizedReturnedValue=0;
T64 lastNormalizedInputValue=0;
/// This constraints timer forward jumps to 1 second, and does not let it jump backwards
/// See http://support.microsoft.com/kb/274323 where the timer can sometimes jump forward by hours or days
/// This also has the effect where debugging a sending system won't treat the time spent halted past 1 second as elapsed network time
T64 NormalizeTime(T64 timeIn)
{
    T64 diff, lastNormalizedReturnedValueCopy;
    static JinMutex mutex;

    mutex.lock();
    if (timeIn>=lastNormalizedInputValue)
    {
        diff = timeIn-lastNormalizedInputValue;
        if (diff > GET_TIME_SPIKE_LIMIT)
            lastNormalizedReturnedValue+=GET_TIME_SPIKE_LIMIT;
        else
            lastNormalizedReturnedValue+=diff;
    }
    else
        lastNormalizedReturnedValue+=GET_TIME_SPIKE_LIMIT;

    lastNormalizedInputValue=timeIn;
    lastNormalizedReturnedValueCopy=lastNormalizedReturnedValue;
    mutex.unlock();

    return lastNormalizedReturnedValueCopy;
}
#endif // #if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0

//RakNet::Time RakNet::GetTime( void )
//{
//	return (RakNet::Time)(gettime64()/1000);
//}

#if   defined(_WIN32)
T64 gettimeus_Windows( void )
{
    if ( initialized == false)
    {
        initialized = true;

        // Save the current process
#if !defined(_WIN32_WCE)
        HANDLE mProc = GetCurrentProcess();
        // Get the current Affinity
#if _MSC_VER >= 1400 && defined (_M_X64)
        GetProcessAffinityMask(mProc, (PDWORD_PTR)&mProcMask, (PDWORD_PTR)&mSysMask);
#else
        GetProcessAffinityMask(mProc, &mProcMask, &mSysMask);
#endif
        mThread = GetCurrentThread(); //没有使用哦?
#endif // _WIN32_WCE
    }
    // 9/26/2010 In China running LuDaShi, QueryPerformanceFrequency has to be called every time because CPU clock speeds can be different
    T64 curTime;
    LARGE_INTEGER PerfVal;
    LARGE_INTEGER yo1;

    QueryPerformanceFrequency( &yo1 );
    QueryPerformanceCounter( &PerfVal );

    __int64 quotient, remainder;
    quotient=((PerfVal.QuadPart) / yo1.QuadPart);
    remainder=((PerfVal.QuadPart) % yo1.QuadPart);
    curTime = (T64) quotient*(T64)1000000 + (remainder*(T64)1000000 / yo1.QuadPart);

#if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0
    return NormalizeTime(curTime);
#else
    return curTime;
#endif // #if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0
}
#elif defined(__GNUC__)  || defined(__GCCXML__) ||  defined(__S3E__)
T64 gettimeus_Linux( void )
{
    timeval tp;
    if ( initialized == false)
    {
        gettimeofday( &tp, 0 );
        initialized=true;
        // I do this because otherwise RakNet::Time in milliseconds won't work as it will underflow when dividing by 1000 to do the conversion
        sg_InitialTime = ( tp.tv_sec ) * (T64) 1000000 + ( tp.tv_usec );
    }

    // GCC
    T64 curTime;
    gettimeofday( &tp, 0 );

    curTime = ( tp.tv_sec ) * (T64) 1000000 + ( tp.tv_usec );

#if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0
    return NormalizeTime(curTime - initialTime);
#else
    return curTime - sg_InitialTime;
#endif // #if defined(GET_TIME_SPIKE_LIMIT) && GET_TIME_SPIKE_LIMIT>0
}
#endif

T64 gettimeus( void )
{
#if   defined(_WIN32)
    return gettimeus_Windows();
#else
    return gettimeus_Linux();
#endif
}





// ####################################################################### //
#if defined(_WIN32) && !defined(__GNUC__)  &&!defined(__GCCXML__)
//#include "gettimeofday.h"
// From http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/
#include <windows.h>
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    /*converting file time to unix epoch*/
    tmpres /= 10;  /*convert into microseconds*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

#endif

static const long kSecOfMinute = 60;
static const long kSecOfHour = 60*kSecOfMinute;
static const long kSecOfDay = 24*kSecOfHour;

JinDateTime::JinDateTime()
    : mytm_(-1)
    , toStrCache_(NULL)
{

}

JinDateTime::JinDateTime(long epoch)
    : mytm_(epoch)
    , toStrCache_(NULL)
{

}

JinDateTime::~JinDateTime()
{
    if(toStrCache_)free(toStrCache_);
}

void JinDateTime::setNow()
{
    mytm_ = getTime();
}

void JinDateTime::timeMoveYear(int year)
{
    if(mytm_<0) setNow();
    struct tm *pltime = NULL;
#ifdef JWIN
    time_t tt = mytm_;
    pltime = gmtime(&tt);
#else
    struct tm gm_time;
    pltime = &gm_time;
    gmtime_r(&mytm_, &gm_time);
#endif

    pltime->tm_year += year;
    mytm_ = mktime(pltime);
}

void JinDateTime::timeMoveMonth(int month)
{
    if(mytm_<0) setNow();
    struct tm *pltime = NULL;
#ifdef JWIN
    time_t tt = mytm_;
    pltime = gmtime(&tt);
#else
    struct tm gm_time;
    pltime = &gm_time;
    gmtime_r(&mytm_, &gm_time);
#endif

    int y = month % 12;  //y总是大于等于0
    int year = (month-y) / 12;
    pltime->tm_year += year;
    pltime->tm_mon += y;
    if(pltime->tm_mon > 11)
    {
        pltime->tm_year += 1;
        pltime->tm_mon -= 12;
    }
    mytm_ = mktime(pltime);

}

void JinDateTime::timeMoveDay(int day)
{
    if(mytm_<0) setNow();
    mytm_ += day * kSecOfDay;
}

void JinDateTime::timeMoveHour(int hour)
{
    if(mytm_<0) setNow();
    mytm_ += hour * kSecOfHour;
}

void JinDateTime::timeMoveMinute(int minute)
{
    if(mytm_<0) setNow();
    mytm_ += minute * kSecOfMinute;
}

void JinDateTime::timeMoveSecond(int second)
{
    if(mytm_<0) setNow();
    mytm_ += second;
}

long JinDateTime::epoch()
{
    if(mytm_<0) setNow();
    return mytm_;
}

const char *JinDateTime::toString()
{
    if(toStrCache_==NULL)
    {
        toStrCache_ = (char*)malloc(32);
    }
    if(!toStrCache_) return "[OutOfMemory]";

    struct tm *pltime = NULL;
#ifdef JWIN
    time_t tt = mytm_;
    pltime = localtime(&tt);
#else
    struct tm gm_time;
    pltime = &gm_time;
    localtime_r(&mytm_, &gm_time);
#endif

    sprintf(toStrCache_,"%04d-%02d-%02d %02d:%02d:%02d",
            pltime->tm_year+1900, pltime->tm_mon+1, pltime->tm_mday,
            pltime->tm_hour, pltime->tm_min, pltime->tm_sec);

    return toStrCache_;
}

T64 JinDateTime::tickCount()
{
    return (T64)(gettimeus()/1000);
}

long JinDateTime::getTime(long *usec)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    if(usec)*usec = tv.tv_usec;
    return tv.tv_sec;
}


