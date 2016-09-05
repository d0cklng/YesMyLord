#ifndef __OMISC_H__
#define __OMISC_H__


////避免因为这几个函数而另外引入网络库.是重实现
//static unsigned short _ntohs( unsigned short netshort)
//{
//    unsigned short h,l;
//    h = netshort << 8;
//    l = netshort >> 8;
//    return h|l;
//    //下面的方法在vs2010的会出现怪异的问题,虽然结果正确,但是本来相等的_ntohs(port)==hport会在某些地方诡异的不等
//    //unsigned char* pc = (unsigned char*)&netshort;
//    //*pc = *pc+*(pc+1);
//    //*(pc+1) = *pc - *(pc+1);
//    //*pc = *pc - *(pc+1);
//    //return netshort;
//}
//static unsigned short _htons(unsigned short hostport)
//{
//    return _ntohs(hostport);
//}

static char * _inet_ntoa(in_addr in)
{
    static char addbuf[16];
    unsigned char *num = (unsigned char*)&in.s_addr;
    sprintf(addbuf,"%hhu.%hhu.%hhu.%hhu",num[0],num[1],num[2],num[3]);
    return addbuf;
}

static char * _inet_ntoa(unsigned int dwip)
{
    in_addr in;
    in.s_addr = dwip;
    return _inet_ntoa(in);
}

static unsigned int _inet_addr(const char *cp )
{
    in_addr out;
    unsigned char p1=0,p2=0,p3=0,p4=0;
    sscanf(cp,"%hhu.%hhu.%hhu.%hhu",&p1,&p2,&p3,&p4);
    //if(p4<0||p3<0||p2<0||p1<0)return 0;
    //if(p1>255||p2>255||p3>255||p4>255)return 0;
    unsigned char* num = (unsigned char*)&out.s_addr;
    num[0] = p1;
    num[1] = p2;
    num[2] = p3;
    num[3] = p4;
    return out.s_addr;
}

static unsigned int xBKDRHash(const char *str)
{
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;
    if(!str)return 0;
    const unsigned char *p = (const unsigned char*)str;
    while (*p)
    {
        hash = hash * seed + (*p++);
    }
    return (hash & 0x7FFFFFFF);
}
#endif
