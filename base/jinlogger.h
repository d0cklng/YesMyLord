#ifndef JINLOGGER_H
#define JINLOGGER_H
#include "jinformatint.h"
#include <stddef.h>

/* ******************************
 *
 * 使用全局单例,单例隐藏在cpp中.使用单独线程同步写日志到文件
 * 依赖: jinsingleton jinthread jinfile
 * jinsignaledevent jinlinkedlist jinstring jindatatime
 *
 * 特征：
 *
 * ******************************/

#ifdef NO_JIN_LOGGER
# define JDLog(format,...)
# define JELog(format,...)
# define JLOG_START(logPath) (true)
# define JLOG_STOP() (NULL);
#else
# ifdef JDEBUG  //__FUNCTION__
#  define JDLog(format,...) jinLogger(__LINE__,__FILE__,__FUNCTION__,format,##__VA_ARGS__)
#  define JELog(format,...) jinErrorOut(__LINE__,__FILE__,__FUNCTION__,format,##__VA_ARGS__)
#  define JPLog(format,...) JDLog(format,##__VA_ARGS__);printf(format"\n",##__VA_ARGS__)
# else
#  define JDLog(format,...)
#  define JELog(format,...) jinErrorOut(__LINE__,__FILE__,__FUNCTION__,format,##__VA_ARGS__)
#  define JPLog(format,...)
# endif
# define JLOG_START(logPath) jinLogerInit(__DATE__,__TIME__,logPath)
# define JLOG_STOP() jinLogerStop()

//################# 使用以上的宏,以下的定义请无视 ##################

//#  define JELog(format,...) jinErrorOut(__LINE__,__FUNCTION__,format,__VA_ARGS__)









bool jinLogerInit(const char* data, const char* time, const char* logFullFilePath);

void jinLogger(int line, const char* fpath, const char* func,
               const char* format, ...);
void jinErrorOut(int line, const char* fpath, const char* func,
                 const char* format, ...);

//如果不显式调用这个函数，全局单例的释放顺序无法掌控，可能JinMemoryCheck先释放，导致JinLogger释放时崩溃
void jinLogerStop();

#endif //NO_JIN_LOGGER

#define JSLog(format,...) jinSoloLog(__LINE__,__FILE__,__FUNCTION__,format,##__VA_ARGS__);printf(format"\n",##__VA_ARGS__)
//独立打日志，如果用过日志，会用日志同一个文件，否则使用默认文件路径.
void jinSoloLog(int line, const char* fpath, const char* func,
                       const char* format, ...);
#endif // JINLOGGER_H
