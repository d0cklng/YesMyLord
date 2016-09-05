#ifndef JINCRYPT_H_201409222238
#define JINCRYPT_H_201409222238
#include <stddef.h>
#include <stdint.h>

#include <tomcrypt.h>


/* *******************************
 *
 * 加密算法tomcrypt整合. 注现在只加入了需要的算法的编译，
 * 直接使用未加入的会有链接错误
 * 将成熟算法实现和自定义算法函数弄到一起作为基础的加密算法支持
 * 注意算法库是c实现，要extern “C”
 *
 * *******************************/

#ifdef __cplusplus
extern "C" {
#endif
// BKDR Hash Function
#define HAS_BKDRHASH
uint64_t BKDRHash64(const char *str);
unsigned int BKDRHash(const char *str);
/*void FakeHash(const char *str,unsigned char *out);*/
//==================================================================
int base32_encode(const unsigned char *in,  unsigned long len,
                        unsigned char *out, unsigned long *outlen);
int base32_decode(const unsigned char *in,  unsigned long len,
                        unsigned char *out, unsigned long *outlen);
//==================================================================
#define BNODE_INT 'i'
#define BNODE_DICT 'd'
#define BNODE_LIST 'l'
#define BNODE_BUF 'b'

// when modify any bnode, all nodes with type 'd' and 'l' are not reliable
// rawLen,rawText,num,content only reliable after bencode_decode
typedef struct _BNode {
    char rawType;	//'i','d','l','b' 0x80
    int  rawLen;    //rawLen,rawText: when modify bnode,
    const char *rawText; //these two are not reliable.

    //if type is 'i', means number itself
    //if type is 'd', means count of key=value pairs
    //if type is 'l', means count of list node
    //if type is 'b', means valid length of string
    int64_t num;

    //only valid when type is 'b'
    //valid length is 'num',no '\0' at end.
    const char *content;

    struct _BNode *sub;
    struct _BNode *next;

#ifdef __cplusplus
    char type()
    {  //'i','d','l','b'
        return this->rawType&0x7F;
    }
#endif
}BNode,*PBNode;


//TODO 大数字容易溢出.
//xx_make_xx系函数不处理rawLen,rawText
void bencode_make_child(PBNode parent, PBNode sub);
void bencode_make_brother(PBNode brother, PBNode next);
void bencode_make_link(PBNode* internal, PBNode next); //方便代码书写.

PBNode bencode_free_node(PBNode node,PBNode parent,PBNode prev);
void bencode_free_node_tree(PBNode nodeTree);
void bencode_free_node_solo(PBNode nodeSolo);  //不处理链表和层级关系,直接删node.
int	bencode_decode(const void *in,  size_t len, PBNode *root); //返回处理过的字节.

int bencode_create_m(char type,PBNode *created); //used for 'l','d'
int bencode_create_i(int64_t num,PBNode *created); //used for 'i'
int bencode_create_b(const void *in, size_t len, PBNode *created); //used for 'b'uff

//please give valid length of 'out' by 'outlen'.
//if CRYPT_BUFFER_OVERFLOW returned, 'outlen' should help.
int bencode_encode(PBNode root, char *out, size_t *outlen);

//参入bt种子decode过后的BNode树,解出hash info[]   [这个函数不该放在这里]
//BNode *	bencode_get_bt_info(BNode *pStart);



#ifdef __cplusplus
}
#endif
#endif
