#define LTC_MECC
#define LTC_RC4
#define TFM_DESC
#include "jincrypt.h"
#include "jinpublickey.h"
#include "jincrypt.h"
#include "jinlogger.h"
#include "jinmemorycheck.h"
#include "jinrandom.h"
#include "jinassert.h"
#include "jinstring.h"
#include "jinfile.h"
#include "jinfiledir.h"
#include "jinrandom.h"
#include "jindatatime.h"
#include "jinendian.h"


JinPublicKey::JinPublicKey()
    : type_(KT_INVALID)
    , key_(NULL)
    , prngState_(NULL)
    , hashIdx_(-1)
    , prngIdx_(-1)
{
}

JinPublicKey::JinPublicKey(const unsigned char *in, unsigned long inlen)
    : type_(KT_INVALID)
    , key_(NULL)
    , prngState_(NULL)
    , hashIdx_(-1)
    , prngIdx_(-1)
{
    importKey(in,inlen);
}

JinPublicKey::~JinPublicKey()
{
    if(key_)
    {
        ecc_free(key_);
        JFree(key_);
        key_ = NULL;
    }
    if(prngState_)
    {
        JFree(prngState_);
        prngState_ = NULL;
    }
}

bool JinPublicKey::generate(int bitKey)
{
    if(!initReady())return false;
    if(!prngReady())return false;
    if(type_ != KT_INITED)
    {
        JELog("error type=%d bitKey=%d",type_,bitKey);
        return false;
    }
    JAssert(key_ == NULL);
    key_ = (ecc_key*)JMalloc(sizeof(ecc_key));
    if(key_ == NULL) return false;
    memset(key_,0,sizeof(ecc_key));

    int err = CRYPT_OK;
    /* make an ECC-256 key */
    if ((err = ecc_make_key(prngState_, /* PRNG state */
                            prngIdx_, /* PRNG idx */
                            bitKey/8, /* 256-bit key */
                            key_) /* where to store the key */ )
            != CRYPT_OK)
    {
        JELog("ecc_make_key %d:%s", err,error_to_string(err));
        JFree(key_);
        key_ = NULL;
        return false;
    }
    JAssert(key_->type == PK_PRIVATE);
    type_ = KT_PRIVATE;
#ifdef JDEBUG  //log out private key.
    debugLogKey();
#endif
    return true;
}

bool JinPublicKey::importKey(const unsigned char *in, unsigned long inlen)
{
    if(!initReady())return false;
    if(!prngReady())return false;
    if(type_ != KT_INITED)
    {
        JELog("error type=%d in=%p inlen=%ld",type_,in,inlen);
        return false;
    }
    JAssert(key_ == NULL);
    key_ = (ecc_key*)JMalloc(sizeof(ecc_key));
    if(key_ == NULL) return false;
    memset(key_,0,sizeof(ecc_key));

    /* make an ECC-256 key */
    int err = ecc_import(in,inlen,key_);
    if (err != CRYPT_OK)
    {
        JELog("ecc_import %d:%s", err,error_to_string(err));
        JFree(key_);
        key_ = NULL;
        return false;
    }
    if(key_->type == PK_PRIVATE) type_ = KT_PRIVATE;
    else type_ = KT_PUBLIC;
#ifdef JDEBUG  //log out private key.
    debugLogKey();
#endif
    return true;
}

JinBuffer JinPublicKey::exportKey(bool expPrivate)
{
    JinBuffer jb;
    if(type_<KT_LARGE_IS_VALID) return jb;
    unsigned char* expout = (unsigned char*)JMalloc(128);
    unsigned long expoutlen = 128;
    int err = ecc_export(expout,&expoutlen,expPrivate?PK_PRIVATE:PK_PUBLIC,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_export %s %s",expPrivate?"PK_PRIVATE":"PK_PUBLIC", error_to_string(err));
        JFree(expout);
    }
    else
    {
        jb.eat(expout,expoutlen);
    }
    return jb;
}

