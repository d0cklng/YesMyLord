#include "jinfileasync.h"
#include "jinasyncengine.h"
#include "jinlogger.h"
#include "jinfile.h"
#ifdef _WIN32
#include <windows.h>
#endif

JinFileAsync::JinFileAsync()
    : isOpen_(false)
    //, aop_(NULL)
{
}

JinFileAsync::JinFileAsync(const char *fullpath)
    : isOpen_(false)
{
    setFullPath(fullpath);
}

JinFileAsync::~JinFileAsync()
{
    if(isOpen_){
        this->close();
        isOpen_ = false;
    }
    //JAssert(aop_ == NULL);

    if(aop_){
        AE::Get()->detach(aop_);
        aop_ = NULL;
    }
}

bool JinFileAsync::isOpen()
{
    return isOpen_;
}

void JinFileAsync::setFullPath(const char *fullpath)
{
    filePath_ = fullpath;
}

//uint64_t JinFileAsync::tell()
//{
//    if(!isOpen_)return false;
//    return 0;  //TODO
//}

uint64_t JinFileAsync::size()
{
    if(isOpen_)
    {  //打开文件的方式求大小
#ifdef WIN32
        LARGE_INTEGER lint;
        lint.LowPart = GetFileSize(hfile_,(LPDWORD)&lint.HighPart);
        if(lint.LowPart!= INVALID_FILE_SIZE ) return lint.QuadPart;
#else
        struct stat64 stat;
        if(0==fstat64(hfile_,&stat)) return stat.st_size;
#endif
    }
    else if(filePath_.length()>0)
    {  //不打开文件的方法
        return JinFile::size(filePath_.c_str());
    }

    return FILENSIZE;

}

int JinFileAsync::open(JFAccessFlag acc, JFOpenFlag ope, bool async)
{
    if(filePath_.length()<2) return -1; //可以完善为检查路径的合法性

    if(async)
    {
        this->handle_ = HDNULL;
        aop_ = AE::Get()->fileOpen(this,filePath_.c_str(),acc,ope);
        //JAssert(aop_);
        if(aop_==NULL){
            return -2;
        }
    }
    else
    {
#ifdef AE_USING_IOCP
        bool forAsync = true;
#else
        //异步标记是通知原生系统支持，这里用线程加同步读写实现，所以这里不要异步标志.
        bool forAsync = false;
#endif
        int ret = JinFile::open(filePath_.c_str(),acc,ope,hfile_,forAsync);
        if(ret != 0){
            return ret;
        }
        isOpen_ = true;
        aop_ = AE::Get()->attach(this);
        if(aop_==NULL){
            JinFile::close(hfile_);
            return -3;
        }
    }
    return 0;
}

bool JinFileAsync::read(JinAsynBuffer& buffer, const uint32_t bufPos, uint64_t readFrom, uint32_t byteToRead)
{
    JAssert(isOpen_);
    if(!isOpen_)return false;
    return AE::Get()->fileRead(aop_, readFrom, buffer, bufPos, byteToRead);
}

bool JinFileAsync::write(JinAsynBuffer& buffer, const uint32_t bufPos, uint64_t writeTo, uint32_t byteToWrite)
{
    JAssert(isOpen_);
    if(!isOpen_)return false;
    return AE::Get()->fileWrite(aop_, writeTo, buffer, bufPos, byteToWrite);
}

void JinFileAsync::close(bool async)
{
    if(isOpen_){
        if(async)
        {
            if(AE::Get()->fileClose(aop_))    return;
        }

        JinFile::close(hfile_);
        isOpen_ = false;
        if(aop_)AE::Get()->detach(aop_);
        aop_ = NULL;
    }
}

void JinFileAsync::OnMainPull(bool isSuccess, HdObject handle, AsynOpType type,
                              void *buf, uint64_t opSize, uint64_t tranSize,
                              void* user, uint64_t user64)
{
    UNUSED(user);
    switch(type)
    {
    case kFileOpen:
        isOpen_ = isSuccess;
        JAssert(isSuccess == (opSize == tranSize));
        //isOpen_ = tranSize>0 && opSize == tranSize;
        if(isSuccess){
            //hfile_ = (HdFile)user64;  //特殊! 2参handle并未赋值
            handle_ = handle;
            aop_ = AE::Get()->attach(this);  //二次attach
            if(aop_==NULL){
                JELog("error while asyn attach after open success.");
                JinFile::close(hfile_);
                isOpen_ = false;
            }
        }
        this->OnOpen(isOpen_,(int)tranSize);
        return;
    case kFileRead:
        JAssert(this->handle_ == handle);
        //当读取字节多余文件末可提供字节，操作本身是成功的，tranSize小于opSize
        //JAssert(isSuccess == (tranSize>0));
        this->OnRead(user64,buf,opSize,tranSize);
        return;
    case kFileWrite:
        JAssert(this->handle_ == handle);
        //应该与Read同理
        //JAssert(isSuccess == (tranSize>0));
        this->OnWrite(user64,buf,opSize,tranSize);
        return;
    case kFileClose:
        JAssert(this->handle_ == handle);
        this->OnClose();
        isOpen_ = false;
        if(aop_)AE::Get()->detach(aop_);
        aop_ = NULL;
        return;
    default:
        break;
    }


    JAssert4(false,"Unexpect AsynOpType=",type);
}

