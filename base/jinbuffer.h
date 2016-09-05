#ifndef JINBUFFER_H_201409211023
#define JINBUFFER_H_201409211023

#include "jinformatint.h"
#include <stddef.h>

/* ******************************
 *
 * 缓冲区简单管理，可以方便以参数方式传递而不用担心内存拷贝
 * 依赖: jinassert jinsharedata jinmemorycheck
 * 特性：1.可以以类形式自由拷贝
 *      2.如果malloc发生问题，data_将保持0，使用前检查data
 *      3.如果malloc发生问题，sharedata的pref为0，拷贝这种对象总是失败data_为0
 *
 * ******************************/

class JinBufferData;
class JinBuffer
{
public:
    JinBuffer();
    ~JinBuffer();

    ///alloc a buff with length len.
    JinBuffer(int32_t len);

    JinBuffer(const char* s);
    JinBuffer(const void *ptr,uint32_t len);

    JinBuffer(const JinBuffer &o);
    JinBuffer& operator=(const JinBuffer& o);
    JinBuffer& operator=(const char* s);

    /// take owner of ptr.
    /// this is not a good but sometimes efficient.
    /// DO NOT use this if you are not sure about it.
    void eat(void *ptr,size_t len);
    void eat(char *str);
    size_t length() const;  //internal buff length!
    //size_t capability() const;
    bool resize(size_t len, bool keepOldData=false); //if fail, nothing happen.

    operator const char*();
    unsigned char* buff();
    const unsigned char* cbuff() const;
    const char* cstr() const;

    /// detach buf and free if necessary
    void clear();
    bool isNull();


private:
    JinBufferData *data_;
};

typedef struct JinRoBuf {  //read only
    //JinRoBuf(){}
    JinRoBuf(int initNum=0):len(0),buf(NULL){len=initNum;}
    size_t len;
    const char  *buf;
} JRoBuf,*PJRoBuf;

//有的架构对不对齐的指针做(*)地址取值会崩溃.
extern uint16_t SafeAlignGetPtrValue16(const void*);
extern uint32_t SafeAlignGetPtrValue32(const void*);
extern uint64_t SafeAlignGetPtrValue64(const void*);

#endif // JINBUFFER_H
