#ifndef JINPUBLICKEY_H
#define JINPUBLICKEY_H

#include "jinbuffer.h"
#define LTC_MECC
#include "jincrypt.h"

#pragma pack(push,4) //证书就是这个格式.数字以小端保存.注意fromLE
struct CertDetailInfo  //证书信息
{
    uint64_t signTime;  //签名时间
    uint64_t deadLine;  //到期时间
    char shortInfo[32]; //简单信息
    //unsigned char hash[20];  //信息体哈希
    uint32_t certLen;
    unsigned char cert[4];  //签名笔迹
};
#pragma pack(pop)

class JinPublicKey
{
public:
    JinPublicKey();
    JinPublicKey(const unsigned char *in, unsigned long inlen);
    ~JinPublicKey();
    /*  初级基础函数 */
    //key的两个来源,一个是创造,一个是导入.
    bool generate(int bitKey=256); //if success, isValid==true,isPrivateKey==true
    bool importKey(const unsigned char *in, unsigned long inlen);
    JinBuffer exportKey(bool expPrivate = false);
    JinBuffer encrypt(const unsigned char *in, unsigned long inlen); //用公钥加密.
    JinBuffer decrypt(const unsigned char *in, unsigned long inlen); //用私钥解密.
    JinBuffer sign(const unsigned char *in,  unsigned long inlen);   //用私钥签.
    bool verify(const unsigned char *cert,  unsigned long certlen,
                const unsigned char *hash, unsigned long hashlen); //用公钥验.
    inline bool isValid() const{ return type_>KT_LARGE_IS_VALID; }
    inline bool isPrivateKey() const{ return type_ == KT_PRIVATE; }

public:
    /*  封装函数: 本地与文件系统进行存取  */
    //把文件保存到指定dir, secret\desc将写入,文件名将自定并返回.失败返回NULL
    const char* saveToFile(const char* dir, JinBuffer secret, const char* desc);
    //从dir读取,扩展名优先找name.pri.如果读取的是pub,secret无效.返回的是desc
    const char* loadFromFile(const char* dir, const char* name, JinBuffer secret);
    //使用secret解读pri,pub. 提取的描述写入desc, desc空间应大于90. fullPath是文件全路径,utf8
    bool loadKey(JinBuffer secret, JinBuffer &desc, const char *fullPath);
    //描述性内容+已导出的key内容作为keyToSave,用secret保存到fullPath
    bool saveKey(JinBuffer keyToSave, JinBuffer secret, const char* desc, const char *fullPath);

    /*  封装函数: 签名与验证  */
    //把另一个key的公钥,加上必要信息进行哈希, 然后对哈希签名 输出证书.
    JinBuffer signToCert(JinBuffer keyToCert, uint32_t validHour, const char* desc);
    //用当前key来验证是否是当前key所签发.当前key可以是公钥. keyWhichIsSigned被签原始信息,cert是证书.
    bool verifyCert(JinBuffer keyWhichIsSigned, JinBuffer cert);  //how to load desc?

protected:
    const static unsigned kFirstDescPart = 96;
    const static unsigned kInternalBufLen = 256;
    char innerBuffer[kFirstDescPart];
private:
    bool initReady();
    bool prngReady();
#ifdef JDEBUG
    void debugLogKey();
#endif
private:
    enum KeyType{KT_INVALID, KT_INITED,
                 KT_LARGE_IS_VALID,
                 KT_PUBLIC, KT_PRIVATE} type_;
    ecc_key* key_;
    prng_state* prngState_;
    int hashIdx_;
    int prngIdx_;
};

#endif // JINPUBLICKEY_H
