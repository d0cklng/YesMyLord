#include "jinendian.h"

uint16_t jhtons(uint16_t host)
{
    return toBE(host);
}

uint16_t jntohs(uint16_t net)
{
    return fromBE(net);
}

uint32_t jhtonl(uint32_t host)
{
    return toBE(host);
}

uint32_t jntohl(uint32_t net)
{
    return fromBE(net);
}

JinEndian::JinEndian()
{
}
