#include "jinstring.h"
#include "jinmemorycheck.h"
#include "jinsharedata.h"
#include "jinassert.h"
#include "jinplatformgap.h"
#include <string.h>

static const char* _NullString = "(null)";  //cause by malloc fail.
//static const char* _NullString = "";

class JinStringData : public JinShareData
{
    friend class JinString;
public:
    JinStringData()
        : buf_(NULL)
        , bufLen_(0)
        , stringLen_(0)
    {}

    virtual ~JinStringData()
    {
        if(buf_)JFree(buf_);
        buf_ = 0;
        bufLen_ = 0;
        stringLen_ = 0;
    }

    void set(const char *str, size_t len=STRNPOS, size_t lenPredict=STRNPOS)  //if len==0,detect by strlen
    {  //lenKeep 为cat优化，避免set一次，cat的时候又发生一次realloc
        JAssert(str);

        if(len == STRNPOS){
            len = strlen(str);
        }

        if(resize((lenPredict!=STRNPOS&&lenPredict>len)?lenPredict:len,false))
        {
            if(len>0)memcpy(buf_,str,len);
            stringLen_ += len;
            buf_[stringLen_]='\0';
        }
    }

    void cat(const char *str, size_t len=STRNPOS)
    {
        JAssert(str);

        if(len == STRNPOS){
            len = strlen(str);
        }

        if(resize(stringLen_ + len,true))
        {
            if(len>0)
            {
                memcpy(buf_+stringLen_,str,len);
            }
            stringLen_ += len;
            buf_[stringLen_]='\0';
        }
    }

    void cat(JinStringData *o)
    {
        if(!o)return;
        cat(o->buf(),o->len());
    }

    JinStringData* copy( size_t lenPredict=STRNPOS )
    {
        JinStringData* newData = JNew(JinStringData);
        if(newData){
            newData->set(buf_,stringLen_,lenPredict);
            if(newData->isNull()){  //malloc faild?
                JDelete(newData);
                newData = NULL;
            }
        }
        return newData;
    }

    JinStringData* toReadableHex()
    {
        JinStringData* newData = JNew(JinStringData);
        if(newData){
            newData->resize(stringLen_*2+1,false);
            if(newData->isNull()){  //malloc faild?
                JDelete(newData);
                newData = NULL;
            }
            static const char *codesHex = "0123456789ABCDEF";
            for(size_t i=0;i<stringLen_;i++)
            {
                *(newData->buf()+i*2) =  codesHex[((uint8_t)buf_[i])>>4];
                *(newData->buf()+i*2+1) =  codesHex[((uint8_t)buf_[i])&0x0F];
            }
            *(newData->buf()+stringLen_*2) = 0;
            newData->setStringLen(stringLen_*2);
        }
        return newData;
    }

    void fromReadableHex(const char *rawbuf, size_t len)
    {  //由于'a'>'A'>'0', 按照合理输入得合理结果,不合理输入得不合理结果处理.
        resize(len/2,false);
        if(isNull())return;
        for(size_t i=0; i<len; i++)
        {
            uint8_t c;
            char src = rawbuf[i];
            if(src>='a') c = src -'a' + 0xa;
            else if(src>='A') c = src -'A' + 0xa;
            else c = src - '0';
            if(i%2==0) buf_[i/2] = (c << 4);
            else buf_[i/2] |= c;
        }
    }

    inline char* buf()
    {
        return buf_;
    }

    inline size_t len() const
    {
        return stringLen_;
    }

    inline int32_t ref()
    {
        return JinShareData::ref();
    }

    inline bool isNull()
    {
        return (buf_==NULL);
    }

protected:

    bool resize(size_t len,bool copy)
    {
        size_t newBufLen = (bufLen_?bufLen_:63);
        while(len+1 > newBufLen)    //使len值保持N个二进制1
        {   newBufLen = newBufLen*2+1;  }
        while(newBufLen>63 && len+1 < newBufLen/8)
        {   newBufLen>>=1;  }

        if(newBufLen != bufLen_)
        {
            char* newBuf = (char*)JMalloc(newBufLen);
            if(!newBuf)return false;
            if(copy && buf_ && stringLen_>0){
                memcpy(newBuf,buf_,stringLen_+1);
            }
            else{
                newBuf[0] = '\0';
                stringLen_ = 0;
            }
            if(buf_)JFree(buf_);
            buf_ = newBuf;
            bufLen_ = newBufLen;

        }

        return true;
    }
    void setStringLen(size_t sl)
    {
        stringLen_ = sl;
    }

private:
    char *buf_;
    size_t bufLen_;
    size_t stringLen_;
};

//============================================================

JinString::JinString()
    : data_(0)
{
    //Null string.
}

JinString::~JinString()
{
    if(data_)data_->detach();
    data_ = NULL;
}

JinString::JinString(const char* s, size_t len)
{
    data_ = JNew(JinStringData);
    if(data_){ data_->set(s,len);}
}