JinBuffer JinPublicKey::encrypt(const unsigned char *in, unsigned long inlen)
{
    JinBuffer jb;
    if(type_<KT_LARGE_IS_VALID || inlen>20 || !initReady() || !prngReady()) return jb;
    unsigned char* encout = (unsigned char*)JMalloc(128);
    if(encout==NULL) return jb;
    unsigned long encoutlen = 128;
    int err = ecc_encrypt_key(in,inlen,encout,&encoutlen,prngState_,prngIdx_,hashIdx_,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_encrypt_key %s", error_to_string(err));
        JFree(encout);
    }
    else
    {
        jb.eat(encout,encoutlen);
    }
    return jb;

}

JinBuffer JinPublicKey::decrypt(const unsigned char *in, unsigned long inlen)
{
    JinBuffer jb;
    if(type_<KT_PRIVATE) return jb;
    unsigned char* decout = (unsigned char*)JMalloc(32);
    if(decout==NULL) return jb;
    unsigned long decoutlen = 32;
    int err = ecc_decrypt_key(in,inlen,decout,&decoutlen,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_decrypt_key %s", error_to_string(err));
        JFree(decout);
    }
    else
    {
        jb.eat(decout,decoutlen);
    }
    return jb;

}

JinBuffer JinPublicKey::sign(const unsigned char *in, unsigned long inlen)
{
    JinBuffer jb;
    if(type_<KT_PRIVATE || !initReady() || !prngReady()) return jb;
    unsigned char* signout = (unsigned char*)JMalloc(128);
    if(signout==NULL) return jb;
    unsigned long signoutlen = 128;
    int err = ecc_sign_hash(in,inlen,signout,&signoutlen,prngState_,prngIdx_,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_sign_hash %s", error_to_string(err));
        JFree(signout);
    }
    else
    {
        jb.eat(signout,signoutlen);
    }
    return jb;

}

bool JinPublicKey::verify(const unsigned char *cert, unsigned long certlen,
                          const unsigned char *hash, unsigned long hashlen)
{
    if(type_<KT_PUBLIC || !initReady()) return false;
    int res = 0;
    int err = ecc_verify_hash(cert,certlen,hash,hashlen,&res,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_verify_hash %s", error_to_string(err));
        return false;
    }
    else if(res != 1)
    {
        JELog("ecc_verify_hash res=%d", res);
        return false;
    }
    return true;
}

const char *JinPublicKey::saveToFile(const char *dir, JinBuffer secret, const char *desc)
{
    if(secret.length() == 0) return NULL;
    if(!isValid())return NULL;
    JinBuffer exppub = exportKey();
    JinBuffer exppri;
    if(isPrivateKey())
    {
        exppri = exportKey(true);
    }
    unsigned char keyFileNameBase[18];
    memset(keyFileNameBase,0x88,18);
    hash_state hs;
    md5_init(&hs);
    md5_process(&hs,exppub.cbuff(),(unsigned long)exppub.length());
    md5_done(&hs,keyFileNameBase);
    unsigned long out = kFirstDescPart;
    base64_encode(keyFileNameBase,15,(unsigned char*)innerBuffer,&out);
    if(innerBuffer[0]>='a') innerBuffer[0] -= ('a'-'A');
    //if(keyFileName[out-1]=='=') keyFileName[out-1] = 'Q';
    innerBuffer[out] = 0;
    for(int i=0;i<(int)out;i++)
    {
        if(innerBuffer[i]=='+') innerBuffer[i] = 'J';
        if(innerBuffer[i]=='=') innerBuffer[i] = 'L';
    }

    const static unsigned long kPartReuseSp = 30;
    JAssert(out<kPartReuseSp && kFirstDescPart>kPartReuseSp*2+10);
    JinFileDir filedir(dir);
    strcpy(innerBuffer+kPartReuseSp,innerBuffer);
    strcat(innerBuffer+kPartReuseSp,".pub");

    JinBuffer pubSecret("jinzeyu.cn");
    if(!saveKey(exppub, pubSecret, desc, filedir.fullPath(innerBuffer+kPartReuseSp)))
    {   return NULL;   }
    if(isPrivateKey())
    {
        strcpy(innerBuffer+kPartReuseSp,innerBuffer);
        strcat(innerBuffer+kPartReuseSp,".pri");
        if(!saveKey(exppri, secret, desc, filedir.fullPath(innerBuffer+kPartReuseSp)))
        {   return NULL;  }
    }
    return innerBuffer;
}

