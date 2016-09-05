#include "jinfile.h"
#include "jinassert.h"
#ifdef WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>
#endif
#include <stdio.h>

JinFile::JinFile()
    : isOpen_(false)
    , hfile_(HDNULL)
{
}

JinFile::JinFile(HdJFile hd)
    : isOpen_(true)  //给的handle应该是已经打开的合法句柄.
    , hfile_(hd)
{

}

JinFile::JinFile(const char *fullpath)
    : isOpen_(false)
    , filePath_(fullpath)
    , hfile_(HDNULL)
{

}

JinFile::~JinFile()
{
    if(isOpen_)
    {
        this->close();
    }
}

int JinFile::open(JFAccessFlag acc, JFOpenFlag ope)
{
    JAssert(!isOpen_);
    //TODO  可以完善为检查路径的合法性.
    if(filePath_.length()<2) return false;
    int ret = JinFile::open(filePath_.cstr(),acc,ope,hfile_);
    isOpen_ = (ret == 0);
    return ret;
}

bool JinFile::isOpen()
{
    return isOpen_;
}

void JinFile::setFullPath(const char *fullfilepath)
{
    filePath_ = fullfilepath;
}

const char *JinFile::fullPath()
{
    return filePath_.cstr();
}

uint64_t JinFile::size()
{
    if(isOpen_)
    {  //已打开文件的方式求大小
#ifdef WIN32
        LARGE_INTEGER lint;
        lint.LowPart = GetFileSize(hfile_,(LPDWORD)&lint.HighPart);
        if(lint.LowPart!= INVALID_FILE_SIZE ) return lint.QuadPart;
#else
        struct stat64 info;
        if(0==fstat64(hfile_,&info)
                && S_ISREG(info.st_mode))
        {
            return info.st_size;
        }
#endif
    }
    else if(filePath_.length()>0)
    {  //不打开文件的方法
        return JinFile::size(filePath_.cstr());
    }

    return FILENSIZE;
}

bool JinFile::seek(uint64_t pos)
{
    return JinFile::seek(hfile_,pos);
}

bool JinFile::seek(HdJFile file, uint64_t pos)
{
#ifdef WIN32
    LARGE_INTEGER lint;
    lint.QuadPart = pos;
    return ( INVALID_SET_FILE_POINTER != SetFilePointer(file,lint.LowPart,&lint.HighPart,FILE_BEGIN) );
#else
    return ( -1 != lseek64(file, pos, SEEK_SET));
#endif
}

uint64_t JinFile::tell()
{
    uint64_t ret = (uint64_t)(-1);
#ifdef WIN32
    LARGE_INTEGER lint;
    lint.QuadPart = 0;
    lint.LowPart =  SetFilePointer(hfile_,lint.LowPart,&lint.HighPart,FILE_CURRENT);
    if(lint.LowPart!=INVALID_SET_FILE_POINTER)
        ret = lint.QuadPart;
#else
    ret = lseek64(hfile_, 0, SEEK_CUR);
#endif
    return ret;
}

bool JinFile::read(char *buf, uint32_t byteToRead, uint32_t &byteReaded)
{
    JAssert(isOpen_);
    return JinFile::read(hfile_,buf,byteToRead,byteReaded);
}

bool JinFile::write(const char *buf, uint32_t byteToWrite, uint32_t &byteWrited)
{
    JAssert(isOpen_);
    return JinFile::write(hfile_,buf,byteToWrite,byteWrited);
}

bool JinFile::flush()
{
    JAssert(isOpen_);
    return JinFile::flush(hfile_);
}

void JinFile::close()
{
    if(isOpen_)
    {
        JinFile::close(hfile_);
        isOpen_ = false;
    }
}

