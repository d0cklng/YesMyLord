#ifndef JINSHAREMEMORY_H
#define JINSHAREMEMORY_H

#include "jinformatint.h"
#ifdef JWIN
#include <windows.h>
#endif

/* ******************************
 *
 * 共享内存实现
 *
 * ******************************/
enum JSMOpenType
{
    kMemoryCreate,  //限定以创建式,如果已存在会失败.
    kMemoryOpen,    //限定以打开式
    kMemoryAny,     //以创建式,若存在改打开式
    __CountOfMemoryOpenType
};
enum JSMAccessFlag
{
    kAccessRead,
    kAccessReadWrite,
    __CountOfAccessFlagType
};

class JinShareMemory
{
public:
    JinShareMemory(const char* name);
    ~JinShareMemory();
    bool open(uint32_t size, JSMOpenType type, JSMAccessFlag flag);
    void leave();  //离开,貌似不能主导关闭?

    uint32_t size();
    //operator const char*();
    unsigned char* buff(){ return (unsigned char*)buff_; }
    const unsigned char* cbuff() const { return (const unsigned char*)buff_; }
    const char* cstr() const { return (const char*)buff_; }
#ifndef JWIN
protected:
    uint32_t keyt();
#endif
private:
    const static int nameLenMax = 64;
    char name_[nameLenMax];
    void *buff_;
    uint32_t realSize_;

#ifdef JWIN
    HANDLE handle_;
#else
    int handle_;
#endif
};

#endif // JINSHAREMEMORY_H
