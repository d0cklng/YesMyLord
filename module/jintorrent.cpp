#include "jintorrent.h"
#include "jincrypt.h"
//#include "jinalloc.h"
#include "jinassert.h"
#include "jinmemorycheck.h"

//JinTorrent::JinTorrent()
//{
//    //bValid_为true就要初始化infoNode.
//}

JinTorrent::~JinTorrent()
{
    if(root_){
        bencode_free_node_tree(root_);  //函数里面，对于需要和不需要释放的node内容，会通过type区分.
    }
}

//TODO 警告：JinTorrent中使用了node-》type，这个node如果是创造出来的 type可能有0x80前缀.

JinTorrent::JinTorrent(const char *torrentPath)
    : bValid_(false)
    , root_(NULL)
    , nextWalk_(NULL)
    , infoDict_(NULL)
    , filesList_(NULL)
    , fileCount_(-1)
{
    FILE *fp = fopen(torrentPath,"rb");
    if(fp)
    {
        fseek(fp,0,SEEK_END);
        long fileSize = ftell(fp);
        if(fileSize>0 && fileSize<20*1024*1024){
            fseek(fp,0,SEEK_SET);
            unsigned char* fileBuf4Read = (unsigned char*)JMalloc(fileSize);
            if(fileBuf4Read){
                content_.eat(fileBuf4Read,fileSize);
                size_t freaded = fread(fileBuf4Read,1,fileSize,fp);
                if(freaded == (size_t)fileSize){
                    if(CRYPT_OK == bencode_decode(fileBuf4Read,freaded,&root_)){
                        infoDict_ = rootDictWalk("info");
                        bValid_ = (infoDict_!=NULL);
                        if(bValid_){
                            fileListValidWalk();
                        }
                    }
                }
            }
        }
        fclose(fp);
    }
}

JinTorrent::JinTorrent(const void *torrentContent, size_t len)
    : bValid_(false)
    , root_(NULL)
    , nextWalk_(NULL)
{
    if(CRYPT_OK == bencode_decode(torrentContent,len,&root_)){
        infoDict_ = rootDictWalk("info");
        bValid_ = (infoDict_!=NULL);
        if(bValid_){
            fileListValidWalk();
        }
    }
}

JRoBuf JinTorrent::getInfo(JTorrentInfoType type)
{
    JRoBuf robuf;
    PBNode node;
    switch(type)
    {
    case kTorrentAnnounce:
        node = rootDictWalk("announce");
        break;
    case kTorrentComment:
        node = rootDictWalk("comment");
        break;
    case kTorrentCreateBy:
        node = rootDictWalk("created by");
        break;
    case kTorrentCreationData:
        node = rootDictWalk("creation date");
        break;
    case kTorrentFolderName:
        node = nodeDictWalk("name",infoDict_->sub);
        break;
    case kTorrentPieceLength:
        //node = rootDictWalk("piece length");
        node = nodeDictWalk("piece length",infoDict_->sub);
        break;
    case kTorrentPieces:
        //node = rootDictWalk("pieces");
        node = nodeDictWalk("pieces",infoDict_->sub);
        break;
    default:
        break;
    }
    if(node){
        robuf.buf = node->content;
        robuf.len = node->num;
    }
    else{
        robuf.buf = NULL;
        robuf.len = 0;
    }
    return robuf;
}

int JinTorrent::getFilesCount(bool *isMulti)
{
    if(fileCount_>=0){
        if(filesList_) { if(isMulti)*isMulti = true; }
        else { if(isMulti)*isMulti = false; }
        return fileCount_;
    }
    //else, auto detect it.
    if(infoDict_==NULL || !bValid_)return (fileCount_=0);
    filesList_ = nodeDictWalk("files",infoDict_->sub);
    if(filesList_){
        if(isMulti)*isMulti = true;
        if(filesList_->type()!=BNODE_LIST)return 0;
        fileCount_=(int)filesList_->num;
        return fileCount_;
    }
    else{
        if(isMulti)*isMulti = false;
        fileCount_=1;
        return fileCount_;
    }
}

