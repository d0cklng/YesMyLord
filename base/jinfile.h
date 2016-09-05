#ifndef JINFILE_H
#define JINFILE_H
#ifdef WIN32
#include <windows.h>
#endif
#include "jinformatint.h"

/* ******************************
 *
 * 文件操作封装
 * 依赖: jinassert
 * 特性：基础操作,包装基本平台差异.
 *
 * ******************************/
#ifndef HDNULL
# ifdef JWIN
#  define HDNULL NULL
# else
#  define HDNULL 0
# endif
#endif

#ifdef JWIN
typedef HANDLE HdJFile;
#else
typedef int HdJFile;
#endif
#define FILENSIZE ((uint64_t)(-1));
#include "jinbuffer.h"


enum JFAccessFlag
{
    kNone=0,    //不要访问权限,不知道什么时候用得上.
    kReadOnly=1,
    kWriteOnly=2,
    kReadWrite = kReadOnly | kWriteOnly, //=3
};
enum JFOpenFlag
{
    kOpenExisting=0,  //总是打开存在,否则失败.
    kOpenAlways=1,    //总是打开,不存在则创建.
    kCreateAlways=2,   //总是打开,不存在则创建,且总按新建文件处理.
};

class JinFile
{
public:
    JinFile();
    JinFile(HdJFile hd);  //op mode操作者模式,操作hd,并不为hd负责.
    JinFile(const char* fullpath);
    virtual ~JinFile();

    int open(JFAccessFlag acc, JFOpenFlag ope);  //同步方式打开文件.
    bool isOpen();
    void setFullPath(const char* fullfilepath);
    const char* fullPath();

    uint64_t size();
    bool seek(uint64_t pos);
    uint64_t tell();
    //异步方式打开的文件不要用这里的读写函数,会立即返回无结果.
    bool read(char *buf, uint32_t byteToRead, uint32_t &byteReaded);
    bool write(const char *buf, uint32_t byteToWrite, uint32_t &byteWrited);
    bool flush();
    void close();

    static int open(const char* fullPath, JFAccessFlag acc, JFOpenFlag ope, HdJFile &hfile, bool forAsync=false);
    static int close(HdJFile file);
    static int remove(const char* fullPath);
    static uint64_t size(const char* fullPath);  //不显式打开文件.
    static bool exist(const char* fullPath);  //不显式打开文件.
    static bool seek(HdJFile file, uint64_t pos);

    static bool read(HdJFile file,char *buf, uint32_t byteToRead, uint32_t &byteReaded);
    static bool write(HdJFile file,const char *buf, uint32_t byteToWrite, uint32_t &byteWrited);
    static bool flush(HdJFile file);

    //static bool resize()
    //static bool truncate(const char* fullPath, uint64_t newSize);
private:
    bool isOpen_;
    JinBuffer filePath_;
    HdJFile hfile_;
};

#endif // JINFILE_H
