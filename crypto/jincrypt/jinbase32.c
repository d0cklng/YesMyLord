/* ************************************
 *
 * 和base64很像的base32实现。base32的效果等同于hex转字符串，
 * 所以这样的实现比较尴尬，tomcrypt没有
 *
 * ************************************/
#include "tomcrypt.h"

static const char *codes32 =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567+/";

/**
   base32 Encode a buffer (NUL terminated)
   @param in      The input buffer to encode
   @param inlen   The length of the input buffer
   @param out     [out] The destination of the base32 encoded data
   @param outlen  [in/out] The max size and resulting size
   @return CRYPT_OK if successful
*/
int base32_encode(const unsigned char *in,  unsigned long inlen,
                        unsigned char *out, unsigned long *outlen)
{
   unsigned long i, len2, leven;
   unsigned char *p;

   LTC_ARGCHK(in     != NULL);
   LTC_ARGCHK(out    != NULL);
   LTC_ARGCHK(outlen != NULL);

   /* valid output size ? */
   len2 = 8 * ((inlen + 4) / 5);	//一组5个字节,编码后每组8个字节.
   if (*outlen < len2 + 1) {
      *outlen = len2 + 1;
      return CRYPT_BUFFER_OVERFLOW;
   }
   p = out;
   leven = 5*(inlen / 5);	//取整组的部分先处理.
   for (i = 0; i < leven; i += 5) {		//base64 3字节=>4字节 base32 5字节=>8字节.
       *p++ = codes32[(in[0] >> 3) & 0x1F];
       *p++ = codes32[(((in[0] & 7) << 2) + (in[1] >> 6)) & 0x1F];
       *p++ = codes32[(in[1] >> 1) & 0x1F];
       *p++ = codes32[(((in[1] & 1) << 4) + (in[2] >> 4)) & 0x1F];
       *p++ = codes32[(((in[2] & 15) << 1) + (in[3] >> 7)) & 0x1F];
       *p++ = codes32[(in[3] >> 2) & 0x1F];
       *p++ = codes32[(((in[3] & 3) << 3) + (in[4] >> 5)) & 0x1F];
       *p++ = codes32[in[4] & 0x1F];
       in += 5;
   }
   /* Pad it if necessary...  */
   if (i < inlen) {
       unsigned a = in[0];
       unsigned b = (i+1 < inlen) ? in[1] : 0;
       unsigned c = (i+2 < inlen) ? in[2] : 0;
       unsigned d = (i+3 < inlen) ? in[3] : 0;

       *p++ = codes32[(a >> 3) & 0x3f];
       *p++ = codes32[(((a & 7) << 2) + (b >> 6)) & 0x1F];
       *p++ = (i+1 < inlen) ? codes32[(b >> 1) & 0x1F] : '=';
       *p++ = (i+1 < inlen) ? codes32[(((b & 1) << 4) + (c >> 4)) & 0x1F] : '=';
       *p++ = (i+2 < inlen) ? codes32[(((c & 15) << 1) + (d >> 7)) & 0x1F] : '=';
       *p++ = (i+3 < inlen) ? codes32[(d >> 2) & 0x1F] : '=';
       *p++ = (i+3 < inlen) ? codes32[((d & 3) << 3) & 0x1F] : '=';
       *p++ = '=';
   }

   /* append a NULL byte */
   *p = '\0';

   /* return ok */
   *outlen = (unsigned long)(p - out);
   return CRYPT_OK;
}


static const unsigned char map32[256] = {
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255,  32, 255, 255, 255,  33,
255, 255, 26,  27,  28,  29,  30,  31,  255,  255, 255, 255,
255, 0, 255, 255, 255,   0,   1,   2,   3,   4,   5,   6,
7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255 };

/**
   base32 decode a block of memory
   @param in       The base32 data to decode
   @param inlen    The length of the base32 data
   @param out      [out] The destination of the binary decoded data
   @param outlen   [in/out] The max size and resulting size of the decoded data
   @return CRYPT_OK if successful
*/
int base32_decode(const unsigned char *in,  unsigned long inlen,
                        unsigned char *out, unsigned long *outlen)
{
    int x,y,z;
    unsigned i;
    unsigned long d=0;

    LTC_ARGCHK(in     != NULL);
    LTC_ARGCHK(out    != NULL);
    LTC_ARGCHK(outlen != NULL);
    if(inlen%8!=0)		return CRYPT_INVALID_PACKET;
    if(*outlen<inlen/8*5)return CRYPT_BUFFER_OVERFLOW;  //actually over demand


    for(i=0;i<inlen;i+=8)
    {
        x=map32[in[i]];
        y=map32[in[i+1]];
        out[d++] = (x<<3|y>>2)&0xFF;
        z=map32[in[i+2]];
        x=map32[in[i+3]];
        out[d++]=(y<<6|z<<1|x>>4)&0xFF;
        y=map32[in[i+4]];
        out[d++]=(x<<4|y>>1)&0xFF;
        z=map32[in[i+5]];
        x=map32[in[i+6]];
        out[d++]=(y<<7|z<<2|x>>3)&0xFF;
        y=map32[in[i+7]];
        out[d++]=(x<<5|y)&0xFF;
    }
    while(!out[d-1])d--;
    *outlen = d;

    return CRYPT_OK;
}
