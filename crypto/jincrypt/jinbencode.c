#include <stdint.h>
#include "jincrypt.h"
#define MAX_NUMBER_STRING_LENGTH  24
#define BNODETYPE(p) (p->rawType&0x7F)
#ifndef JINSWAP
#define JINSWAP
#define SWAPINT(x,y) (x)^=(y);(y)^=(x);(x)^=(y)
#endif
size_t i64toa(int64_t value, char *str, int base) //base 2~16
{
    int sign = value<0?-1:1;
    value = value*sign;
    int arrow=0;
    do
    {
        str[arrow++] = "0123456789abcdef"[value%base];
        value /= base;
    }
    while(value>0);
    if(sign<0)str[arrow++]='-';
    str[arrow]=0;

    base = 0;  //base reuse.
    sign = arrow;  //sign reuse as length
    for(;arrow>1;arrow-=2)  //swap
    {
        SWAPINT(str[base],str[base+arrow-1]);
        ++base;
    }

    return (size_t)sign;
}

//only base=10. if len=0 stop when \0 or reach maximum
int64_t atoi64 (const char * str, size_t len)
{
    int64_t ret = 0;
    unsigned arrow = 0;
    char ch;
    int sign = 1;
    //0x7fff ffff ffff ffff = 9223372036854775807
    if(len==0)len = 20; //20 is the max!

    while((ch = str[arrow])!=0 && arrow < len){
        if(arrow++==0 && str[0]=='-'){
            sign=-1;
            continue;
        }
        if(ch<'0' || ch>'9')break;
        ret = ret*10+(ch-'0');
    }
    return ret*sign;
}


//======================================================================

int _bencode_create(char type,int64_t num,const char *content, PBNode *created)
{
    int ret = CRYPT_OK;
    size_t len;
    int prtlen;
    char *bufnode = NULL;
    PBNode pBNode = NULL;
    pBNode = (BNode*)XMALLOC(sizeof(BNode));
    if(pBNode == NULL){
        ret = CRYPT_MEM;
        goto create_error;
    }
    pBNode->rawType = type;
    pBNode->sub = NULL;
    pBNode->next = NULL;
    //pBNode->type = type&0x7F;  //highest bit for memory flag. means we alloc it.
    switch(BNODETYPE(pBNode))
    {
    case BNODE_BUF:
        len = num+MAX_NUMBER_STRING_LENGTH+1;
        bufnode = (char*)XMALLOC(len);
        if(bufnode == NULL)
        {
            ret = CRYPT_MEM;
            goto create_error;
        }
        pBNode->rawType = type|0x80;
        prtlen = (int)i64toa(num,bufnode,10);// sprintf(bufnode,"%lld:",num);
        bufnode[prtlen++]=':';
        memcpy(bufnode+prtlen,content,num);
        pBNode->num = num;
        pBNode->content = bufnode+prtlen;
        pBNode->rawLen = (int)num+prtlen;
        pBNode->rawText = bufnode;

        break;
    case BNODE_INT:
        len = MAX_NUMBER_STRING_LENGTH+2;
        bufnode = (char*)XMALLOC(len);
        if(bufnode == NULL)
        {
            ret = CRYPT_MEM;
            goto create_error;
        }
        pBNode->rawType = type|0x80;
        bufnode[0] = 'i';
        prtlen = (int)i64toa(num,bufnode+1,10);//sprintf(bufnode,"i%llde",num);
        bufnode[++prtlen]='e';
        pBNode->num = num;
        pBNode->content = 0;
        pBNode->rawLen = ++prtlen;
        pBNode->rawText = bufnode;

        break;
    case BNODE_DICT:
    case BNODE_LIST:
        pBNode->rawType = type;
        pBNode->num = 0;
        pBNode->content = NULL;
        pBNode->rawLen = 0;
        pBNode->rawText = 0;
        break;
    }
    *created = pBNode;
    return ret;
create_error:
    if(pBNode)XFREE(pBNode);
    return ret;
}

int bencode_create_m(char type,PBNode *created) //used for 'l','d'
{
    LTC_ARGCHK(type==BNODE_DICT || type==BNODE_LIST);
    return _bencode_create(type,0,NULL,created);
}

int bencode_create_i(int64_t num,PBNode *created) //used for 'i'
{
    return _bencode_create(BNODE_INT,num,NULL,created);
}

int bencode_create_b(const void *in, size_t len,PBNode *created) //used for 'b'
{
    return _bencode_create(BNODE_BUF,len,in,created);
}

