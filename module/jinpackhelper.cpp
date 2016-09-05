#include "jinpackhelper.h"
#include "jinendian.h"
#include "jinlogger.h"

//设计的pack格式是: 2字节mainid 2字节assistid
//然后是一组一组的字段. 一组字段由1字节代表内建类型,1字节代表用户类型
//内建类型前面代表数字,同时表明num的长度;
//0x10代表是字符串,字符串需要指定长度,那么不同的组合类型表达放len的空间长度.
//例如kTypeV4Str, 1字节0x13,1字节用户type,4字节代表len,剩下len那么长的内容.
enum InnerType
{
    kTypeNum = 0,  //invalid
    kTypeNum1 = 1,
    kTypeNum2 = 2,
    kTypeNum4 = 4,
    kTypeNum8 = 8,
    kTypeVStr = 0x10,  //invalid
    kTypeV1Str = 0x10 | kTypeNum1,
    kTypeV2Str = 0x10 | kTypeNum2,
    kTypeV4Str = 0x10 | kTypeNum4,
    kTypeV8Str = 0x10 | kTypeNum8,
};
char validInner[16]={1,0,0,1,0,1,1,1,0,1,1,1,1,1,1,1}; //0=valid, other invalid

JinPackParser::JinPackParser(const char *buf, size_t len, size_t offset)
    : mainid_(0)
    , assistid_(0)
    , isValid_(false)
{
    if(len < offset)
        isValid_ = false;
    else
        isValid_ = parse(buf+offset,len-offset);
}

JinPackParser::~JinPackParser()
{

}

JinRoBuf JinPackParser::get(uint8_t key)
{
    JinRoBuf rbuf;
    rbuf.buf = NULL;
    rbuf.len = 0;
    if(isValid_)
    {
        rbuf = kvMap_.get(key,rbuf);
    }
    return rbuf;
}

#define PhFuncGet(fname,ftype) ftype JinPackParser::fname(uint8_t key)\
{\
    ftype rt = 0;\
    if(isValid_)\
    {\
        JinRoBuf rbuf;\
        rbuf = kvMap_.get(key,rbuf);\
        if(rbuf.len==sizeof(ftype))\
        {\
            memcpy(&rt,rbuf.buf,sizeof(ftype));\
        }\
    }\
    return rt;\
}
// rt = *(ftype*)rbuf.buf;
PhFuncGet(get8u,uint8_t)
PhFuncGet(get16u,uint16_t)
PhFuncGet(get32u,uint32_t)
PhFuncGet(get64u,uint64_t)
//uint8_t JinPackParser::get8u(uint8_t key)
//{
//    uint8_t rt = 0;
//    if(isValid_)
//    {
//        JinRoBuf rbuf;
//        rbuf = kvMap_.get(key,rbuf);
//        if(rbuf.len==1)
//        {
//            rt = *(uint8_t*)rbuf.buf;
//        }
//    }
//    return rt;
//}


JinString JinPackParser::getStr(uint8_t key)
{
    JinString rt;
    if(isValid_)
    {
        JinRoBuf rbuf;
        rbuf = kvMap_.get(key,rbuf);
        if(rbuf.len>0)
        {
            rt = JinString(rbuf.buf,rbuf.len);
        }
    }
    return rt;
}

bool JinPackParser::parse(const char *buf, size_t len)
{
    bool valid = false;
    if(len<4) return valid;
    mainid_ = fromLE(*(uint16_t*)(buf+0)); //fromLE(SafeAlignGetPtrValue16(buf+0));
    assistid_ = fromLE(*(uint16_t*)(buf+2)); //fromLE(SafeAlignGetPtrValue16(buf+2));

    size_t idx = 4;
    uint8_t usrkey, innertype;
    bool isVstr;
    JinRoBuf rbuf;
    while(idx < len)
    {
        valid = false;
        size_t vNum = 0;
        innertype = *(uint8_t*)(buf+idx);
        isVstr = (innertype > kTypeVStr);
        innertype &= 0x0F;
        if(validInner[innertype]!=0)break;
        ++ idx;

        if(idx >= len) break;
        usrkey = *(uint8_t*)(buf+idx);
        ++ idx;

        if(idx + innertype > len) break;
        if(isVstr)
        {
            switch(innertype)
            {
            case kTypeNum1:
                vNum = *(uint8_t*)(buf+idx);
                break;
            case kTypeNum2:
                vNum = *(uint16_t*)(buf+idx);//SafeAlignGetPtrValue16(buf+idx);
                break;
            case kTypeNum4:
                vNum = *(uint32_t*)(buf+idx);//SafeAlignGetPtrValue32(buf+idx);
                break;
            case kTypeNum8:
                vNum = *(uint64_t*)(buf+idx);//SafeAlignGetPtrValue64(buf+idx);
                break;
            default:
                break; //不可能 已经过滤过了.
            }
            if(vNum == 0)break;
            idx += innertype;
            if(idx + vNum > len) break;
        }
        else
        {
            vNum = innertype;
        }
        rbuf.buf = buf+idx;
        rbuf.len = vNum;
        kvMap_.set(usrkey,rbuf);
        idx += vNum;
        valid = true;
    }
    return valid;
}


