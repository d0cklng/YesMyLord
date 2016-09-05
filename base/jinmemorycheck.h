#ifndef JINMEMORYCHECK_H
#define JINMEMORYCHECK_H
#include "jinformatint.h"
#include <stddef.h>
/* *******************************
 *
 * 内存泄漏检查，用于检查内存的申请和释放
 * 依赖: jinmutex jinassert std::map
 * 链接： http://jinzeyu.cn/index.php/archives/194/
 *
 * *******************************/
#if defined(JMSVC) && defined(JDEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new  new(_CLIENT_BLOCK, __FILE__, __LINE__)
#endif

#if defined(__FreeBSD__)
#include <stdlib.h>

#elif defined ( __APPLE__ ) || defined ( __APPLE_CC__ )
#include <malloc/malloc.h>
#include <alloca.h>
#elif defined(_WIN32)
#include <malloc.h>
#else
#include <malloc.h>
// Alloca needed on Ubuntu apparently
#include <alloca.h>
#endif


//#ifndef MEMORY_NO_DEBUG
#ifdef JDEBUG

//@f.10.6 以下定义重新改成不干涉原始函数的方式。仅仅追踪量.
//TODO 考虑区分malloc new 和array方式，防止申请于释放不匹配.
#define JMalloc(size) jinAlloc(malloc(size),size,__FILE__,__LINE__)
#define JMalloc2(size,file,line) jinAlloc(malloc(size),size,file,line)
#define JFree(pointer) jinFree(pointer,__FILE__,__LINE__);free(pointer)
#define JFree2(pointer,file,line) jinFree(pointer,file,line);free(pointer)
#define JNew(type,...) (type*)jinAlloc(new type(__VA_ARGS__),sizeof(type),__FILE__,__LINE__)
#define JNewAry(type,count) (type*)jinAlloc(new type[count],sizeof(type)*count,__FILE__,__LINE__)
#define JDelete(pointer) jinFree(pointer,__FILE__,__LINE__);delete (pointer)
#define JDelAry(pointer) jinFree(pointer,__FILE__,__LINE__);delete[] (pointer)

void* jinAlloc(void *alloced, size_t size, const char *file, unsigned int line);
void jinFree(void *tobefree, const char *file, unsigned int line);
//dump并预测泄露数和量，如果预测正确返回true
bool __dumpWithExpect(size_t leakCount=0,size_t leakBytes=0);
#define JMemDumpE(ec,eb)  __dumpWithExpect(ec,eb)
#define JMemDump() __dumpWithExpect(0,0)
#else  // MEMORY_NO_DEBUG

#define JMalloc(size) malloc(size)
#define JMalloc2(size,file,line) malloc(size)
#define JFree(pointer) free(pointer)
#define JFree2(pointer,file,line) free(pointer)
#define JNew(type,...) new type(__VA_ARGS__)
#define JNewAry(type,count,...) new type[count]
//#define JNewAry(type,count,...) new type(__VA_ARGS__)[count]
#define JDelete(pointer) delete (pointer)
#define JDelAry(pointer) delete[] (pointer)
#define JMemDumpE(ec,eb)
#define JMemDump()
#endif // MEMORY_NO_DEBUG


#endif // JINMEMORYCHECK_H

