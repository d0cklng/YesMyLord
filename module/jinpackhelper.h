#ifndef JINPACKHELPER_H
#define JINPACKHELPER_H
#include "jinhashmap.h"
#include "jinstring.h"
#include "jinbuffer.h"

//两种基本类型,一种要带长度,一种type本身就可以知道长度.
#define JPTYPE_1 1
#define JPTYPE_2 2
#define JPTYPE_4 4
#define JPTYPE_8 8

class JinPackParser
{
public:
    JinPackParser(const char* buf, size_t len, size_t offset);
    ~JinPackParser();
    bool isValid(){return isValid_;}
    uint16_t mainID(){return mainid_;}
    uint16_t asstID(){return assistid_;}

    JinRoBuf get(uint8_t key);
    uint8_t  get8u(uint8_t key);
    uint16_t get16u(uint8_t key);
    uint32_t get32u(uint8_t key);
    uint64_t get64u(uint8_t key);
    JinString getStr(uint8_t key);
private:
    uint16_t mainid_;
    uint16_t assistid_;
private:
    bool parse(const char* buf, size_t len);
    JinHashMap<uint8_t,JinRoBuf> kvMap_;
    bool isValid_;
};

class JinPackJoiner
{
public:
    JinPackJoiner(uint16_t firstPadding=1); //空出来几个字节. NetPacket要留1字节头.
    ~JinPackJoiner();
    void setHeadID(uint16_t mainid, uint16_t asstid);
    //return false基本也表示valid也是否定的.
    bool pushNumber(uint8_t key, uint8_t n8);
    bool pushNumber(uint8_t key, uint16_t n16);
    bool pushNumber(uint8_t key, uint32_t n32);
    bool pushNumber(uint8_t key, uint64_t n64);
    bool pushString(uint8_t key, const char* buf, uint32_t len=0);
    char* buff( size_t *len=NULL ); //不合法的返回NULL.
    int length();  //0或者更小都是失败的.
protected:
    bool pushInner(uint8_t innerkey, uint8_t usrkey,
                   const void* numbuf, uint16_t nblen,
                   const void* buf = NULL, uint32_t len=0);
private:
    const static size_t kInnerBufLen = 32;
    char innerBuf_[kInnerBufLen];
    int validLen_;
    uint16_t firstPadding_;
    JinString inner_;
};

#endif // JINPACKHELPER_H
