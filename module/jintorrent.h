#ifndef JINTORRENT_H
#define JINTORRENT_H

#include "jinformatint.h"
#include <stddef.h>

#include "jinbuffer.h"

/* ******************************
 *
 * 代表torrent，可用于加载解析，也可以修改重建。对构建种子做的还不够
 * 依赖: jinbuffer jinassert jincrypt(bencode*)
 * 间接依赖: jinsharedata jinalloc
 *
 * ******************************/

struct _BNode;
//typedef struct JinRoBuf {  //read only
//  size_t len;
//  const char  *buf;
//} JRoBuf,*PJRoBuf;
//use buff instead of string, I should not consider the string codec.
//All strings must be UTF-8 encoded.(from wiki)
// http://en.wikipedia.org/wiki/Torrent_file
//  ## BitComet torrent ##
// (BC)代表BitComet制作的种子中存在，可能比较特别的字段
// 所有的.utf8估计也是BC采用的，wiki上表示所有编码都应该是utf8的
//[dict根字典]
//|-{announce}={#tracker服务器#}
//|-{announce-list}=[#tracker-list更多服务器#]
//|                 |-[{#服务器1镜像1#},{#服务器1镜像2#}]
//|                 |-[{#服务器2#}]
//|                 |-[{#服务器3#}]
//|                 |- ...
//|-{comment}={#comment注释内容#}
//|-{comment.utf8}={#utf8编码的注释#}  (BC)BitComet制作的种子
//|-{created by}={#创建者}
//|-{creation date}={#1970年起的秒数}
//|-{encoding}={#采用的编码} (BC)
//|-{info}=[字典]
//|        |-{files}=[列表]  仅多文件时有
//|        |         |
//|        |         |-[dict字典]
//|        |         | |-{ed2k} =buf
//|        |         | |-{filehash} =buf20  (BC)国外种子无
//|        |         | |-{length} =int
//|        |         | |-{path}=[list] 代表一级一级的路径
//|        |         | |        |-{aaa}
//|        |         | |        |-{bbb}
//|        |         | |        |-{ccc.mp4}
//|        |         | |-{path.utf8} =list
//|        |         |
//|        |         |-[dict字典]
//|        |         | |-
//|        |         | |-
//|        |         |
//|        |         |-[dict字典]...同上
//|        |         |
//|        |-{name (.utf8)}={#单文件表示文件名，多文件表示目录}
//|        |-{piece length}={&块大小}
//|        |-{pieces}={#每块SHA1哈希汇聚在一起}
//|        |-{publisher (.utf8)}={#发布者} (BC)
//|        |-{publisher-url (.utf8)}={#发布者链接}  (BC)
//|-{nodes}=[列表] 协议扩展：这个字段是DHT peers
//|         |- ...省略
//|-{url-list}=[列表]
//|            |-{#url镜像地址} 协议扩展：http镜像

enum JTorrentInfoType{  //TODO announce-list
    kTorrentAnnounce,
    kTorrentComment,
    kTorrentCreateBy,
    kTorrentCreationData,
    kTorrentFolderName,  //only exist when isMulti=true.

    kTorrentPieceLength,
    kTorrentPieces,

    kTorrentInfoTypeCount
};


#define NILSIZE ((size_t)(-1))

class JinTorrent
{
public:
    //for parser purpose...
    JinTorrent(const char* torrentPath); //read from file
    //read from buff, caller must keep torrentContent before JinTorrent destroy.
    JinTorrent(const void* torrentContent,size_t len);
    ~JinTorrent();

    //get simple-normal info contain by torrent.
    JRoBuf getInfo(JTorrentInfoType type);
    //种子描述中说有多少个文件
    //get how many files descript by torrent.
    int getFilesCount(bool *isMulti=NULL);

    //多文件
    //{folderName}/{subPath}.../{fileName}
    //allSubPath = folderName+subPath
    //subPathAndName = subPath+name
    //allSubPathAndName = all
    //单文件，所有folder,path都是空的
    //{name}
    uint64_t getFileSize(int idxOfFile);
    JinRoBuf getFolderName();  //=getInfo(kTorrentFolderName) //only exist when isMulti=true.
    JinRoBuf getFileName(int idxOfFile);
    //以下函数会以\0结尾
    char* getFileName(int idxOfFile, char* buf, size_t bufLen);
    char* getSubPath(int idxOfFile, char* buf, size_t bufLen, char slash='/');
    char* getAllSubPath(int idxOfFile, char* buf, size_t bufLen, char slash='/');
    char* getSubPathAndName(int idxOfFile, char* buf, size_t bufLen, char slash='/');
    char* getAllSubPathAndName(int idxOfFile, char* buf, size_t bufLen, char slash='/');
    //int64_t getFilePropertyNum(int idxOfFile,const char* property); not implement.
    char* getFileProperty(int idxOfFile,const char* property, char* buf, size_t bufLen);

    //for unified use...
    inline bool isValid(){return bValid_;}
    const unsigned char* getInfoHash();

    //当存在比特彗星padding机制的时候，需要获取原torrent中的真实index用于bt查询
    inline bool isBitCometPadding(){return bHasBcPadding_;}
    int getRealIndex(int idxOfFile);
protected:
    //walk with root dict
    struct _BNode *rootDictWalk(const char* key);
    struct _BNode *nodeDictWalk(const char* key,struct _BNode *start, bool utf8Compat=true);
    struct _BNode *fileListWalk(int index);

    bool keyMatch(const char* search,const char* tkey,size_t tkeyLen);
    void fileListValidWalk();   //检查性质的过一遍fileList_,其中会适应BitComet扩展移除padding文件。
private:
    bool bValid_;
    struct _BNode *root_;
    //for temp use. when get info at good order, walk will go fast.
    struct _BNode *nextWalk_;
    struct _BNode *infoDict_;  //invalid when bValid!=true
    struct _BNode *filesList_;
    int fileCount_;  //-1 not determined
    JinBuffer content_;
    JinBuffer infoHash_;

    bool bHasBcPadding_;
};

#endif // JINTORRENT_H
