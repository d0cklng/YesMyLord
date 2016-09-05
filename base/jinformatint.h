#ifndef JINFORMATINT_H
#define JINFORMATINT_H
#include <stdio.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

# if __WORDSIZE == 64
#define PRISIZE "lu"
#else
#define PRISIZE "u"
#endif

#endif // JINPRINTFORMAT_H