#include <string>
JinString::JinString(const JinString &o)
{
    data_ = o.data_;
    if(data_ && !data_->attach())
        data_ = NULL;
}

JinString& JinString::operator=(const JinString& o)
{
    if(data_)data_->detach();
    data_ = o.data_;
    if(data_ && !data_->attach())
        data_ = NULL;
    return *this;
}

JinString &JinString::operator=(const char *s)
{
    if(data_)data_->detach();
    //data_ = JNew JinBufferData();
    data_ = JNew(JinStringData);
    if(data_){ data_->set(s); }
    return *this;
}

JinString &JinString::operator +=(const JinString &o)
{
    if(data_){
        if(data_->ref()>1){
            JinStringData* newData = data_->copy(data_->len()+o.data_->len());
            if(!newData) return *this;
            data_->detach();
            data_ = newData;
        }
        data_->cat(o.data_);
    }
    else
    {
        data_ = o.data_;
        if(!data_->attach())
            data_ = NULL;
    }
    return *this;
}

JinString &JinString::operator +=(const char *str)
{
    size_t len = strlen(str);
    return cat(str,len);
}

JinString &JinString::operator +=(int num)
{
    char buf[16];
    itoa(num,buf,10);
    size_t len = strlen(buf);
    return cat(buf,len);
}

bool JinString::operator <(const JinString &right) const
{
    if(data_==NULL || right.data_==NULL)
    {
        if(right.data_!=NULL)return true;
        return false;
    }
    return strcmp(data_->buf(),right.data_->buf())<0;
}

bool JinString::operator <=(const JinString &right) const
{
    if(data_==NULL || right.data_==NULL)
    {
        if(data_!=NULL)return false;
        return true;
    }
    return strcmp(data_->buf(),right.data_->buf())<=0;
}

bool JinString::operator >(const JinString &right) const
{
    if(data_==NULL || right.data_==NULL)
    {
        if(data_!=NULL)return true;
        return false;
    }
    return strcmp(data_->buf(),right.data_->buf())>0;
}

bool JinString::operator >=(const JinString &right) const
{
    if(data_==NULL || right.data_==NULL)
    {
        if(right.data_!=NULL)return false;
        return true;
    }
    return strcmp(data_->buf(),right.data_->buf())>=0;
}

bool JinString::operator!=(const JinString &rhs) const
{
    if(data_==NULL || rhs.data_==NULL)
    {
        return (data_ != rhs.data_);
    }
    else
    {
        if(data_->len()!=rhs.data_->len())  return true;
        else{
            return memcmp(data_->buf(),rhs.data_->buf(),data_->len())!=0;
        }
    }
}

bool JinString::operator!=(const char *str) const
{
    if(data_==NULL || str==NULL)
    {
        return (data_ || str);
    }
    else
    {
        size_t len = strlen(str);
        if(data_->len()!=len)  return true;
        else{
            return memcmp(data_->buf(),str,len)!=0;
        }
    }
}

size_t JinString::find(const JinString str, size_t startPos, bool nocase) const
{
    return this->find(str.c_str(),startPos,nocase);
}

JinString operator+(const JinString &lhs, const JinString &rhs)
{
    JinString rst = lhs;
    rst += rhs;
    return rst;
}

JinString operator+(const JinString &lhs, const char *rhs)
{
    JinString rst = lhs;
    rst += rhs;
    return rst;
}

JinString operator+(const char *lhs, const JinString &rhs)
{
    JinString rst(lhs);
    rst += rhs;
    return rst;
}

JinString operator+ (const JinString& lhs, char rhs)
{
    JinString rst(lhs);
    rst.cat(&rhs,1);
    return rst;
}

JinString operator+ (char lhs, const JinString& rhs)
{
    JinString rst(&lhs,1);
    rst += rhs;
    return rst;
}

JinString operator+(const JinString &lhs, int rhs)
{
    JinString rst = lhs;
    rst += rhs;
    return rst;
}

//nocase 忽略大小写.
inline bool isChrEqual(char a, char b, bool nocase=false)
{
    if(nocase)
    {
        if(a>='A' || a<='Z') a+=('a'-'A');
        if(b>='A' || b<='Z') b+=('a'-'A');
        return a==b;
    }
    else
    {
        return a==b;
    }
}

//#define SupportLongSubString
#ifdef SupportLongSubString  //用于支持较长的sub字符串.
    typedef unsigned short OCCType;
#else
    typedef unsigned char OCCType;