size_t _bencode_node_length(PBNode node)
{
    size_t len = 0;
    if(!node)return 0;
    while(node)
    {
        switch(BNODETYPE(node))
        {
        case BNODE_BUF:
        case BNODE_INT:
            len += node->rawLen;
            break;
        case BNODE_DICT:
        case BNODE_LIST:
            len += _bencode_node_length(node->sub) + 2;
            break;
            //default:
        }
        node = node->next;
    }
    return len;
}

int _bencode_encode(PBNode root, char *out, int *pos)
{
    PBNode pNode = root;

    while(pNode)
    {
        switch(BNODETYPE(pNode))
        {
        case BNODE_BUF:
        case BNODE_INT:
            memcpy(out+(*pos),pNode->rawText,pNode->rawLen);
            *pos += pNode->rawLen;
            break;
        case BNODE_DICT:
            out[(*pos)++] = BNODE_DICT;
            _bencode_encode(pNode->sub,out,pos);
            out[(*pos)++] = 'e';
            break;
        case BNODE_LIST:
            out[(*pos)++] = BNODE_LIST;
            _bencode_encode(pNode->sub,out,pos);
            out[(*pos)++] = 'e';
            break;
        }
        pNode = pNode->next;
    }
    return CRYPT_OK;
}

//please give valid length of 'out' by 'outlen'.
//if CRYPT_BUFFER_OVERFLOW returned, 'outlen' should help.
int bencode_encode(PBNode root, char *out, size_t *outlen)
{
    //first calculate length
    size_t len;
    int x=0;

    PBNode pStart = root;
    if(!pStart)return CRYPT_INVALID_ARG;
    len = _bencode_node_length(pStart);
    if(*outlen < len){
        *outlen = len;
        return CRYPT_BUFFER_OVERFLOW;
    }
    *outlen = len;

    _bencode_encode(pStart,out,&x);

    return CRYPT_OK;
}

//==========================================


//TODO 做上内存泄露检查。要等JAlloc做好.
//TODO 不能针对性处理list和dict.
void bencode_make_child(PBNode parent,PBNode sub)
{
    LTC_ARGCHK(BNODETYPE(parent) == BNODE_DICT || BNODETYPE(parent) == BNODE_LIST)
    PBNode pNode = parent->sub;
    if(parent->sub){
        while(pNode->next){
            pNode = pNode->next;
        }
        pNode->next = sub;
        //parent->rawLen = 0;
        //parent->rawText = NULL;
        parent->rawLen += (int)_bencode_node_length(sub);
    }
    else{
        parent->sub = sub;
    }
}

void bencode_make_brother(PBNode brother,PBNode next)
{
    PBNode pNode = brother;
    while(pNode->next){
        pNode = pNode->next;
    }
    pNode->next = next;
}

void bencode_make_link(PBNode* internal, PBNode next)
{
    if(*internal == NULL){
        *internal = next;
    }
    else{
        (*internal)->next = next;
        (*internal) = next;
    }
}

PBNode bencode_free_node(PBNode node,PBNode parent,PBNode prev)
{
    if(prev){    prev->next = node->next;   }
    else{    parent->sub = node->next;   }

    if(node->sub)
    {
        bencode_free_node_tree(node->sub);
    }
    if(node->rawType&0x80){
        XFREE((char*)node->rawText);
    }
    XFREE(node);

    if(prev){    node = prev->next;   }
    else{    node = parent->sub;   }

    return node;
}

void bencode_free_node_tree(PBNode nodeTree)
{
    PBNode next;
    if(nodeTree==NULL)return;
    next=NULL;
    do
    {
        next = nodeTree->next;
        if(nodeTree->sub)bencode_free_node_tree((PBNode)nodeTree->sub);
        bencode_free_node_solo(nodeTree);
        nodeTree = next;
    }
    while(next);
}

void bencode_free_node_solo(PBNode nodeSolo)
{
    if(nodeSolo->rawType&0x80){
        XFREE((char*)nodeSolo->rawText);
    }
    XFREE(nodeSolo);
}


