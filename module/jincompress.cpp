#include "jincompress.h"
#include "jinbuffer.h"
#include "jinmemorycheck.h"
#include "jinassert.h"
#include "jinlogger.h"

JinCompress::JinCompress(JCompressType type)
    : type_(type)
    , buffer_(NULL)
    , compress_state_(NULL)
    , decompress_state_(NULL)
{
}

JinCompress::~JinCompress()
{
    if(compress_state_) JFree(compress_state_);
    if(decompress_state_) JFree(decompress_state_);
    if(buffer_) JDelete(buffer_);
}

JinRoBuf JinCompress::compress(const void *buf, size_t bufLen)
{
    JAssert(bufLen!=0);
    if(buffer_ == NULL)
    {
        buffer_ = JNew(JinBuffer);
    }
    bool ret = false;
    if(buffer_)
    {
        switch(type_)
        {
        case kTypeQuickLz:
            ret = compress_qlz(buf,bufLen);
            break;
        default:
            ret = false;
            break;
        }
    }

    JinRoBuf rbuf;
    if(ret)
    {
        rbuf.buf = buffer_->cstr();
        rbuf.len = buffer_->length();
        return rbuf;
    }
    return rbuf;
}

JinRoBuf JinCompress::decompress(const void *buf, size_t bufLen)
{
    JAssert(bufLen!=0);
    if(buffer_ == NULL)
    {
        buffer_ = JNew(JinBuffer);
    }
    bool ret = false;
    if(buffer_)
    {
        switch(type_)
        {
        case kTypeQuickLz:
            ret = decompress_qlz(buf,bufLen);
            break;
        default:
            ret = false;
            break;
        }
    }

    JinRoBuf rbuf;
    if(ret)
    {
        rbuf.buf = buffer_->cstr();
        rbuf.len = buffer_->length();
        return rbuf;
    }
    return rbuf;
}

bool JinCompress::isStreamMode()
{
    if(type_ == kTypeQuickLz) return true;

    return false;
}

bool JinCompress::isSupportVariantLevel()
{
    if(type_ == kTypeQuickLz) return false;

    return false;
}



#include "quicklz/quicklz.h"
bool JinCompress::compress_qlz(const void *buf, size_t bufLen)
{
    if(compress_state_ == NULL)
    {
        compress_state_ = JMalloc(sizeof(qlz_state_compress));
        memset(compress_state_, 0, sizeof(qlz_state_compress));
    }
    size_t goodSize = ((bufLen+400)/(4*1024)+1)*(4*1024);
    if(!buffer_->resize(goodSize)) return false;
    goodSize = qlz_compress(buf, (char*)buffer_->buff(), bufLen, (qlz_state_compress*)compress_state_);
    JDLog("Compress %"PRISIZE"=>%"PRISIZE,bufLen,goodSize);
    return buffer_->resize(goodSize);  //由大变小一定会成功
}

bool JinCompress::decompress_qlz(const void *buf, size_t bufLen)
{
    if(decompress_state_ == NULL)
    {
        decompress_state_ = JMalloc(sizeof(qlz_state_decompress));
        memset(decompress_state_, 0, sizeof(qlz_state_decompress));
    }
    if(bufLen <= 9) return false;
    size_t c = qlz_size_compressed((const char*)buf);
    if(c != bufLen) return false;
    c = qlz_size_decompressed((const char*)buf);
    size_t goodSize = (c/(4*1024)+1)*(4*1024);
    if(!buffer_->resize(goodSize)) return false;
    goodSize = qlz_decompress((const char*)buf,buffer_->buff(),(qlz_state_decompress*)decompress_state_);
    JDLog("Decompress %"PRISIZE"=>%"PRISIZE,bufLen,goodSize);
    JAssert(goodSize == c);
    return buffer_->resize(goodSize);  //由大变小一定会成功
}

