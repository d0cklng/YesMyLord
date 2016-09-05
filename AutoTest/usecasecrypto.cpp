#include "usecasecrypto.h"
#include "jinstring.h"
#include "jinfiledir.h"
#include "jinrandom.h"
#include "jinsetting.h"
#include "jinlogger.h"
#include "jinpublickey.h"
#define LTC_RC4
#define TFM_DESC
#include "jincrypt.h"
CHAIN_USE_CASE(Crypto);

UseCaseCrypto::UseCaseCrypto()
{
    errmsg_[0] = 0;
}

UseCaseCrypto::~UseCaseCrypto()
{

}

bool UseCaseCrypto::run()
{
    int ret = CRYPT_OK;
    ret = testRSA();
    CHECK_ASSERT(ret == EXIT_SUCCESS,"RSA test failed.");
    ret = testECC();
    CHECK_ASSERT(ret == EXIT_SUCCESS,"ECC test failed.");
    ret = testRealUse();
    CHECK_ASSERT(ret == EXIT_SUCCESS,"real use test failed.");
    ret = testJinPublicKey();
    CHECK_ASSERT(ret == EXIT_SUCCESS,"test jin public key failed.");
    ret = testPublicKeyMore();
    CHECK_ASSERT(ret == EXIT_SUCCESS,"test jin public key failed.");

    return true;
}



int UseCaseCrypto::testRSA(void)
{
    int err, hash_idx, prng_idx, res;
    unsigned long l1, l2;
    unsigned char pt[16], pt2[16], out[1024];
    rsa_key key;
    /* register prng/hash */
    if (register_prng(&rc4_desc) == -1)
    {
        JPLog("Error registering sprng");
        return __LINE__;
    }
    /* register a math library (in this case TomsFastMath)*/
    ltc_mp = ltm_desc;
    if (register_hash(&sha1_desc) == -1)
    {
        JPLog("Error registering sha1");
        return __LINE__;
    }
    hash_idx = find_hash("sha1");
    prng_idx = find_prng("rc4");


    prng_state pst;
    uint32_t rd = JinRandom::rnd();
    prng_descriptor[prng_idx].start(&pst);
    prng_descriptor[prng_idx].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),&pst);
    prng_descriptor[prng_idx].ready(&pst);
    /* make an RSA-1024 key */
    if ((err = rsa_make_key(&pst, /* PRNG state */
                            prng_idx, /* PRNG idx */
                            1024/8, /* 1024-bit key */
                            65537, /* we like e=65537 */
                            &key) /* where to store the key */ )
            != CRYPT_OK)
    {
        JPLog("rsa_make_key %s", error_to_string(err));
        return __LINE__;
    }

    /* fill in pt[] with a key we want to send ... */
    for(int i=0;i<16;i+=4)
    {
        *(uint32_t*)&pt[i] = JinRandom::rnd();
    }

    l1 = sizeof(out);
    if ((err = rsa_encrypt_key(pt, /* data we wish to encrypt */
                               16, /* data is 16 bytes long */
                               out, /* where to store ciphertext */
                               &l1, /* length of ciphertext */
                               (const unsigned char*)"TestApp", /* our lparam for this program */
                               7, /* lparam is 7 bytes long */
                               &pst, /* PRNG state */
                               prng_idx, /* prng idx */
                               hash_idx, /* hash idx */
                               &key) /* our RSA key */
         ) != CRYPT_OK)
    {
        JPLog("rsa_encrypt_key %s", error_to_string(err));
        rsa_free(&key);
        return __LINE__;
    }
    /* now let’s decrypt the encrypted key */
    l2 = sizeof(pt2);
    if ((err = rsa_decrypt_key(out, /* encrypted data */
                               l1, /* length of ciphertext */
                               pt2, /* where to put plaintext */
                               &l2, /* plaintext length */
                               (const unsigned char*)"TestApp", /* lparam for this program */
                               7, /* lparam is 7 bytes long */
                               hash_idx, /* hash idx */
                               &res, /* validity of data */
                               &key) /* our RSA key */
         ) != CRYPT_OK)
    {
        JPLog("rsa_decrypt_key %s", error_to_string(err));
        rsa_free(&key);
        return __LINE__;
    } /* if all went well pt == pt2, l2 == 16, res == 1 */

    err = memcmp(pt,pt2,16);
    rsa_free(&key);
    JPLog("rsa_decrypt_key %s l2=%lu res=%d",err?"mismatch":"match",l2,res);
    if(err!=0 || l2!=16 || res!=1)
    {
        return __LINE__;
    }

    return EXIT_SUCCESS;
}