uint64_t JinTorrent::getFileSize(int idxOfFile)
{
    bool isMulti;
    PBNode walk = NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return NILSIZE;
        if(!filesList_)   return NILSIZE;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return NILSIZE;
        walk = nodeDictWalk("length",detailDict->sub);
        if(walk && walk->type() == BNODE_INT){
            return walk->num;
        }
    }
    else
    {
        if(idxOfFile!=0)  return NILSIZE;
        walk = nodeDictWalk("length",infoDict_->sub);
        if(walk && walk->type() == BNODE_INT){
            return walk->num;
        }
    }
    return NILSIZE;
}

JinRoBuf JinTorrent::getFolderName()
{
    return getInfo(kTorrentFolderName);
}

JinRoBuf JinTorrent::getFileName(int idxOfFile)
{
    bool isMulti;
    JinRoBuf rtnbuf;
    PBNode walk = NULL;
    rtnbuf.buf=0;rtnbuf.len=0;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return rtnbuf;
        if(!filesList_)   return rtnbuf;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return rtnbuf;
        walk = nodeDictWalk("path",detailDict->sub);
        if(walk && walk->type() == BNODE_LIST){
            walk = walk->sub;
            if(!walk)return rtnbuf;
            while(walk->next){ walk = walk->next;}
            rtnbuf.buf = walk->content;
            rtnbuf.len = walk->num;
            return rtnbuf;
        }
    }
    else
    {
        if(idxOfFile!=0)  return rtnbuf;
        walk = nodeDictWalk("name",infoDict_->sub);
        if(walk && walk->type() == BNODE_BUF){
            rtnbuf.buf = walk->content;
            rtnbuf.len = walk->num;
            return rtnbuf;
        }
    }
    return rtnbuf;
}

char *JinTorrent::getFileName(int idxOfFile, char *buf, size_t bufLen)
{

    bool isMulti;
    PBNode walk = NULL;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return NULL;
        if(!filesList_)   return NULL;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return NULL;
        walk = nodeDictWalk("path",detailDict->sub);
        if(walk && walk->type() == BNODE_LIST){
            walk = walk->sub;
            if(!walk)return NULL;
            while(walk->next){ walk = walk->next;}
            if(bufLen<(size_t)walk->num+1)return NULL;
            memcpy(buf,walk->content,walk->num);
            buf[walk->num]='\0';
            return buf;
        }
    }
    else
    {
        if(idxOfFile!=0)  return NULL;
        walk = nodeDictWalk("name",infoDict_->sub);
        if(walk && walk->type() == BNODE_BUF){
            if(bufLen<(size_t)walk->num+1)return NULL;
            memcpy(buf,walk->content,walk->num);
            buf[walk->num]='\0';
            return buf;
        }
    }
    return NULL;
}

char *JinTorrent::getSubPath(int idxOfFile, char *buf, size_t bufLen, char slash)
{
    bool isMulti;
    PBNode walk = NULL;
    int bufWritePoint = 0;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return NULL;
        if(!filesList_)   return NULL;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return NULL;
        walk = nodeDictWalk("path",detailDict->sub);
        if(walk && walk->type() == BNODE_LIST){
            walk = walk->sub;
            if(!walk)return NULL;
            while(walk && walk->next)
            {
                if(walk->num<0 || bufLen<(size_t)walk->num+1)return NULL;
                memcpy(buf+bufWritePoint,walk->content,walk->num);
                bufWritePoint += (int)walk->num;
                bufLen -= (walk->num+1);
                if(walk->next->next){
                    buf[bufWritePoint++]=slash;
                }
                else{
                    break;
                }
                walk = walk->next;
            }
            buf[bufWritePoint++]='\0';
            return buf;
        }
    }
    else
    {
        if(idxOfFile!=0)  return NULL;
        buf[bufWritePoint++]='\0';
        return buf;
    }
    return NULL;
}

char *JinTorrent::getAllSubPath(int idxOfFile, char *buf, size_t bufLen, char slash)
{
    bool isMulti;
    int bufWritePoint = 0;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        JinRoBuf ro = this->getFolderName();
        if(ro.len==0 || bufLen<ro.len+1)return NULL;
        memcpy(buf+bufWritePoint,ro.buf,ro.len);
        bufWritePoint += (int)ro.len;
        buf[bufWritePoint++]=slash;
    }
    char* subpath = getSubPath(idxOfFile,buf+bufWritePoint,bufLen-bufWritePoint,slash);
    if(!subpath)return NULL;
    return buf;

}

