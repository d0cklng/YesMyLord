#ifndef JINASSERT_H
#define JINASSERT_H
#include <assert.h>

/* *****************************
 *
 * 先申请资源，一旦触发断言则用保持的资源输出必要信息。
 * 依赖: -无-
 *
 * *****************************/

#ifdef JDEBUG

//#define JAssert(expr,param,msg) jAssert(__FILE__,__LINE__,expr,param,#expr)
//#define JAssert(expr,param,msg) assert(false)
#define JAssert(expr) \
    if((expr)==false){jAssertOut(__FILE__,__FUNCTION__,__LINE__,#expr,0,0);assert(expr);}
#define JAssert2(expr,msg) \
    if((expr)==false){jAssertOut(__FILE__,__FUNCTION__,__LINE__,#expr,msg,0);assert(expr);}
#define JAssert3(expr,msg,param) \
    if((expr)==false){jAssertOut(__FILE__,__FUNCTION__,__LINE__,#expr,msg,(void*)param);assert(expr);}
#define JAssert4(expr,msg,param) \
    if((expr)==false){jAssertOut(__FILE__,__FUNCTION__,__LINE__,#expr,msg,(void*)&param);assert(expr);}


void jAssertOut(const char *file,
             const char* function,
             unsigned int line,
             const char* expr,
             const char *msg = 0,
             void *param = 0);

extern void logAssertStack();
#else  //JDEBUG


#define JAssert(expr) assert(expr)
#define JAssert2(expr,msg) assert(expr)
#define JAssert3(expr,msg,param) assert(expr)
#define JAssert4(expr,msg,param) assert(expr)


#endif  //JDEBUG

#endif // JINASSERT_H
