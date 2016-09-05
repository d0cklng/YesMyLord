#include "jinnetaddress.h"
//#include "jinendian.h"
#include "jinassert.h"
#include "jinendian.h"

//#if defined(JWIN) && !defined(JMINGW)

//#ifdef JDEBUG
char gDLogBufAddr[LOGBUFADDRLENGTH] = {0};
char gDLogBufAddr2[LOGBUFADDRLENGTH] = {0};
//#endif

int jinet_pton(int af, const char *src, void *dst)
{  //友好地址转in_addr
#ifdef JWIN
# if (_WIN32_WINNT < 0x0600)  //not support ipv6
    if(af==AF_INET){
        JinNetRawAddr4 nip = inet_addr(src);
        if(nip == INADDR_NONE) return 0;
        ((in_addr*)dst)->s_addr = nip;
        return 1;
    }
    else return 0;
# else
    return InetPtonA(af,src,dst);  //InetPtonA 等效 inet_pton(win版本）
# endif
#else //linux
    return inet_pton(af,src,dst);
#endif

}
const char *jinet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{  //in_addr转友好地址
#ifdef JWIN
# if (_WIN32_WINNT < 0x0600)  //not support ipv6
    if(af == AF_INET){
        char* ntoaout = inet_ntoa(*(in_addr*)src);
        if(!ntoaout || cnt < (socklen_t)strlen(ntoaout))return NULL;
        strcpy(dst,ntoaout);
        return dst;
    }
    else return NULL;
# else
    return InetNtopA(af,(PVOID)src,dst,cnt);
# endif
#else //linux
    return inet_ntop(af,src,dst,cnt);
#endif
}


uint16_t JinNetPort::rawPort() const
{
    return toBE(port_);
}

JinNetPort JinNetPort::fromRawPort(JinNetRawPort rawPort)
{
    JinNetPort np(fromBE(rawPort));
    return np;
}

JinNetAddr JinNetAddr::fromRawIP(JinNetRawAddr4 rawIP)
{
    JinNetAddr na(rawIP);
    return na;
}

bool JinNetAddr::setAddr(const char *addr)
{
    _reset();
    bool ret = false;
    // 判断ipv4还是v6
    const char* p = addr;
    while(*p && p-addr < 5)
    {
        if(*p == '.')
        {
            _sin_family = AF_INET;
            ret = (0 < jinet_pton(_sin_family,addr,&_sin_addr4));
            JAssert(ret == true || this->isValid()==false);
            return ret;
        }
        else if(*p == ':')
        {
            _sin_family = AF_INET6;
            _sin_addr6 = JNew(_in6addr);
            _sin_addr6->ref = 1;
            ret = (0 < jinet_pton(_sin_family,addr,&_sin_addr6->addr));
            JAssert(ret == true || this->isValid()==false);
            return ret;
        }
        ++p;
    }
    return false;

}
const char *JinNetAddr::toAddr(char* buf, size_t *validLen) const
{
    const char *rtn;
    size_t vlen = 0;
    if(isV4()){
        if(validLen){
            vlen = *validLen;
            rtn = jinet_ntop(_sin_family,&_sin_addr4,buf,(socklen_t)vlen);
            *validLen = strlen(buf);
            return rtn;
        }
        vlen = 20;  //16够.
        return jinet_ntop(_sin_family,&_sin_addr4,buf,(socklen_t)vlen);
    }
    if(isV6()){
        if(validLen){
            vlen = *validLen;
            rtn = jinet_ntop(_sin_family,&_sin_addr6->addr,buf,(socklen_t)vlen);
            *validLen = (size_t)vlen;
            return rtn;
        }
        vlen = 40;  //40够.
        return jinet_ntop(_sin_family,&_sin_addr6->addr,buf,(socklen_t)vlen);
    }
    JAssert2(false,"unexpect toAddr failed.");
    return NULL;
}

//====== Net Addr ====

bool JinNetAddr::setAddr4(const char *addr)
{
    bool valid = false;
    int idx = 0;
    uint16_t pv = 0;
    int progress = 0; //10位代表第几个数,个位代表第几步
    do
    {
        const char v = addr[idx];
        if(v==0)
        {
            if(progress>30) valid = true;
            body_[progress/10]= (uint8_t)pv;
            break;
        }
        if(v=='.')
        {
            body_[progress/10]= (uint8_t)pv;
            pv = 0;
            progress = (progress/10 + 1) * 10;
            if(progress >= 40)break;
            ++idx;
            continue;
        }
        char iv = v-'0';
        if(iv<0 || iv>=10) break;
        pv = pv*10 + iv;
        if(((++progress % 10) > 3) || pv>0xFF)
        {
            break;
        }
        ++idx;
    }while(true);

    if(!valid) *((JinNetRawAddr4*)body_) = 0u;
    return valid;
}

const char *JinNetAddr::toAddr4(char *buf, size_t *validLen) const
{
    int progress = 0;
    int idx = 0;
    for(int i=0;i<4;++i)
    {
        uint16_t pv = body_[i];
        progress = i*10;
        while(pv)
        {
            int mv = progress % 10;
            if(mv==1) buf[idx+1] = buf[idx];
            else if(mv==2){
                buf[idx+2] = buf[idx+1];
                buf[idx+1] = buf[idx];
            }
            buf[idx] = '0' + pv % 10;
            pv /= 10;
            ++ progress;
        }

        if(progress % 10 == 0)
        {
            buf[idx]='0';
            ++ progress;
        }

        idx += progress % 10;
        buf[idx++] = '.';
    }

    buf[--idx] = '\0';
    JAssert(validLen==NULL || (int)*validLen>idx);
    if(validLen)*validLen = (size_t)idx;
    return buf;
}


