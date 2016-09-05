#ifndef JINNETADDRESS_H_201503132216
#define JINNETADDRESS_H_201503132216
#include <stddef.h>
#ifdef JWIN
#include <winsock2.h>
#include <windows.h>
#include <In6addr.h>
#include <Ws2tcpip.h>
#else
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#endif
#include "jinformatint.h"
#include "jinmemorycheck.h"

//这两个函数，linux版比较合理，windows版老版没有，新版有但参数微微不同相当二。没办法这里自己包装一个.
extern int jinet_pton(int af, const char *src, void *dst);
extern const char *jinet_ntop(int af, const void *src, char *dst, socklen_t cnt);


//#ifdef JDEBUG
#define LOGBUFADDRLENGTH 40
extern char gDLogBufAddr[LOGBUFADDRLENGTH];
extern char gDLogBufAddr2[LOGBUFADDRLENGTH];
//#endif
typedef uint32_t JinNetRawAddr4;  //big endian
typedef uint16_t JinNetRawPort;  //big endian

class JinNetPort
{
public:
    JinNetPort() : port_(0){}
    JinNetPort(const uint16_t port): port_(port) {}
    JinNetRawPort rawPort() const;
    inline uint16_t port() const
    {   return port_;   }
    static JinNetPort fromRawPort(JinNetRawPort rawPort);
private:
    uint16_t port_;
};

class JinNetAddr
{
public:
    JinNetAddr()
        :_sin_family(AF_INET),_sin_addr6(NULL)
    {}

    JinNetAddr(const char* addr)
        :_sin_family(AF_INET),_sin_addr6(NULL)
    {
        setAddr(addr);
    }
    JinNetAddr(const JinNetAddr& other)  // copy construct
    {
        _sin_addr6 = NULL;  //不加这句初始化不完整,memcheck有警告.
        _sin_family = other._sin_family;
        if(other._sin_family == AF_INET6){
            _sin_addr6 = other._sin_addr6;
            _sin_addr6->ref ++;
        }
        else{
            _sin_addr4 = other._sin_addr4;
        }
    }
    //JinNetAddr(const char* addr){ setAddr(addr); }
    JinNetAddr& operator=(JinNetRawAddr4 nip)
    {
        _reset();  //赋值前先断掉‘浅拷贝’联系，这一点非常非常重要
        _sin_family = AF_INET;
        _sin_addr4.s_addr = nip;
        return *this;
    }
    JinNetAddr& operator=(in_addr nip4)
    {
        _reset();  //赋值前先断掉‘浅拷贝’联系，这一点非常非常重要
        _sin_family = AF_INET;
        _sin_addr4.s_addr = nip4.s_addr;
        return *this;
    }
    JinNetAddr& operator=(in6_addr nip6)
    {
        _reset();
        _sin_family = AF_INET6;
        _sin_addr6 = JNew(_in6addr);
        _sin_addr6->ref = 1;
        memcpy(&_sin_addr6->addr,&nip6,sizeof(in6_addr));
        return *this;
    }
    JinNetAddr& operator=(const JinNetAddr &other)
    {
        _reset();
        _sin_family = other._sin_family;
        if(other._sin_family == AF_INET6)
        {
            _sin_addr6 = other._sin_addr6;
            _sin_addr6->ref ++;
        }
        else
        {
            _sin_addr4 = other._sin_addr4;
        }
        return *this;
    }
    JinNetAddr& operator=(const char* addr)
    {
        setAddr(addr);
        return *this;
    }
    ~JinNetAddr(){ _reset(); }

    bool operator==(const JinNetAddr& other)
    {
        if(_sin_family != other._sin_family) return false;
        if(_sin_family==AF_INET) return (_sin_addr4.s_addr==other._sin_addr4.s_addr);
        if(_sin_family==AF_INET6)
        {
            return (memcmp(&(_sin_addr6->addr),&(other._sin_addr6->addr),sizeof(in6_addr))==0);
        }
        return false;
    }
    inline const in6_addr& addr6() const{ return _sin_addr6->addr; }
    inline const in_addr& addr4() const{ return _sin_addr4; }
    bool isValid() const{ return (_sin_addr6 != 0); }
    inline bool isV4() const {return (_sin_family==AF_INET);}
    inline bool isV6() const {return (_sin_family==AF_INET6);}

    inline JinNetRawAddr4 rawIP() const { return _sin_addr4.s_addr; }
    static JinNetAddr fromRawIP(JinNetRawAddr4 rawIP);

    bool setAddr(const char* addr);  //会导致reset
    const char *toAddr(char* buf = gDLogBufAddr2, size_t *validLen = NULL) const;
    //下面自己实现的,针对ipv4, validLen要么给NULL表示肯定够,不关心.否则会影响结果,返回实际len.
    bool setAddr4(const char* addr);
    const char *toAddr4(char* buf, size_t *validLen = NULL) const; //suggest 16


protected:
    void _reset()
    {
        if(_sin_family == AF_INET6 && _sin_addr6)
        {
            if(--_sin_addr6->ref == 0)
            {
                JDelete(_sin_addr6);
            }
        }
        _sin_family = AF_INET;
        _sin_addr6 = NULL;
    }
    JinNetAddr(JinNetRawAddr4 rawIP){
        _sin_family = AF_INET;
        _sin_addr4.s_addr = rawIP;
    }
private:
    struct _in6addr
    {
        in6_addr addr;
        int ref;
    };
    uint16_t _sin_family;
    union{
        _in6addr* _sin_addr6;
        in_addr _sin_addr4;
        uint8_t body_[4];
    };
};


#endif // JINNETADDRESS_H