JinPackJoiner::JinPackJoiner(uint16_t firstPadding)
    : validLen_(0)
    , firstPadding_(firstPadding)
{
    if(firstPadding > 0)
    {
        if(firstPadding < kInnerBufLen)
        {
            inner_.cat(innerBuf_,firstPadding);
            validLen_ += firstPadding;
        }
        else
        {
            JAssert("too long padding."==0);
            char *longPadding = (char *)JMalloc(firstPadding);
            if(!longPadding) validLen_ = -1;
            else
            {
                inner_.cat(longPadding,firstPadding);
                validLen_ += firstPadding;
            }
            JFree(longPadding);
        }
    }

    if(validLen_ >= 0)
    {
        memset(innerBuf_,0,4);
        inner_.cat(innerBuf_,4);
        validLen_ += 4;
    }
}

JinPackJoiner::~JinPackJoiner()
{

}

void JinPackJoiner::setHeadID(uint16_t mainid, uint16_t asstid)
{
    if(validLen_ < firstPadding_ + 4) return;
    char *buf = inner_.unsafe_str() + firstPadding_;
    mainid = toLE(mainid);
    memcpy(buf+0,&mainid,2);
    asstid = toLE(asstid);
    memcpy(buf+2,&asstid,2);
}

bool JinPackJoiner::pushNumber(uint8_t key, uint8_t n8)
{
    n8 = toLE(n8);
    return pushInner(kTypeNum1,key,&n8,1);
}

bool JinPackJoiner::pushNumber(uint8_t key, uint16_t n16)
{
    n16 = toLE(n16);
    return pushInner(kTypeNum2,key,&n16,2);
}

bool JinPackJoiner::pushNumber(uint8_t key, uint32_t n32)
{
    n32 = toLE(n32);
    return pushInner(kTypeNum4,key,&n32,4);
}

bool JinPackJoiner::pushNumber(uint8_t key, uint64_t n64)
{
    n64 = toLE(n64);
    return pushInner(kTypeNum8,key,&n64,8);
}

bool JinPackJoiner::pushString(uint8_t key, const char *buf, uint32_t len)
{
    //if(len > 0xFFFF) return false;
    //if(len > 0xFFFFFFFF) return false;
    JAssert(buf);
    if(len == 0)
    {
        len = strlen(buf);
    }
    if(len > 0xFFFF)
    {
        //JPLog("len too long, len=%u",len);
        JAssert3(len <= 0xFFFF,"len too long",&len);
        uint32_t num = toLE((uint32_t)len);
        return pushInner(kTypeV4Str,key,&num,4,buf,len);
    }
    if(len > 0xFF)
    {
        uint16_t num = toLE((uint16_t)len);
        return pushInner(kTypeV2Str,key,&num,2,buf,len);
    }
    else
    {
        uint8_t num = toLE((uint8_t)len);
        return pushInner(kTypeV1Str,key,&num,1,buf,len);
    }
}

char *JinPackJoiner::buff( size_t *len )
{
    if(validLen_ > 0)
    {
        JAssert((int)inner_.length() == validLen_);
        if(len) *len = inner_.length();
        return inner_.unsafe_str();  //c_str();
    }
    else
    {
        return NULL;
    }
}

int JinPackJoiner::length()
{
    if(validLen_ > 0)
    {
        JAssert((int)inner_.length() == validLen_);
        return validLen_;
    }
    else
    {
        return 0;
    }
}

bool JinPackJoiner::pushInner(uint8_t innerkey, uint8_t usrkey,
                              const void *numbuf, uint16_t nblen,
                              const void *buf, uint32_t len)
{
    JAssert(nblen < 16);
    if(validLen_ < firstPadding_ + 4) return false;
    inner_.cat((const char*)&innerkey, 1);
    inner_.cat((const char*)&usrkey, 1);
    inner_.cat((const char*)numbuf, nblen);
    validLen_ += (nblen+2);
    if(len>0)
    {
        inner_.cat((const char*)buf, len);
        validLen_ += len;
    }
    if(validLen_ != (int)inner_.length())
    {
        validLen_ = -1;
        return false;
    }
    return true;
}