const char *JinPublicKey::loadFromFile(const char *dir, const char *name, JinBuffer secret)
{
    //优先找pri, 然后才用pub
    size_t namelen = strlen(name);
    JinBuffer namebuf((int32_t)namelen+5);
    strcpy((char*)namebuf.buff(),name);
    JinFileDir filedir(dir);
    JinBuffer desc;

    strcpy((char*)namebuf.buff()+namelen,".pri");
    if(loadKey(secret, desc, filedir.fullPath(namebuf.cstr()) ))
    {
        memcpy(innerBuffer,desc.buff(),desc.length());
        innerBuffer[desc.length()-1] = 0;
        return innerBuffer;
    }


    JinBuffer pubSecret("jinzeyu.cn");
    strcpy((char*)namebuf.buff()+namelen,".pub");
    filedir.fullPath(namebuf.cstr());
    if(loadKey(pubSecret, desc, filedir.fullPath(namebuf.cstr()) ))
    {
        memcpy(innerBuffer,desc.buff(),desc.length());
        innerBuffer[desc.length()-1] = 0;
        return innerBuffer;
    }
    return NULL;
}

bool JinPublicKey::saveKey(JinBuffer keyToSave, JinBuffer secret, const char *desc, const char *fullPath)
{
    JAssert(kFirstDescPart%8==0);
    //先写desc,不超过90字节,然后从96开始写export key, 算出凑足整8字节需要的pad,写入[95]
    //用secret md5, 前后8字节做key 轮流加密整个plain.
    unsigned char plainToBeSave[kInternalBufLen];
    unsigned char secretToSave[kInternalBufLen];
    //memset(plainToBeSave,0,kInternalBufLen);
    for(unsigned i=0;i<kFirstDescPart;i+=4)
    {
        *(uint32_t*)&plainToBeSave[i] = JinRandom::rnd();
    }
    size_t descLen = strlen(desc);
    JAssert(descLen <= kFirstDescPart-6);
    if(descLen > kFirstDescPart - 6) return false;
    strcpy((char*)plainToBeSave,desc);
    uint32_t tailPad = 8 - keyToSave.length() % 8;
    if(tailPad == 8) tailPad = 0;
    plainToBeSave[kFirstDescPart-1] = tailPad;
    uint32_t plainLength = kFirstDescPart + (int)keyToSave.length() + tailPad;
    JAssert(plainLength % 8 == 0);
    JAssert(plainLength < kInternalBufLen);
    memcpy(plainToBeSave+kFirstDescPart,keyToSave.cbuff(),keyToSave.length());
    unsigned char secretKey[16];
    hash_state md;
    md5_init(&md);
    md5_process(&md,secret.cbuff(),(unsigned long)secret.length());
    md5_done(&md,secretKey);
    symmetric_key des[2];
    des_setup(&secretKey[0],8,0,&des[0]);
    des_setup(&secretKey[8],8,0,&des[1]);
    for(unsigned i=0;i<plainLength;i+=8)
    {
        des_ecb_encrypt(plainToBeSave+i,secretToSave+i,&des[i/8%2]);
    }
    JinFile f(fullPath);
    if(0==f.open(kReadWrite,kCreateAlways))
    {
        uint32_t writed = 0;
        if(f.write((const char*)secretToSave,(uint32_t)plainLength,writed)
                && writed == (uint32_t)plainLength)
        {
            return true;
        }
    }
    return false;
}

