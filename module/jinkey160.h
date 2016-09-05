#ifndef JINKEY160_H
#define JINKEY160_H
#include "jinformatint.h"
#include <stddef.h>
//160bit分为4bit组
#include "jinhashmap.h"

class JinKeyData;
class JinKey160
{
public:
    JinKey160();  //default is not valid
    JinKey160(const unsigned char buf[20]);  //固定是20字节.
    JinKey160(const JinKey160 &o);
    ~JinKey160();
    //异常重复设置的话,debug则断言崩溃;release则无效
    void setOnlyOnce(const unsigned char *buf);
    JinKey160& operator=(const JinKey160& o);
    bool operator == ( const JinKey160& other ) const;
    bool isEqual(const JinKey160* pkey) const;
    //bool operator == ( unsigned char *buf );

    //以4bit为单位,计算前多少个单位是相同的
    uint8_t sameStartAsBit4(const JinKey160& other);
    uint8_t samePrefixBit(const JinKey160& other) const;
    uint8_t getBit4At(uint8_t idx) const;
    const char* toString() const;
    const unsigned char* rawBuf() const;
    bool isValid() const;

    JinKey160 x(const JinKey160& o);  //xor  xor是保留字?
    //分别对比与myself_的距离,left离myself_更近则返回<0 其他照推.
    int compareDistanceToMine(const JinKey160& left, const JinKey160& right);
    static JinKey160 random(const unsigned char *buf=NULL, int bytes=0);

    static uint32_t JinKey160HashFunc(const JinKey160& key);
private:
    JinKeyData *data_;
};

#endif // JINKEY160_H
