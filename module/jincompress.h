#ifndef JINCOMPRESS_H
#define JINCOMPRESS_H
#include "jinbuffer.h"
//不可重入.
typedef enum _JCompressType
{
    kNoCompress = 0,
    kTypeQuickLz = 'q',

    __below_is_not_implement,
    kTypeZIP = 'z',
    kTypeLZMA7z = '7',
    kTypeRAR = 'r',//不开源 基本不可能了.
}JCompressType;
enum JCompressLevel
{
    //更小的会被直接传入压缩算法（如果算法支持的话,quicklz在编译的时候就固化了）
    __larger_is_easy_define,
    kCpLevelLowest = 7700,
    kCpLevelMedium = 7701,
    kCpLevelHighest = 7702,
    kCpLevelRecommend = 7703,
};

//目前主要打算使用quicklz，预期使用流模式，需要保证压缩数据和解压顺序相同。
//我的理解是为了保持高压缩比，保持的字典需要正确的顺序使用，不能少解压丢包缺包。
//目前的结构在中间raknet传输应该可以做到。 raknet不能取消已经投递的发送包。
class JinBuffer;
class JinCompress
{
public:
    JinCompress(JCompressType type);
    ~JinCompress();

    JCompressType type(){ return type_; }
    //这套压缩解压是有延续性的，必须按顺序压缩后按顺序解压
    JinRoBuf compress(const void *buf, size_t bufLen);
    JinRoBuf decompress(const void *buf, size_t bufLen);

    bool isStreamMode();
    bool isSupportVariantLevel();

protected:
    bool compress_qlz(const void *buf, size_t bufLen);
    bool decompress_qlz(const void *buf, size_t bufLen);

private:
    JCompressType type_;
    JinBuffer *buffer_;
    void* compress_state_;
    void* decompress_state_;
};

#endif // JINCOMPRESS_H