bool JinPublicKey::loadKey(JinBuffer secret, JinBuffer &desc, const char *fullPath)
{
    JinFile f(fullPath);
    if(0!=f.open(kReadOnly,kOpenExisting))
        return false;
    unsigned char secretByLoad[kInternalBufLen];
    unsigned char plainByLoad[kInternalBufLen];
    uint32_t sz = (uint32_t)f.size();
    if(sz >= kInternalBufLen || sz < kFirstDescPart + 64) return false;
    uint32_t readed = 0;
    if(!f.read((char*)secretByLoad,sz,readed) || readed != sz)
        return false;
    unsigned char secretKey[16];
    hash_state md;
    md5_init(&md);
    md5_process(&md,secret.cbuff(),(unsigned long)secret.length());
    md5_done(&md,secretKey);
    symmetric_key des[2];
    des_setup(&secretKey[0],8,0,&des[0]);
    des_setup(&secretKey[8],8,0,&des[1]);
    for(uint32_t i=0;i<sz;i+=8)
    {
        des_ecb_decrypt(secretByLoad+i,plainByLoad+i,&des[i/8%2]);
    }
    int tailPad = plainByLoad[kFirstDescPart-1];
    if(tailPad < 0 || tailPad >=8) return false;
    //strncpy(desc,(char*)plainByLoad,kFirstDescPart);
    desc = (char*)plainByLoad;

    return importKey(plainByLoad+kFirstDescPart, sz-kFirstDescPart-tailPad);
}

bool JinPublicKey::initReady()
{
    if(type_ >= KT_INITED) return true;
    /* register prng/hash */
    if (register_prng(&rc4_desc) == -1)
    {
        JELog("Error registering sprng");
        return false;
    }
    /* register a math library (in this case TomsFastMath)*/
    ltc_mp = ltm_desc;
    if (register_hash(&sha1_desc) == -1)
    {
        JELog("Error registering sha1");
        return false;
    }
    hashIdx_ = find_hash("sha1");
    prngIdx_ = find_prng("rc4");
    JAssert(hashIdx_!=-1 && prngIdx_!=-1);
    type_ = KT_INITED;
    return true;
}

bool JinPublicKey::prngReady()
{
    if(prngState_ != NULL) return true;
    if(type_ < KT_INITED) return false;
    prngState_ = (prng_state*)JMalloc(sizeof(prng_state));
    if(prngState_ == NULL) return false;

    uint32_t rd = 0;
    prng_descriptor[prngIdx_].start(prngState_);
    rd = JinRandom::rnd();
    prng_descriptor[prngIdx_].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),prngState_);
    rd = JinRandom::rnd();
    prng_descriptor[prngIdx_].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),prngState_);
    prng_descriptor[prngIdx_].ready(prngState_);

    return true;
}

#ifdef JDEBUG
void JinPublicKey::debugLogKey()
{
    int pktype;
    if(type_ == KT_PRIVATE) pktype = PK_PRIVATE;
    else if(type_ == KT_PUBLIC) pktype = PK_PUBLIC;
    else
    {
        JDLog("key invalid");
        return;
    }
    unsigned char expri[512];
    unsigned long lki = sizeof(expri);
    int err = ecc_export(expri,&lki,pktype,key_);
    if(err != CRYPT_OK)
    {
        JELog("ecc_export %d:%s", err,error_to_string(err));
        return;
    }
    JinString expkey((char*)expri,lki);
    JDLog("%skey = \n%s",(pktype==PK_PUBLIC)?"pub":"pri",
          expkey.toReadableHex().c_str());
}
#endif

#pragma pack(push,4)
struct CertBodyInfo  //被签信息体结构
{
    uint64_t signTime;  //签名时间
    uint64_t deadLine;  //到期时间
    char shortInfo[32]; //简单信息
    char msgToBeSign[4];  //待签信息体. 公钥
};
//struct CertDetailInfo  //证书信息
//{
//    uint64_t signTime;  //签名时间
//    uint64_t deadLine;  //到期时间
//    char shortInfo[32]; //简单信息
//    //unsigned char hash[20];  //信息体哈希
//    uint32_t certLen;
//    unsigned char cert[4];  //签名笔迹
//};
#pragma pack(pop)

