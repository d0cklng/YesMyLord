/* ************************************
 *
 * 几种有意思的哈希函数
 * BKDRHash将字符串转成数字，可以用于字符串索引
 * FakeHash将字符串哈希，可以假装是SHA1来使用
 *
 * ************************************/
#include "tomcrypt.h"
#include <stdint.h>

/// BKDR Hash 由于在Brian Kernighan与Dennis Ritchie的《The C Programming Language》
/// 一书被展示而得 名，是一种简单快捷的hash算法，
/// 也是Java目前采用的字符串的Hash算法（累乘因子为31）.
uint64_t BKDRHash64(const char *str)
{
    uint64_t hash = 0;
    if(!str)return 0;
    while ( *str )
    {
        hash = hash * 131 + *str;
        ++str;
    }
    return hash;
    //return (hash & 0x7FFFFFFF);
}

// BKDR Hash Function
unsigned int BKDRHash(const char *str)
{
    const unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;

    while (*str)
    {
        hash = hash * seed + (*str++);
    }

    return (hash & 0x7FFFFFFF);
}

/*
void FakeHash(const char *str,unsigned char *out)
{
    //memset(out,0,20);
    zeromem(out,20);
    if(!str || str[0]==NULL)
    {
        return;
    }
    int len = strlen(str);
    char *instr = new char[len+1];  //TODO  to modify
    strcpy(instr,str);
    for(int idx=0;idx<5;idx++)
    {
        *(unsigned int*)&out[idx*4] += BKDRHash((const unsigned char*)instr)+0x12345678;
        for(int i=0;i<len;i++)
        {
            instr[i]+=131;
        }
    }
    delete[] instr;
}*/
