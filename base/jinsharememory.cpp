#include "jinsharememory.h"
#ifdef JWIN
#include <Windows.h>
#define INVALIDHD INVALID_HANDLE_VALUE
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#define INVALIDHD (-1)
#endif

#include <string.h>
#include "jinassert.h"
#include "jinlogger.h"


JinShareMemory::JinShareMemory(const char *name)
    : buff_(NULL)
    , realSize_(0)
    , handle_(INVALIDHD)
{
#ifdef JWIN
    strncpy(name_,name,nameLenMax);
#else
    strncpy(name_,"/tmp/JinSm-",11);
    strncpy(name_+11,name,nameLenMax-11);
#endif
    name_[nameLenMax-1] = 0;
}

JinShareMemory::~JinShareMemory()
{
    leave();
}

#ifdef JWIN
bool JinShareMemory::open(uint32_t size, JSMOpenType type, JSMAccessFlag flag)
{
    JAssert(name_[0]!=0);
    //JDLog("ShareMemory open, name=%s, type=%d, flag=%d",name_,type,flag);
    bool flagOpenAnyway = (flag==kMemoryAny);
    switch(type)
    {
    case kMemoryAny:
        flagOpenAnyway = true;
        // no break;
    case kMemoryCreate:
    {
        DWORD accflag = 0;
        if(flag = kAccessReadWrite) accflag = PAGE_READWRITE;
        else accflag = PAGE_READONLY;

        handle_ = CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,(DWORD)accflag,0,size,name_);
        if(handle_ == INVALIDHD){
            return false;
        }
        //如果ALREADY_EXISTS,实际size应该是第一次创建的那个,这里没办法知道?!
        realSize_ = size;
        if(GetLastError() == ERROR_ALREADY_EXISTS && !flagOpenAnyway){
            CloseHandle(handle_);
            return false;
        }
    }
        break;
    case kMemoryOpen:
    {
        DWORD accflag = 0;
        if(flag = kAccessReadWrite) accflag = FILE_MAP_WRITE; //write即readwrite
        else accflag = FILE_MAP_READ;
        handle_ = OpenFileMappingA(accflag,FALSE,name_);
        if(handle_ == NULL){
            handle_ = INVALIDHD;
            return false;
        }
    }
        break;
    default:
        JAssert2(false,"JSMOpenType unknown");
        return false;
    }

    DWORD accflag = 0;
    if(flag == kAccessReadWrite) accflag = FILE_MAP_WRITE;
    else accflag = FILE_MAP_READ;
    buff_ = MapViewOfFile(handle_,accflag,0,0,size);  //always full map
    if(buff_ == NULL)
    {
        CloseHandle(handle_);
        handle_ = INVALIDHD;
        return false;
    }
    return true;
}
#else
bool JinShareMemory::open(uint32_t size, JSMOpenType type, JSMAccessFlag flag)
{
    int openflag = 0;   //kMemoryOpen
    //IPC_CREAT   如果共享内存不存在，则创建一个共享内存，否则打开操作。
    //IPC_EXCL    只有在共享内存不存在的时候，新的共享内存才建立，否则就产生错误。
    if(type==kMemoryAny)  openflag |= IPC_CREAT;   //IPC_CREAT;
    if(type==kMemoryCreate) openflag |= IPC_EXCL;   //IPC_CREAT|IPC_EXCL;
    //key_t nk = ftok(name_,'J'); 不使用ftok.
    handle_ = shmget((key_t)keyt(),size,openflag);
    if(handle_ == INVALIDHD)
    {
        return false;
    }

    int accflag = 0;
    if(flag == kAccessReadWrite) accflag = 0;
    if(flag == kAccessRead) accflag = SHM_RDONLY;
    buff_ = shmat(handle_,NULL,accflag);
    if(buff_ == NULL)
    {
        shmctl(handle_,IPC_RMID,0); //要求移除,内核会等ref归0.
        handle_ = INVALIDHD;
        return false;
    }
    return true;
}
#endif

void JinShareMemory::leave()
{
    if(buff_)
    {
#ifdef JWIN
        UnmapViewOfFile(buff_);
#else
        shmdt(buff_);
#endif
        buff_ = NULL;
    }
    if(handle_!=INVALIDHD)
    {
#ifdef JWIN
        CloseHandle(handle_);
#else
        shmctl(handle_,IPC_RMID,0); //要求移除,内核会等ref归0.
#endif
        handle_ = INVALIDHD;
    }
}

uint32_t JinShareMemory::size()
{
    if(buff_)
    {
        return realSize_;
    }
    return 0;
}

#ifndef JWIN
uint32_t JinShareMemory::keyt()
{
    char* str = name_;
    const unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}
#endif

