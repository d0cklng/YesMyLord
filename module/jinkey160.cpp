#include "jinkey160.h"
#include <string.h>
#include "jinsharedata.h"
#include "jinmemorycheck.h"
#include "jinassert.h"
#include "jinrandom.h"


static const uint8_t prefix[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};  // prefix[异或值] = 相同的前缀数.

class JinKeyData : public JinShareData
{
private:
    const static int kKeyLen = 20;
    friend class JinKey160;
    JinKeyData()
        : toStrCache_(NULL)
    {}

    virtual ~JinKeyData()
    {
        if(toStrCache_)JFree(toStrCache_);
    }

    inline void keySet(const unsigned char *buf)
    {
        JAssert(toStrCache_==NULL);
        memcpy(key_,buf,20);
    }


    inline const char* toString()
    {
        if(toStrCache_==NULL)
        {
            toStrCache_ = (char*)JMalloc(kKeyLen*2+1);
            static const char *codesHex = "0123456789ABCDEF";
            uint8_t i=0;
            for(;i<kKeyLen*2;i++)
            {
                toStrCache_[i] = codesHex[getBit4At(i)];
            }
            toStrCache_[i] = 0;
        }

        return toStrCache_;

    }

    inline uint8_t sameStartAsBit4(JinKeyData* data)
    {
        for(int i=0;i<20;i++)
        {
            if(key_[i]!=data->key_[i])
            {
                if((key_[i]&0xF0) == (data->key_[i]&0xF0))
                {
                    return i*2 + 1;
                }
                else
                {
                    return i*2;
                }
            }
        }
        return 40;
    }

    inline uint8_t samePrefixBit(JinKeyData* data) const
    {
        if(!data)return 0;
        int samePrefix = 0;
        for(int i=0;i<20;i++)
        {
            samePrefix = prefix[key_[i]^data->key_[i]];
            if(samePrefix!=8){
                return i*8+samePrefix;
            }
        }
        return 160;
    }

    inline uint8_t getBit4At(uint8_t idx) const
    {
        JAssert(idx<40);
        return ((key_[idx/2]) >> ((1-idx%2)*4)) & 0x0F;
    }

    inline int compare(JinKeyData* data)
    {
        return memcmp(key_,data->key_,kKeyLen);
    }

    unsigned char* x(JinKeyData* data,unsigned char* keyResult)
    {
        for(int i=0;i<5;i++)
        {
            (*(uint32_t*)&keyResult[i*4]) =
                    (*(uint32_t*)&key_[i*4]) ^ (*(uint32_t*)&data->key_[i*4]);
        }
        return keyResult;
    }

    unsigned char key_[kKeyLen];
    char* toStrCache_;
};

JinKey160::JinKey160()
    : data_(NULL)
{
}

JinKey160::JinKey160(const unsigned char buf[])
    : data_(NULL)
{
    if(buf) setOnlyOnce(buf);  // 处理=0的情况.
}

JinKey160::~JinKey160()
{
    if(data_)data_->detach();
}

JinKey160::JinKey160(const JinKey160 &o)
{
    data_ = o.data_;
    if(data_ && !data_->attach())
        data_ = NULL;
}

JinKey160& JinKey160::operator=(const JinKey160& o)
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

bool JinKey160::operator ==(const JinKey160 &other) const
{
    if(this->data_ == other.data_) return true;
    if(this->data_==NULL || other.data_== NULL)return false;
    return (this->data_->compare(other.data_)==0);
}

bool JinKey160::isEqual(const JinKey160 *pkey) const
{
    if(this->data_ == pkey->data_) return true;
    if(this->data_==NULL || pkey->data_== NULL)return false;
    return (this->data_->compare(pkey->data_)==0);
}

uint8_t JinKey160::sameStartAsBit4(const JinKey160 &other)
{
    if(!data_)return 0;
    return data_->sameStartAsBit4(other.data_);
}

uint8_t JinKey160::samePrefixBit(const JinKey160 &other) const
{
    if(!data_)return 0;
    return data_->samePrefixBit(other.data_);
}

uint8_t JinKey160::getBit4At(uint8_t idx) const
{
    if(!data_)return 0;
    return data_->getBit4At(idx);
}

const char *JinKey160::toString() const
{
    if(!data_) return "<Invalid Key>";
    return data_->toString();
}

const unsigned char *JinKey160::rawBuf() const
{
    return data_->key_;
}

bool JinKey160::isValid() const
{
    return (data_!=NULL);
}

JinKey160 JinKey160::x(const JinKey160 &o)
{
    JinKey160 xkey;
    unsigned char xdata[20];
    data_->x(o.data_,xdata);
    xkey.setOnlyOnce(xdata);
    return xkey;
}

int JinKey160::compareDistanceToMine(const JinKey160 &left, const JinKey160 &right)
{
    uint8_t sl = samePrefixBit(left);
    uint8_t rl = samePrefixBit(right);
    if(sl==rl)
    {
        if(sl==160)return 0;
        rl = left.samePrefixBit(right);
        if(rl==160)return 0;
        uint8_t c = getBit4At(rl/4);
        return (int)((left.getBit4At(rl/4)^c) > (right.getBit4At(rl/4)^c));
    }
    else
    {
        return (sl<rl)?(-1):(1);
    }
}

JinKey160 JinKey160::random(const unsigned char *buf, int bytes)
{
    unsigned char mbuf[20];
    for(int i=0;i<10;i++)
    {
        *(uint16_t*)&mbuf[i*2] = (uint16_t)(JinRandom::rnd()& 0xFFFF);
    }
    if(buf && bytes)memcpy(mbuf,buf,bytes);
    return JinKey160(mbuf);
}

uint32_t JinKey160::JinKey160HashFunc(const JinKey160 &key)
{
    return BKDRHashHex(key.rawBuf(),20);
}

void JinKey160::setOnlyOnce(const unsigned char *buf)
{
    JAssert(data_==NULL);
    if(data_)return;

    data_ = JNew(JinKeyData);
    if(data_){ data_->keySet(buf);}
}
