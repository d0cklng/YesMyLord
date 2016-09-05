#include "jinbuffer.h"
#include "jinsharedata.h"
#include "jinassert.h"
#include "jinmemorycheck.h"
#include <string.h>  //memcpy  strlen

class JinBufferData : public JinShareData
{
public:
    JinBufferData()
        : buf_(0)
        , len_(0)
        , validLen_(0)
    {}


    void copy(const void *ptr, size_t len)
    { //set the data as ptr provide
        if(len <= len_)
        {
            validLen_ = len;
            memcpy(buf_,ptr,len);
            return;
        }

        if(alloc(len))
        {
            memcpy(buf_,ptr,len);
        }
        else
        {  //error occur.
            if(buf_) JFree(buf_);
            buf_ = NULL;
            len_ = 0;
            validLen_ = 0;
        }
    }

    void set(void *ptr, size_t len)
    {
        validLen_ = len;
        len_ = len;
        buf_ = ptr;
    }

    ///alloc a buff with length len.
    bool alloc(size_t len, bool keepOldData=false)
    {
        if(len <= len_)
        {
            validLen_ = len;
            return true;
        }

        void *nbuf = JMalloc(len);
        if(!nbuf) return false;
        if(keepOldData && buf_)
        {
            memcpy(nbuf,buf_,len_);
        }
        if(buf_) JFree(buf_);
        buf_ = nbuf;
        len_ = len;
        validLen_ = len;
        return true;
    }

    void* buf()
    {
        return buf_;
    }

    size_t len() const
    {
        return validLen_;
    }

    size_t capability() const
    {
        return len_;
    }
protected:
    virtual ~JinBufferData()
    {
        if(buf_)JFree(buf_);
        buf_ = 0;
        len_ = 0;
        validLen_ = 0;
    }
private:
    void *buf_;
    size_t len_;
    size_t validLen_;
};

//============================================================

JinBuffer::JinBuffer()
    : data_(0)
{
    //printf("JinBuffer\n");
}

JinBuffer::~JinBuffer()
{
    //printf("~JinBuffer\n");
    if(data_)data_->detach();
}

///alloc a buff with length len.
JinBuffer::JinBuffer(int32_t len)
{
    //data_ = JNew JinBufferData();
    data_ = JNew(JinBufferData);
    if(data_){ data_->alloc(len); }
}

JinBuffer::JinBuffer(const char* s)
{
    data_ = JNew(JinBufferData);
    if(data_){ data_->copy(s,strlen(s)+1); }
}

JinBuffer::JinBuffer(const void *ptr,uint32_t len)
{
    //data_ = JNew JinBufferData();
    data_ = JNew(JinBufferData);
    if(data_){ data_->copy(ptr,len); }
}

JinBuffer::JinBuffer(const JinBuffer &o)
{
    data_ = o.data_;
    if(data_ && !data_->attach())
        data_ = NULL;
}

JinBuffer& JinBuffer::operator=(const JinBuffer& o)
{
    if(data_)data_->detach();
    data_ = o.data_;
    if(data_)
    {
        if(!data_->attach())
            data_ = NULL;
    }
    return *this;
}

JinBuffer& JinBuffer::operator=(const char* s)
{
    if(data_)data_->detach();
    //data_ = JNew JinBufferData();
    if(!s)return *this;
    data_ = JNew(JinBufferData);
    if(data_){ data_->copy(s,strlen(s)+1); }
    return *this;
}

/// take owner of ptr.
/// this is not a good but sometimes efficient.
void JinBuffer::eat(void *ptr,size_t len)
{
    if(data_)data_->detach();
    //data_ = JNew JinBufferData();
    data_ = JNew(JinBufferData);
    if(data_){ data_->set(ptr,len); }
}

void JinBuffer::eat(char *str)
{
    if(data_)data_->detach();
    //data_ = JNew JinBufferData();
    data_ = JNew(JinBufferData);
    if(data_){ data_->set(str,strlen(str)+1); }
}

size_t JinBuffer::length() const
{
    return data_?data_->len():0;
    //return validLen_;
}

//size_t JinBuffer::capability() const
//{
//    return data_?data_->len():0;
//}

bool JinBuffer::resize(size_t len, bool keepOldData)
{
    if(data_==NULL)
    {
        data_ = JNew(JinBufferData);
    }
    if(data_) return data_->alloc(len,keepOldData);
    return false;
}



JinBuffer::operator const char *()
{
    if(data_)
        return (const char*)data_->buf();
    else
        return (const char*)0;
}

unsigned char* JinBuffer::buff()
{
    if(data_)
        return (unsigned char*)data_->buf();
    else
        return (unsigned char*)0;
}

const unsigned char *JinBuffer::cbuff()const
{
    if(data_)
        return (const unsigned char*)data_->buf();
    else
        return (const unsigned char*)0;
}

const char *JinBuffer::cstr() const
{
    if(data_)
        return (const char*)data_->buf();
    else
        return (const char*)0;
}

void JinBuffer::clear()
{
    if(data_)data_->detach();
    data_ = 0;
}

bool JinBuffer::isNull()
{
    return (data_==NULL || data_->len()==0);
}


uint16_t SafeAlignGetPtrValue16(const void *param)
{
    uint16_t v;
    memcpy(&v,param,sizeof(uint16_t));
    return v;
}
uint32_t SafeAlignGetPtrValue32(const void *param)
{
    uint32_t v;
    memcpy(&v,param,sizeof(uint32_t));
    return v;
}
uint64_t SafeAlignGetPtrValue64(const void *param)
{
    uint64_t v;
    memcpy(&v,param,sizeof(uint64_t));
    return v;
}