//[in/out] len输入最大解析长度,输出解析的长度.
//TODO 用不完整的torrent文件测试强壮性.
int	_bencode_decode(BNode *pUplevel,const char *in, size_t *plen,
                    int64_t *unit,  PBNode *subRoot, size_t *recursionLevel)
{
    int pairsCheck,pairs;//在pUplevel type=='d'的时候pair必须为偶数.
    size_t x,length,len = *plen;
    BNode *pPre,*pFirst,*pBNode;
    //char buf[36];  使用ato64后，免去了.
    int ret = CRYPT_OK;
    x=0;
    pairs=0;
    pairsCheck=(pUplevel&&(pUplevel->rawType&0x7F)=='d')?1:0;
    pPre = NULL;
    pFirst = NULL;
    pBNode = NULL;
    *subRoot = NULL;

    if(++*recursionLevel > 10)
    {
        ret = CRYPT_INVALID_ROUNDS;
        goto parse_error;
    }
    for(/*x=0*/;x<len;/*x++*/)
    {
        pBNode = (BNode*)XMALLOC(sizeof(BNode));
        if(pBNode==NULL){
            ret = CRYPT_MEM;
            goto parse_error;
        }
        memset(pBNode,0,sizeof(BNode));
        switch(in[x])
        {
        case 'd':
        case 'l':
            if(len < x+2)
            {
                ret = CRYPT_INVALID_PACKET;
                goto parse_error;
            }
            length = len-x-2; //脱去d和e里面的内容长度.
            pBNode->rawType = in[x];
            pBNode->rawText = &in[x];  //rawText包括d......e或l.....e的整个内容.
            pBNode->rawLen = 2;
            pBNode->num = 0;
            pBNode->content = NULL;
            ret = _bencode_decode(pBNode,&in[x+1],&length,&pBNode->num,&pBNode->sub,recursionLevel);
            if(CRYPT_OK != ret)  goto parse_error;
            pBNode->rawLen += (int)length;
            x+=length;//+(pUplevel?2:0);
            if(in[x+1]!='e'){
                ret = CRYPT_INVALID_PACKET;
                goto parse_error;
            }
            x+=2;

            if(pUplevel==NULL && pairs == 0)
            {  //底层d解析完毕则解析结束.
                pFirst = pBNode;
                goto parse_compelete;
            }
            //pBNode->rawLen++;
            if(pPre)pPre->next = pBNode;
            pPre = pBNode; //记录当前为上一个记录,下一次读取的时候填其他信息.
            break;
        case 'i':
            pBNode->rawType = 'i';
            pBNode->rawText = &in[x++];
            pBNode->rawLen = 1;
            pBNode->num = 0;
            pBNode->content = pBNode->rawText+1;
            pBNode->sub = NULL;
            do  //保证不会越len界.
            {
                if(x>=len){
                    ret = CRYPT_INVALID_PACKET;
                    goto parse_error;
                }
                pBNode->rawLen++;
            }
            while(in[x++]!='e');
            pBNode->num = atoi64(pBNode->rawText+1,pBNode->rawLen-2);
            if(pPre)pPre->next = pBNode;
            pPre = pBNode; //记录当前为上一个记录,下一次读取的时候填其他信息.
            break;
        case 'e':
            XFREE(pBNode);
            goto parse_compelete;
            break;
        default:
            pBNode->rawType = 'b';
            pBNode->rawText = &in[x];
            pBNode->rawLen = 0;
            pBNode->sub = NULL;
            while(in[x++]!=':')
            {
                pBNode->rawLen++;
                if((in[x-1]!='0' && x+1>=len) ||
                        pBNode->rawLen>MAX_NUMBER_STRING_LENGTH ||
                        in[x-1]<'0' || in[x-1]>'9'){
                    ret = CRYPT_INVALID_PACKET;
                    goto parse_error;
                };
            }
            length = (size_t)atoi64(pBNode->rawText,pBNode->rawLen);
            if(x+length>len){ //if(length<=0 || x+length>len){
                ret = CRYPT_INVALID_PACKET;
                goto parse_error;
            }
            pBNode->num = length;
            pBNode->content = &in[x];
            pBNode->rawLen += (int)length;
            x+=length;
            if(pPre)pPre->next = pBNode;
            pPre = pBNode;
        }
        if(pFirst==NULL)pFirst = pBNode;
        //if(pairsCheck)
        ++pairs;
    }
parse_compelete:
    if(pairsCheck)
    {
        if(pairs%2!=0){
            ret = CRYPT_INVALID_PACKET;
            pBNode=NULL;  //避免重复删除.
            goto parse_error;
        }
        else{
            pairs/=2;
        }
    }
    if(unit)*unit = pairs;
    *plen = x;
    *subRoot = pFirst;
    --*recursionLevel;
    return CRYPT_OK;
parse_error:
    //...
    if(pBNode){
        XFREE(pBNode);
        pBNode=NULL;
    }
    if(pFirst)bencode_free_node_tree(pFirst);
    return ret;
    //...
}

int	bencode_decode(const void *in,  size_t len, PBNode *root)
{
    size_t recursionLevel = 0;
    return _bencode_decode(NULL,(const char*)in,&len,NULL,root,&recursionLevel);
}