#endif
size_t JinString::find(const char *sub, size_t startPos, bool nocase) const
{
    if(!data_ || data_->len()<startPos)return STRNPOS;
    ///[Sun 90] D.M. Sunday: A Very Fast Substring Search Algorithm. Communications of the ACM, 33, 8, 132-142 (1990)
    /// http://www-igm.univ-mlv.fr/~lecroq/string/
    /// http://www.iti.fh-flensburg.de/lang/algorithmen/pattern/sundayen.htm
    /// http://blog.csdn.net/force_eagle/article/details/10340907
    ///[Sunday算法]根据网上的算法，纠错、调整、改进
    //预处理和strlen一起做,原本算法这个数组是要预计算移位数，但是我不先计算长度.
    //所以重新约定值v含义：len-(v-1）= len-v+1 = 需移动的字节数. v=0表示无法匹配.
    OCCType occ[256];
    memset(occ,0,256);
    int subLen=0;
    unsigned char ch;
    while((ch = (unsigned char)sub[subLen])){
        occ[ch] = (OCCType)++subLen;
    }
    JAssert(subLen<(1<<(sizeof(OCCType)*8)));
    if(subLen+startPos>data_->len())return STRNPOS;

    for(int matchAt=1;matchAt<=subLen;matchAt++)
    {
        if(!isChrEqual(data_->buf()[startPos + subLen - matchAt]
                , sub[subLen - matchAt], nocase))
        {  //如果不匹配，按算法移动startPos重新匹配.
           startPos += subLen - occ[(unsigned char) (data_->buf()[startPos + subLen])] + 1;
           if(subLen+startPos>data_->len())return STRNPOS;
           matchAt = 0;
        }
    }
    return startPos;
}

JinString JinString::left(size_t n)
{
    if(data_){
        if(n>data_->len())n=data_->len();
        if(n>0){
            return JinString(data_->buf(),n);
        }
    }
    return JinString();
}

JinString JinString::mid(size_t pos, size_t n) const
{
    if(data_){
        if(pos<data_->len()){
            if(n+pos>data_->len())n=data_->len()-pos;
            if(n>0){
                return JinString(data_->buf()+pos,n);
            }
        }
    }
    return JinString();
}

JinString JinString::right(size_t n)
{
    if(data_){
        if(n>data_->len())n=data_->len();
        if(n>0){
            return JinString(data_->buf()+(data_->len()-n),n);
        }
    }
    return JinString();
}

bool JinString::endWith(char ch)
{
    return (data_ && data_->len()>0 && data_->buf()[data_->len()-1]==ch);
}

bool JinString::endWith(const char *str)
{
    if(!data_)return false;
    size_t len = strlen(str);
    if(data_->len()<len)return false;
    for(size_t idx=1;idx<=len;idx++)
    {
        if(str[len-idx] != data_->buf()[data_->len()-idx])return false;
    }
    return true;
}

void JinString::clear()
{
    if(data_)data_->detach();
    data_ = NULL;
}

size_t JinString::length()const
{
    if(!data_)return 0;
    return data_->len();
}

// http://porg.es/blog/counting-characters-in-utf-8-strings-is-faster
// int porges_strlen2(char *s)
size_t JinString::utf8length()const
{
    if(!data_)return 0;
    int i = 0;
    int iBefore = 0;
    char *s = data_->buf();
    int count = 0;

    while (s[i] > 0)
ascii:  i++;

    count += i-iBefore;
    while (s[i])
    {
        if (s[i] > 0)
        {
            iBefore = i;
            goto ascii;
        }
        else
            switch (0xF0 & s[i])
            {
            case 0xE0: i += 3; break;
            case 0xF0: i += 4; break;
            default:   i += 2; break;
            }
        ++count;
    }
    return count;
}

JinString JinString::toReadableHex()
{
    if(!data_)return JinString();
    JinString out;
    out.data_ = data_->toReadableHex();
    return out;
}

JinString JinString::fromReadableHex(const char *rawbuf, size_t len)
{
    JinString out;
    out.data_ = JNew(JinStringData);
    if(out.data_){
        out.data_->fromReadableHex(rawbuf,len);
    }
    return out;
}

const char *JinString::c_str()const  //TODO 确定是否以\0结尾.
{
    if(!data_)return _NullString;
    return (const char*)data_->buf();
}

//安全的做法是在已有length范围内单线程修改.
char *JinString::unsafe_str()
{
    JAssert(data_);
    return data_->buf();
}

JinString &JinString::cat(const char *str, size_t len)
{
    if(data_){
        if(len==STRNPOS)len = strlen(str);
        if(data_->ref()>1){
            JinStringData* newData = data_->copy(data_->len()+len);
            if(!newData)return *this;
            data_->detach();
            data_ = newData;
        }
        data_->cat(str,len);
    }
    else
    {
        data_ = JNew(JinStringData);
        if(data_){ data_->set(str,len);}
    }
    return *this;
}

int JinString::stricmp(const char *target, size_t len, size_t startPos)
{ //将末尾字符置换出来 stricmp后恢复.
    if(startPos > data_->len()) return -1;
    if(len==STRNPOS) len = data_->len() - startPos;
    char rp = data_->buf()[startPos+len];
    data_->buf()[startPos+len] = '\0';
#ifdef JWIN
    int ret = _stricmp(data_->buf()+startPos, target);
#else
    int ret = strcasecmp(data_->buf()+startPos, target);
#endif
    data_->buf()[startPos+len] = rp;
    return ret;
}