int JinFile::open(const char* fullPath, JFAccessFlag acc, JFOpenFlag ope, HdJFile &hfile, bool forAsync)
{
    //if(filePath_.length()<2) return false;
    JAssert(fullPath!=HDNULL);
    if(fullPath==HDNULL)return -1;

#ifdef WIN32
    DWORD desiredAccess = 0;
    DWORD creationDisposition = 0;
    if(acc&kReadOnly)desiredAccess |= GENERIC_READ;
    if(acc&kWriteOnly)desiredAccess |= GENERIC_WRITE;
    switch(ope)
    {
    case kOpenAlways: creationDisposition = OPEN_ALWAYS;  break;
    case kOpenExisting: creationDisposition = OPEN_EXISTING; break;
    case kCreateAlways: creationDisposition = CREATE_ALWAYS; break;
    }
    hfile = CreateFileA(fullPath,desiredAccess,FILE_SHARE_READ,NULL,creationDisposition,forAsync?FILE_FLAG_OVERLAPPED:0,NULL);
    if(INVALID_HANDLE_VALUE == hfile){
        return (int)GetLastError();
    }
#else
    int32_t os_flags = O_RDWR;
    if(acc == kReadOnly) os_flags = O_RDONLY;
    if(acc == kWriteOnly) os_flags = O_WRONLY;

    switch(ope)
    {
    case kOpenAlways: os_flags |= O_CREAT;  break;
    case kOpenExisting: os_flags |= O_EXCL; break;
    case kCreateAlways: os_flags |= O_CREAT|O_TRUNC; break;
    }

    if(forAsync) os_flags |= O_NONBLOCK;

    hfile = ::open(fullPath, os_flags, (0644));

    if (hfile == -1){
        return errno;
    }
    //fchmod(ret_val, 0777);//errno
#endif
    return 0;
}

int JinFile::close(HdJFile file)
{
#ifdef WIN32
    if(FALSE == CloseHandle(file))
    {    return (int)GetLastError();   }
    else
    {    return 0;   }
#else
    return ::close(file);
#endif
}

int JinFile::remove(const char *fullPath)
{
//#ifdef WIN32
//    return (TRUE == ::DeleteFileA(fullPath));
//#else
    int ret = ::remove(fullPath);
    if(ret == 0)return 0;
    else return errno;
    //return (0 == ::remove(fullPath));
//#endif
}

uint64_t JinFile::size(const char *fullPath)
{
#ifdef WIN32
    WIN32_FIND_DATAA fdata;
    HANDLE fhandle;
    fhandle = FindFirstFileA(fullPath,&fdata);
    if(fhandle != INVALID_HANDLE_VALUE &&
            (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0)
    {
        LARGE_INTEGER lint;
        lint.LowPart = fdata.nFileSizeLow;
        lint.HighPart = fdata.nFileSizeHigh;
        FindClose(fhandle);
        return lint.QuadPart;
    }
#else
    struct stat64 info;
    if(0==stat64(fullPath,&info)
            && S_ISREG(info.st_mode))
    {
        return info.st_size;
    }
#endif
    return FILENSIZE;
}

bool JinFile::exist(const char *fullPath)
{
#ifdef JMSVC
    WIN32_FIND_DATAA fdata;
    HANDLE fhandle;
    fhandle = FindFirstFileA(fullPath,&fdata);
    return (fhandle != INVALID_HANDLE_VALUE && !(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
#else
    return (access(fullPath, 0) == 0);
#endif
}

bool JinFile::read(HdJFile file, char *buf, uint32_t byteToRead, uint32_t &byteReaded)
{
#ifdef WIN32
    return (TRUE == ReadFile(file,(LPVOID)buf,(DWORD)byteToRead,(LPDWORD)&byteReaded,NULL));
#else
    int ret =::read(file,buf,byteToRead);
    if(ret < 0)return false;
    byteReaded = (uint32_t)ret;
    return true;
#endif
}

bool JinFile::write(HdJFile file,const char *buf, uint32_t byteToWrite, uint32_t &byteWrited)
{
#ifdef WIN32
    return (TRUE == WriteFile(file,(LPCVOID)buf,(DWORD)byteToWrite,(LPDWORD)&byteWrited,NULL));
#else
    int ret =::write(file,buf,byteToWrite);
    if(ret < 0)return false;
    byteWrited = (uint32_t)ret;
    return true;
#endif
}

bool JinFile::flush(HdJFile file)
{
#ifdef JWIN
    return FlushFileBuffers(file);
#else
    return fsync(file);
    return false;
#endif
}

//bool JinFile::truncate(const char *fullPath, uint64_t newSize)
//{
//    SetEndOfFile()
//}
