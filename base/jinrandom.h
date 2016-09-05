#ifndef JINRANDOM_H
#define JINRANDOM_H
#include "jinformatint.h"

/* *********************************************************
 *
 * 随机数，实现基本照抄RakNet
 * MT是`Mersenne Twister'随机数生产算法 MT19937
 *
 * *********************************************************/
// Same thing as above functions, but not global
class JinRandom
{
public:
    JinRandom();
    ~JinRandom();
    void SeedMT( uint32_t seed );
    uint32_t ReloadMT( void );
    uint32_t RandomMT( void );
    float FrandomMT( void );
    void FillBufferMT( void *buffer, uint32_t bytes );

    static uint32_t rnd();
protected:
    uint32_t state[ 624 + 1 ];
    uint32_t *next;
    int left;
};


#endif // JINRANDOM_H
