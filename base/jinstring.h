#ifndef JINSTRING_H
#define JINSTRING_H

/* ******************************
 *
 * 仿经典string
 * 依赖: jinmemorycheck jinassert
 * 间接依赖: jinsharedata
 * 特性： copy-on-write
 *
 * ******************************/

#include <stddef.h>
#define STRNPOS ((size_t)(-1))

class JinStringData;
class JinString
{
public:
    JinString();
    ~JinString();
    JinString(const char* s, size_t len=STRNPOS);
    JinString(const JinString &o);
    JinString& operator=(const JinString& o);
    JinString& operator=(const char* s);

    //JinString& operator+(const JinString& o);
    //JinString& operator+(const char* s);
    /// Concatenation
    JinString& operator +=( const JinString& o);
    JinString& operator +=( const char *str );
    JinString& operator +=( int num );

    friend JinString operator+ (const JinString& lhs, const JinString& rhs);
    friend JinString operator+ (const JinString& lhs, const char*   rhs);
    friend JinString operator+ (const char*   lhs, const JinString& rhs);
    friend JinString operator+ (const JinString& lhs, char          rhs);
    friend JinString operator+ (char          lhs, const JinString& rhs);
    friend JinString operator+ (const JinString& lhs, int           rhs);

    /// Equality
    //bool operator==(const JinString &rhs) const;
    //bool operator==(const char *str) const;

    /// Comparison
    bool operator < ( const JinString& right ) const;
    bool operator <= ( const JinString& right ) const;
    bool operator > ( const JinString& right ) const;
    bool operator >= ( const JinString& right ) const;

    /// Inequality
    bool operator!=(const JinString &rhs) const;
    bool operator!=(const char *str) const;

    operator const char*() const{return c_str();}

    size_t find(const JinString str, size_t startPos=0, bool nocase=false) const;
    size_t find(const char* sub, size_t startPos=0, bool nocase=false) const;
    JinString left(size_t n);
    JinString mid(size_t pos,size_t n) const;
    JinString right(size_t n);
    bool endWith(char ch);
    bool endWith(const char* str);
    void clear();
    size_t length() const;
    size_t utf8length() const;
    //size_t find(JinString& sub);
    //size_t rfind(JinString& sub);

    JinString toReadableHex();
    static JinString fromReadableHex(const char* rawbuf, size_t len);

    const char* c_str() const;
    char* unsafe_str();  //unsafe internal buf.
//protected:
    JinString& cat(const char* str, size_t len=STRNPOS);
    int stricmp(const char *target, size_t len=STRNPOS, size_t startPos=0); //startPos起len那么长跟target比较
protected:
    //用于JinHashMap类似类.
//    static uint32_t valueHash(const JinString &str)
//    {    return BKDRHashHex(str.c_str(),str.length());  }
//    static bool isEqual(const JinString &a, const JinString &b)
//    {
//        if(a.length() != b.length()) return false;
//        return !(a!=b);
//    }
private:
    JinStringData* data_;
};

#endif // JINSTRING_H