//最终格式: 有效期至, certhash
JinBuffer JinPublicKey::signToCert(JinBuffer keyToCert, uint32_t validHour, const char *desc)
{
    JAssert(strlen(desc) < 32);
    JAssert(keyToCert.length() > 0);
    JinBuffer rtnBuffer;
    if(strlen(desc) >= 32 || keyToCert.length()==0 || !isPrivateKey() )
        return rtnBuffer;
    unsigned long bodyBufLen = (unsigned long)(sizeof(CertBodyInfo)-4+keyToCert.length());
    unsigned char *bodyBuf = (unsigned char*)JMalloc(bodyBufLen);
    if(!bodyBuf) return rtnBuffer;
    CertBodyInfo* body = (CertBodyInfo*)bodyBuf;
    JinDateTime dt;
    body->signTime = toLE((uint64_t) dt.epoch() );
    JDLog("key sign time:[%s] desc:%s",dt.toString(),desc);
    dt.timeMoveHour(validHour);
    body->deadLine = toLE((uint64_t) dt.epoch() );
    JDLog("key end time:[%s]",dt.toString());
    memset(body->shortInfo,0,32);
    strncpy(body->shortInfo,desc,32);
    body->shortInfo[32-1]=0;
    memcpy(body->msgToBeSign,keyToCert.cbuff(),keyToCert.length());

    unsigned char bodyhash[20];
    hash_state md;
    sha1_init(&md);
    sha1_process(&md,bodyBuf,bodyBufLen);
    sha1_done(&md,bodyhash);

    JinBuffer signBuff = this->sign(bodyhash,20);
    if(signBuff.length() == 0)
    {
        JFree(bodyBuf);
        return rtnBuffer;
    }

    bodyBufLen = (unsigned long)(sizeof(CertDetailInfo)-4+signBuff.length());
    char* certBody = (char*)JMalloc(bodyBufLen);
    if(!certBody)
    {
        JFree(bodyBuf);
        return rtnBuffer;
    }
    CertDetailInfo* detail = (CertDetailInfo*)certBody;
    detail->signTime = body->signTime;
    detail->deadLine = body->deadLine;
    memcpy(detail->shortInfo,body->shortInfo,32);
    JFree(bodyBuf);
    detail->certLen = toLE( (uint32_t)signBuff.length() );
    memcpy(detail->cert,signBuff.cbuff(),signBuff.length());
    rtnBuffer.eat(certBody,bodyBufLen);
    return rtnBuffer;
}

//验证方法: 把 有效期至 + 待验证公钥 进行sha1, 输入sha1结果和certhash验证
//注: 自己此时应该是服务器公钥,签名那个钥匙的公钥. 验证是否自己私钥签的.
bool JinPublicKey::verifyCert(JinBuffer keyWhichIsSigned, JinBuffer cert)
{  //keyWhichIsSigned 来自被签的那个公钥.
    if(cert.length() < sizeof(CertDetailInfo)) return false;
    CertDetailInfo* detail = (CertDetailInfo*)cert.cbuff();
    size_t certLen = fromLE( detail->certLen );
    if(sizeof(CertDetailInfo)+certLen-4 != cert.length()) return false;

    JinDateTime nowTime;
    if((uint64_t)nowTime.epoch() < fromLE( detail->signTime )) return false;
    if((uint64_t)nowTime.epoch() > fromLE( detail->deadLine )) return false;
    unsigned long bodyBufLen = (unsigned long)(sizeof(CertBodyInfo)-4+keyWhichIsSigned.length());
    unsigned char *bodyBuf = (unsigned char*)JMalloc(bodyBufLen);
    if(!bodyBuf) return false;
    CertBodyInfo* body = (CertBodyInfo*)bodyBuf;
    body->signTime = detail->signTime;
    body->deadLine = detail->deadLine;
    memcpy(body->shortInfo,detail->shortInfo,32);
    memcpy(body->msgToBeSign,keyWhichIsSigned.cbuff(),keyWhichIsSigned.length());

    unsigned char bodyhash[20];
    hash_state md;
    sha1_init(&md);
    sha1_process(&md,bodyBuf,bodyBufLen);
    sha1_done(&md,bodyhash);
    JFree(bodyBuf);

    return this->verify(detail->cert,detail->certLen,bodyhash,20);
}