char *JinTorrent::getSubPathAndName(int idxOfFile, char *buf, size_t bufLen, char slash)
{  //和getSubPath很相近，多了name.代码不同处用diff标记.
    bool isMulti;
    PBNode walk = NULL;
    int bufWritePoint = 0;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return NULL;
        if(!filesList_)   return NULL;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return NULL;
        walk = nodeDictWalk("path",detailDict->sub);
        if(walk && walk->type() == BNODE_LIST){
            walk = walk->sub;
            if(!walk)return NULL;
            while(walk)  //diff
            {
                if(walk->num<0 || bufLen<(size_t)walk->num+1)return NULL;
                memcpy(buf+bufWritePoint,walk->content,walk->num);
                bufWritePoint += (int)walk->num;
                bufLen -= (walk->num+1);
                if(walk->next){  //diff
                    buf[bufWritePoint++]=slash;
                }
                walk = walk->next;
            }
            buf[bufWritePoint++]='\0';
            return buf;
        }
    }
    else
    {
        return getFileName(idxOfFile,buf,bufLen);
    }
    return NULL;
}

char *JinTorrent::getAllSubPathAndName(int idxOfFile, char *buf, size_t bufLen, char slash)
{
    bool isMulti;
    int bufWritePoint = 0;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        JinRoBuf ro = this->getFolderName();
        if(ro.len==0 || bufLen<ro.len+1)return NULL;
        memcpy(buf+bufWritePoint,ro.buf,ro.len);
        bufWritePoint += (int)ro.len;
        buf[bufWritePoint++]=slash;

    }
    char* subpath = getSubPathAndName(idxOfFile,buf+bufWritePoint,bufLen-bufWritePoint,slash);
    if(!subpath)return NULL;
    return buf;
}

char *JinTorrent::getFileProperty(int idxOfFile, const char *property, char *buf, size_t bufLen)
{
    bool isMulti;
    PBNode walk = NULL;
    if(bufLen<=1)return NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return NULL;
        if(!filesList_)   return NULL;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return NULL;
        walk = nodeDictWalk(property,detailDict->sub);
    }
    else
    {
        if(idxOfFile!=0)  return NULL;
        walk = nodeDictWalk(property,infoDict_->sub);

    }

    if(walk && walk->type() == BNODE_BUF){
        if(bufLen<(size_t)walk->num+1)return NULL;
        memcpy(buf,walk->content,walk->num);
        buf[walk->num]='\0';
        return buf;
    }
    return NULL;
}

const unsigned char* JinTorrent::getInfoHash()
{
//  老代码
//  pbcell = bencode_get_bt_info(pbcell);
//	if(!pbcell)goto faultReturn;
//	sha1_init(&md);
//	if(CRYPT_OK!=sha1_process(&md,pbcell->rawText,pbcell->rawLen))goto faultReturn;
//	if(CRYPT_OK!=sha1_done(&md,infoHash))goto faultReturn;
    if(infoDict_==NULL)return NULL;
    if(infoHash_.isNull()){
        infoHash_ = JinBuffer(20);
        if(infoHash_.isNull())return NULL;
        hash_state md;
        sha1_init(&md);
        sha1_process(&md,(const unsigned char*)infoDict_->rawText,infoDict_->rawLen);
        sha1_done(&md,infoHash_.buff());
    }
    return infoHash_.cbuff();
}

int JinTorrent::getRealIndex(int idxOfFile)
{
    if(idxOfFile==0) return 0;
    if(!bHasBcPadding_)return idxOfFile;

    bool isMulti;
    PBNode walk = NULL;
    getFilesCount(&isMulti);
    if(isMulti){
        if(idxOfFile<0 || idxOfFile>=this->fileCount_)  return -1;
        if(!filesList_)   return -1;
        PBNode detailDict = fileListWalk(idxOfFile);
        if(!detailDict) return -1;
        walk = nodeDictWalk("realIndex",detailDict->sub);
    }
    else return 0;

    if(walk && walk->type() == BNODE_INT){
        return (int)walk->num;
    }
    return -1;
}
//#ifndef MIN
//   #define MIN(x, y) ( ((x)<(y))?(x):(y) )
//#endif