int UseCaseCrypto::testECC()
{
    int err, hash_idx, prng_idx;
    unsigned long l1, l2;
    unsigned char pt[16], pt2[16], out[1024];
    ecc_key key;
    /* register prng/hash */
    if (register_prng(&rc4_desc) == -1)
    {
        JPLog("Error registering sprng");
        return __LINE__;
    }
    /* register a math library (in this case TomsFastMath)*/
    ltc_mp = ltm_desc;
    if (register_hash(&md5_desc) == -1)
    {
        JPLog("Error registering md5");
        return __LINE__;
    }
    hash_idx = find_hash("md5");
    prng_idx = find_prng("rc4");


    prng_state pst;
    uint32_t rd = JinRandom::rnd();
    prng_descriptor[prng_idx].start(&pst);
    prng_descriptor[prng_idx].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),&pst);
    prng_descriptor[prng_idx].ready(&pst);
    /* make an ECC-256 key */
    if ((err = ecc_make_key(&pst, /* PRNG state */
                            prng_idx, /* PRNG idx */
                            256/8, /* 256-bit key */
                            &key) /* where to store the key */ )
            != CRYPT_OK)
    {
        JPLog("ecc_make_key %s", error_to_string(err));
        return __LINE__;
    }

    /* fill in pt[] with a key we want to send ... */
    for(int i=0;i<16;i+=4)
    {
        *(uint32_t*)&pt[i] = JinRandom::rnd();
    }

    l1 = sizeof(out);
    if ((err = ecc_encrypt_key(pt, /* data we wish to encrypt */
                               16, /* data is 16 bytes long */
                               out, /* where to store ciphertext */
                               &l1, /* length of ciphertext */
                               &pst, /* PRNG state */
                               prng_idx, /* prng idx */
                               hash_idx, /* hash idx */
                               &key) /* our ECC key */
         ) != CRYPT_OK)
    {
        JPLog("ecc_encrypt_key %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    }
    /* now let’s decrypt the encrypted key */
    l2 = sizeof(pt2);
    if ((err = ecc_decrypt_key(out, /* encrypted data */
                               l1, /* length of ciphertext */
                               pt2, /* where to put plaintext */
                               &l2, /* plaintext length */
                               &key) /* our ECC key */
         ) != CRYPT_OK)
    {
        JPLog("ecc_decrypt_key %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    } /* if all went well pt == pt2, l2 == 16, res == 1 */

    ecc_free(&key);

    err = memcmp(pt,pt2,16);
    JPLog("ecc_decrypt_key %s l2=%lu",err?"mismatch":"match",l2);
    if(err!=0 || l2!=16)
    {
        return __LINE__;
    }

    return EXIT_SUCCESS;
}

int UseCaseCrypto::testRealUse()
{
    int err = CRYPT_OK;
    err = ecc_test();
    CHECK_ASSERT(err == CRYPT_OK,"ecc_test failed.");

    //①生成密钥 ②保存公钥私钥 ③公钥加密私钥解--常规加解密
    //④私钥加密公钥解--签名
    ecc_key key;
    int hash_idx, prng_idx, res;
    unsigned long l1, l2, l3, lku, lki;
    unsigned char pt[16], pt2[16], sh[16], encout[512];
    unsigned char expub[512],expri[512];
    unsigned char ehash[512]; //,hashV[512]
    /* register prng/hash */
    if (register_prng(&rc4_desc) == -1)
    {
        JPLog("Error registering sprng");
        return __LINE__;
    }
    /* register a math library (in this case TomsFastMath)*/
    ltc_mp = ltm_desc;
    if (register_hash(&sha1_desc) == -1)
    {
        JPLog("Error registering sha1");
        return __LINE__;
    }
    hash_idx = find_hash("sha1");
    prng_idx = find_prng("rc4");

    prng_state pst;
    uint32_t rd = 0;
    prng_descriptor[prng_idx].start(&pst);
    rd = JinRandom::rnd();
    prng_descriptor[prng_idx].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),&pst);
    rd = JinRandom::rnd();
    prng_descriptor[prng_idx].add_entropy((const unsigned char*)&rd,sizeof(uint32_t),&pst);
    prng_descriptor[prng_idx].ready(&pst);

    /* make an ECC-256 key */
    if ((err = ecc_make_key(&pst, /* PRNG state */
                            prng_idx, /* PRNG idx */
                            256/8, /* 256-bit key */
                            &key) /* where to store the key */ )
            != CRYPT_OK)
    {
        JPLog("ecc_make_key %s", error_to_string(err));
        return __LINE__;
    }

    memcpy(pt,".jin fr.ee net.",16);
    l1 = sizeof(encout);
    if ((err = ecc_encrypt_key(pt, /* data we wish to encrypt */
                               16, /* data is 16 bytes long */
                               encout, /* where to store ciphertext */
                               &l1, /* length of ciphertext */
                               &pst, /* PRNG state */
                               prng_idx, /* prng idx */
                               hash_idx, /* hash idx */
                               &key) /* our ECC key */
         ) != CRYPT_OK)
    {
        JPLog("ecc_encrypt_key %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    }


    lku = sizeof(expub);
    err = ecc_export(expub,&lku,PK_PUBLIC,&key);
    if(err != CRYPT_OK)
    {
        ecc_free(&key);
        return __LINE__;
    }

    lki = sizeof(expri);
    err = ecc_export(expri,&lki,PK_PRIVATE,&key);
    if(err != CRYPT_OK)
    {
        ecc_free(&key);
        return __LINE__;
    }

    ecc_free(&key);

    err = ecc_import(expri,lki,&key);  //私钥.
    if(err != CRYPT_OK)
    {
        return __LINE__;
    }

    /* now let’s decrypt the encrypted key */
    l2 = sizeof(pt2);
    if ((err = ecc_decrypt_key(encout, /* encrypted data */
                               l1, /* length of ciphertext */
                               pt2, /* where to put plaintext */
                               &l2, /* plaintext length */
                               &key) /* our ECC key */
         ) != CRYPT_OK)
    {
        JPLog("ecc_decrypt_key %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    }
    err = memcmp(pt,pt2,16);
    JPLog("ecc_decrypt_key %s l2=%lu",err?"mismatch":"match",l2);
    if(err!=0 || l2!=16)
    {
        ecc_free(&key);
        return __LINE__;
    }

    memcpy(sh,"..jinfree.net..",16);
    l3 = sizeof(ehash);
    if ((err = ecc_sign_hash(sh,16,ehash,&l3,&pst,prng_idx,&key)
         ) != CRYPT_OK)
    {
        JPLog("ecc_sign_hash %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    }

    ecc_free(&key);

    err = ecc_import(expub,lku,&key);  //公钥.
    if(err != CRYPT_OK)
    {
        return __LINE__;
    }

    if ((err = ecc_verify_hash(ehash,l3,sh,16,&res,&key)
         ) != CRYPT_OK)
    {
        JPLog("ecc_verify_hash %s", error_to_string(err));
        ecc_free(&key);
        return __LINE__;
    }
    if(err!=0 || res != 1)
    {
        ecc_free(&key);
        return __LINE__;
    }
    ecc_free(&key);
    return EXIT_SUCCESS;
}

int UseCaseCrypto::testJinPublicKey()
{
    JinBuffer exppub,exppri, encout,decout,signout;
    JinPublicKey myKey;
    bool ret = myKey.generate();
    JAssert(ret);
    exppub = myKey.exportKey();
    exppri = myKey.exportKey(true);
    JAssert(exppub.length()>0);
    JAssert(exppri.length()>0);
    const unsigned char* plainFakeHash = (const unsigned char*)"TrU0qOgcpIepvPaZ257i";
    JinPublicKey impPubKey,impPriKey;
    ret = impPubKey.importKey(exppub.cbuff(),exppub.length());
    JAssert(ret);
    ret = impPriKey.importKey(exppri.cbuff(),exppri.length());
    JAssert(ret);
    //导入的公钥加密,原私钥解密.
    encout = impPubKey.encrypt(plainFakeHash,20);
    JAssert(encout.length()>0);
    decout = impPriKey.decrypt(encout.cbuff(),encout.length());
    JAssert(decout.length()>0);
    ret = (0==memcmp(plainFakeHash,decout.cstr(),20));
    JAssert(ret);

    signout = impPriKey.sign(plainFakeHash,20);
    JAssert(signout.length()>0);
    ret = impPubKey.verify(signout.cbuff(),signout.length(),plainFakeHash,20);
    JAssert(ret);

    return EXIT_SUCCESS;
}

int UseCaseCrypto::testPublicKeyMore()
{
    JinPublicKey pk;
    bool ret = pk.generate();
    JAssert(ret);
    JinFileDir fdir;
    fdir.setWith(kDirTemp);
    ret = fdir.mk("jinzeyu.cn");
    //JAssert(ret == true);
    JinDirRtn dirrtn = fdir.cd("jinzeyu.cn");
    JAssert(dirrtn == kRtnSuccess);
    JinBuffer secret("jinzeyu.cn");
    const char* fnSave = pk.saveToFile(fdir.fullPath(),secret,"this is desc");
    JAssert(fnSave);
    JinPublicKey pkLoad;
    const char* descLoad = pkLoad.loadFromFile(fdir.fullPath(),fnSave,secret);
    JAssert(descLoad && 0== strcmp(descLoad,"this is desc"));
    JinPublicKey pkLoadPub;
    JinBuffer descByLoad;
    char nameToLoad[64];
    sprintf(nameToLoad,"%s.pub",fnSave);
    ret = pkLoadPub.loadKey(secret,descByLoad,fdir.fullPath(nameToLoad));
    JAssert(ret == true);
    JAssert(0== strcmp(descByLoad.cstr(),"this is desc"));

    JinPublicKey tobeSign;
    ret = tobeSign.generate();
    JAssert(ret == true);
    JinBuffer cert = pk.signToCert(tobeSign.exportKey(),10,"this is also desc.");
    JAssert(cert.length() > 0);
    ret = pkLoadPub.verifyCert(tobeSign.exportKey(),cert);
    JAssert(ret == true);

    return EXIT_SUCCESS;
}
