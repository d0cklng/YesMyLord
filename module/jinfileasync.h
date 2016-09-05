#ifndef JINFILESYNC_H
#define JINFILESYNC_H

#include "jinformatint.h"
#include <stddef.h>
#include "jinasyncdef.h"
#include "jinstring.h"
#include "jinfile.h"

#include "jinasyncdef.h"

//enum JinFileOpenType
//{
//    kOpenReadOnly = 1,  //读取，如果文件不存在，失败。
//    kOpenWriteOnly = 2,  //写入，如果文件不存在，失败。
//    kOpenReadWrite = kOpenReadOnly|kOpenWriteOnly,  //读写，如果文件不存在，失败。
//    kOpenAlways = 4,
//    kOpenReadAlways = kOpenReadOnly|kOpenAlways,
//    kOpenWriteAlways = kOpenWriteOnly|kOpenAlways,
//    kOpenReadWriteAlways = kOpenReadWrite|kOpenAlways,
//};

class JinFileAsync : public IAsynObj
{
public:
    JinFileAsync();
    JinFileAsync(const char* fullpath);
    virtual ~JinFileAsync();

    //bool open(JinFileOpenType type);
    bool isOpen();
    void setFullPath(const char* fullpath);
    const char* fullPath(){return filePath_.c_str();}

    //uint64_t tell();
    uint64_t size();
    int open(JFAccessFlag acc, JFOpenFlag ope, bool async = false);  //可以异步打开.默认是同步的.
    bool read(JinAsynBuffer& buffer, const uint32_t bufPos, uint64_t readFrom, uint32_t byteToRead);
    bool write(JinAsynBuffer& buffer, const uint32_t bufPos, uint64_t writeTo, uint32_t byteToWrite);
    void close(bool async = false);  //可以异步关闭,默认是同步关闭.
protected:
    virtual void OnOpen(bool isSucc, int errcode)
    {UNUSED(isSucc);UNUSED(errcode);}
    virtual void OnRead(uint64_t fromPos, void *buf, uint64_t toRead, uint64_t readed)
    {UNUSED(fromPos); UNUSED(buf); UNUSED(toRead); UNUSED(readed);}
    virtual void OnWrite(uint64_t toPos, void *buf, uint64_t toWrite, uint64_t wrote)
    {UNUSED(toPos); UNUSED(buf); UNUSED(toWrite); UNUSED(wrote);}
    virtual void OnClose(){}
                                                     //主线程或者调用线程拉
    virtual void OnMainPull(bool isSuccess, HdObject handle, AsynOpType type, void *buf, uint64_t opSize,
                            uint64_t tranSize, void* user, uint64_t user64);
private:
    bool isOpen_;
    JinString filePath_;
    //AsynOp aop_;
};

#endif // JINFILESYNC_H