_BNode *JinTorrent::rootDictWalk(const char *key)
{
    //size_t keylen = strlen(key);
    if(!root_ || root_->type()!=BNODE_DICT || root_->sub==NULL)return NULL;
    PBNode walk = nextWalk_?nextWalk_:root_->sub;
    do{
        if(walk->next==NULL)return NULL;
        if(walk->type() != BNODE_BUF)return NULL;
        //if(memcmp(walk->content,key,MIN(walk->num,keylen))==0){
        //if(strnicmp(walk->content,key,walk->num)==0){
        if(keyMatch(key,walk->content,walk->num)){
            nextWalk_ = walk->next->next;
            if(nextWalk_==NULL)nextWalk_ = root_->sub;
            return walk->next;
        }
        walk = walk->next->next;
        if(walk==NULL)walk = root_->sub;  //cycle back.
    }
    while(walk!=nextWalk_);
    return NULL;
}

_BNode *JinTorrent::nodeDictWalk(const char *key, _BNode  *start, bool utf8Compat)
{
    if(!start)return NULL;
    do{
        if(start->next==NULL)return NULL;
        if(start->type() != BNODE_BUF)return NULL;
        //if(memcmp(start->content,key,MIN(start->num,keylen))==0){
        //if(strnicmp(start->content,key,start->num)==0){
        if(keyMatch(key,start->content,start->num))
        {
            if(utf8Compat)
            {
                char ukey[64];
                memset(ukey,0,64);
                strncpy(ukey,key,64-10);
                strcat(ukey,".UTF-8");
                PBNode compat = nodeDictWalk(ukey,start->next->next,false);
                if(compat) return compat;
            }
            return start->next;
        }
        start = start->next->next;
    }
    while(start!=NULL);
    return NULL;
}

_BNode *JinTorrent::fileListWalk(int index)   //TODO 取filename似乎有问题...
{
    bool isMulti;
    getFilesCount(&isMulti);
    if(index>=fileCount_ || index<0)return NULL;
    if(filesList_==NULL && index>0)return NULL;
    PBNode walk = isMulti? filesList_->sub : infoDict_;
    if(isMulti){
        while(index>0 && walk!=NULL)
        {
            //if(walk->type()!=BNODE_LIST || walk->rawLen<=0) return NULL;  //不预期会这样.
            //walk是dict。 walk完要记得使walk是dict的sub
            //if(index==0){
            //    walk = walk->sub;
            //    break;
            //}
            //else{
                walk = walk->next;
                --index;
            //}
        }
    }
    if(walk==NULL || walk->type()!=BNODE_DICT || walk->rawLen<=0) return NULL;  //不预期会这样.
    return walk;
}

bool JinTorrent::keyMatch(const char *search, const char *tkey, size_t tkeyLen)
{
    char left,right;
    size_t len = strlen(search);
    if(len!=tkeyLen)return false;
    for(size_t i=0;i<len;i++)
    {
        if(search[i]>='A' && search[i]<='Z')left = search[i]-'A'+'a';
        else left = search[i];
        if(tkey[i]>='A' && tkey[i]<='Z')right = tkey[i]-'A'+'a';
        else right = tkey[i];
        if(left!=right)return false;
    }
    return true;
}

//BitComet padding 简化了piece hash计算.
// 但是要特意去掉那些多余的无用文件.
void JinTorrent::fileListValidWalk()
{
    if(infoDict_==NULL)return;
    filesList_ = nodeDictWalk("files",infoDict_->sub);
    if(!filesList_ || filesList_->type()!=BNODE_LIST) return;

    PBNode walk = filesList_->sub;
    PBNode preWalk = NULL;
    while(walk){
        if(walk->type()==BNODE_DICT){
            PBNode pathList=0,pathNode=0;
            pathList = nodeDictWalk("path",walk->sub);
            if(pathList && pathList->type()==BNODE_LIST){
                pathNode = pathList->sub;
                if(pathNode && pathNode->type()==BNODE_BUF){
                    if(pathNode->num<20 || strncmp("_____padding_file_",pathNode->content,18)!=0){
                        preWalk = walk;
                        walk = walk->next;
                        continue;
                    }
                }
            }
        }
        //以上任意不合格，都会到这里，这里移除这个节点.
        walk = bencode_free_node(walk,filesList_,preWalk);
        //if(preWalk){    preWalk->next = walk->next;   }
        //else{    filesList_->sub = walk->next;   }
        //bencode_free_node(walk);
        //if(preWalk){    walk = preWalk->next;   }
        //else{    walk = filesList_->sub;   }
        filesList_->num -= 1;

    }
}






