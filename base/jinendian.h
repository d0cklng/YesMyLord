#ifndef JINENDIAN_H
#define JINENDIAN_H

#include "jinformatint.h"

/* ******************************
 *
 * 大小端，做一点转换。目前只是几个普通函数。
 * 依赖：
 * 特性：
 *
 *
 *
 * ******************************/

//没拜读过linux内核代码，下面这段据说是linux内核代码判断大小端的宏定义.
static union { char c[4]; unsigned long l; } endian_test = { { 'l', '?', '?', 'b' } };
#define ENDIANNESS ((char)endian_test.l)

#ifndef JINSWAP
#define JINSWAP
#define SWAPINT(x,y) (x)^=(y);(y)^=(x);(x)^=(y)
#endif

template<typename number>
number swapEndian(number n)
{
    int sz = sizeof(number);
    int walk = 0;
    for(;sz>1;sz-=2)  //swap
    {
        SWAPINT(((char*)(&n))[walk],((char*)(&n))[walk+sz-1]);
        ++walk;
    }
    return n;
}

template<typename number>
number _swapifle(number n)
{
    if (ENDIANNESS=='b')
        return n;
    else
        return swapEndian<number>(n);

}

template<typename number>
number _swapifbe(number n)
{
    if (ENDIANNESS=='l')
        return n;
    else
        return swapEndian<number>(n);
}

#define fromLE _swapifbe
#define fromBE _swapifle
#define toLE _swapifbe
#define toBE _swapifle

uint16_t jhtons(uint16_t host);
uint16_t jntohs(uint16_t net);
uint32_t jhtonl(uint32_t host);
uint32_t jntohl(uint32_t net);

//template<typename number>
//number toBE(number n)
//{
//#if ENDIANNESS=='b'
//    return n;
//#else
//    return swapEndian<number>(n);
//#endif
//}

//template<typename number>
//number toLE(number n)
//{
//#if ENDIANNESS=='l'
//    return n;
//#else
//    return swapEndian<number>(n);
//#endif
//}







class JinEndian
{
public:
    JinEndian();
};

#endif // JINENDIAN_H
