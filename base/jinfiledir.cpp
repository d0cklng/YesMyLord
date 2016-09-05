#include "jinfiledir.h"
#include "jinassert.h"
#include <stdio.h>
#include <string.h>  //strlen memset
#include <stdlib.h>
#ifdef JWIN
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "jinmemorycheck.h"
#ifdef JWIN
#define SYSSEP '\\'
#else
#define SYSSEP '/'
#endif

#define N00 '\0' // null terminate
#define CDE 'e'  //other char codec
#define CTL 't'  //control char ascii<128, \r\n included.

//#define OTH 'o'  //这些字符不做臆断.
#define HOM 'h'  //~ home
#define SEP 's'  //正反斜杠.
#define MRK 'm'  //普通符号.
#define DOT 'd'  //.  dot
#define BLK ' '  //   空格
#ifdef WIN32
#define COL 'c'  //:  colon
#define MRV 'v'  //invalid 符号
#endif

#define NUM 'n'  //num
//#define ULT 'U'  //大写字母.
#define ULT 'u'  //大写字母.
#define uLT 'u'  //小写字母.
#define LTR 'u'  //字母.
//'e'错误字符对待
//'s'普通符号
static const char dcmap[256] = {
    N00,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,
    CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,CTL,
#ifdef JWIN //invalid @win  ",22   *,2a   :,3a   <,3c   >,3e   ?,3f   |,7c
    BLK,MRK,MRV,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRV,MRK,MRK,MRK,DOT,SEP,
    NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,COL,MRK,MRV,MRK,MRV,MRV,
    MRK,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,
    ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,MRK,SEP,MRK,MRK,MRK,  //<-下划线.
    MRK,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,
    uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,MRK,MRV,MRK,HOM,CTL,
#else
    BLK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,MRK,DOT,SEP,
    NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,NUM,MRK,MRK,MRK,MRK,MRK,MRK,
    MRK,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,
    ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,ULT,MRK,SEP,MRK,MRK,MRK,
    MRK,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,
    uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,uLT,MRK,MRK,MRK,HOM,CTL,
#endif
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,
    CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE,CDE
};
static inline char cmap(const char c)
{
    return dcmap[(unsigned char)c];
}

static inline int jmkdir(const char* path)
{
#ifdef JWIN
    return _mkdir(path);
#else
    return ::mkdir(path,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
#endif
}

JinFileDir::JinFileDir()
    : m_mvIdx(0)
{}

JinFileDir::JinFileDir(CCTypeStr path)
    : m_mvIdx(0)
{
    if(kRtnSuccess != fromPath(path))
    {
        m_mvIdx = (size_t)(-1);
    }
}

JinFileDir::JinFileDir(CCTypeStr path, bool withFileName)
    : m_mvIdx(0)
{
    if(!withFileName) fromPath(path);
    else
    {
        CCTypeStr fname = NULL;
        if(kRtnSuccess != fromPath(path,&fname))
        {
            m_mvIdx = (size_t)(-1);
        }
    }
}

JinDirRtn JinFileDir::cd(CCTypeStr dir)
{
    return this->_cd(dir);
}

JinDirRtn JinFileDir::cdUp()
{
    if(m_mvIdx==0) return kRtnSuccess;
    if(m_mvIdx == (size_t)(-1)) return kRtnErrParse;
    size_t ckIdx = m_mvIdx-1;
    do
    {
        if(cmap(m_filePath[ckIdx])==SEP)
        {
            m_mvIdx = ckIdx;
            return kRtnSuccess;
        }
        if(ckIdx==0)break;
        --ckIdx;
    }
    while(true);
    return kRtnCdUpTop;
}

bool JinFileDir::mk(CCTypeStr subPath)
{
    if(m_mvIdx==0) return true;
    if(m_mvIdx == (size_t)(-1)) return false;

    m_filePath[m_mvIdx] = SYSSEP;
    strncpy(m_filePath+m_mvIdx+1,subPath,kMaxFilePathLength-m_mvIdx-2);
    m_filePath[kMaxFilePathLength-1] = 0;

    bool ret = JinFileDir::mkdir(m_filePath, false);
    //m_filePath[m_mvIdx] = N00;
    return ret;
}

CCTypeStr JinFileDir::fullPath(CCTypeStr filename)
{
    if(m_mvIdx == (size_t)(-1)) return NULL;
    m_filePath[m_mvIdx] = SYSSEP;
    if(filename){
        strncpy(m_filePath+m_mvIdx+1,filename,kMaxFilePathLength-m_mvIdx-2);
    }
    else{
        m_filePath[m_mvIdx+1] = N00;
    }
    return m_filePath;
}

/*
正反斜杠等效化处理, 支持路径中夹带./..
当前路径形态 "" "." "./" ".X"="./.X" "XX/XX" "..X"="./..X"
   win32     "/XX"  "c"
绝对路径形态 "/"
   win32    "c:" "c:/" "c:/"
上一级路径形态 ".." "../"
错误路径形态  ".:XX" ":XX" "XX~"
*/
JinDirRtn JinFileDir::fromPath(CCTypeStr path, PCCType pfname)
{
    m_mvIdx = 0;  //reset here.
    size_t ckIdx = 0;
    //首先是确定起始为相对还是绝对的阶段
    if(path[0])
    {
        switch(cmap(path[0]))
        {
#ifdef WIN32
        case LTR:   // "c:" "c:/"
            if(cmap(path[1]) == COL)
            {
                unsigned char c2 = cmap(path[2]);
                if(c2 != N00 && c2 != SEP)
                    return kRtnErrParse;
                m_filePath[0] = path[0];
                m_filePath[1] = ':';
                m_filePath[2] = SYSSEP;
                if(c2 == N00) ckIdx = 2;
                else ckIdx = 3;
                m_mvIdx = 2;
                return _cd(path+ckIdx,pfname);
            }
            break;
        case SEP: //fullpath对斜杠开头的路径按当前盘符根目录来取,是否参照??!
        {
            JinDirRtn rtn = setWith(kDirCurr);
            if(rtn != kRtnSuccess) return rtn;
            m_mvIdx = 2;
            return _cd(path+1,pfname);
        }
            break;
#else
        case SEP:  // "/"
            m_filePath[0] = SYSSEP;
            m_mvIdx = 0;
            return _cd(path+1,pfname);
            break;
#endif
        case HOM:  //"~
        {
            char c2 = cmap(path[1]);
            if(c2 == N00 || c2 == SEP)
            {
                setWith(kDirHome);
                return _cd(path+1,pfname);
            }
        }
            break;
        case DOT:
            switch(cmap(path[1]))
            {
            case N00:
                setWith(kDirCurr);
                return _cd(path+1,pfname);
                break;
            case SEP:
                ckIdx = 2;
                //setWith(kDirCurr);
                break;
            case DOT:
                switch(cmap(path[2]))
                {
                case N00:
                    setWith(kDirCurr);
                    cdUp();
                    return _cd(path+2,pfname);
                    break;
                case SEP:
                    setWith(kDirCurr);
                    cdUp();
                    return _cd(path+3,pfname);
                    break;
                default:
                    //return kRtnErrParse;
                    break;
                }
            default: //点开头的文件(夹)
                break;
            }
            break;
        case N00:
            JAssert(false);
            //no break;
        case CTL:
            return kRtnSuccess; //kRtnInvalidCharacter;
        default:
            break;
        }
        setWith(kDirCurr);
    }
    return _cd(path+ckIdx,pfname);
}

JinDirRtn JinFileDir::_cd(CCTypeStr dir, PCCType pfname)
{
    size_t lastSep = m_mvIdx;
    m_filePath[m_mvIdx++] = SYSSEP;
    size_t ckIdx = 0;
    while(dir[ckIdx]!=0)
    {
        switch(cmap(dir[ckIdx]))
        {
        case SEP:
            if(m_mvIdx-1 != lastSep)
            {
                lastSep = m_mvIdx;
                m_filePath[m_mvIdx++] = SYSSEP;
            }
            break;
        case CDE:
        case MRK:
        case HOM:
        case NUM:
        case BLK:
        case LTR:
            m_filePath[m_mvIdx++] = dir[ckIdx];
            break;
        case DOT:
            if(m_mvIdx-1 == lastSep)  //紧挨SEP的点要判断一下是不是.和..
            {
                unsigned char cc = cmap(dir[ckIdx+1]);
                switch(cc)
                {
                case SEP:
                    JAssert(cmap(dir[ckIdx-1])!=DOT); //因为 ../一定会被处理和消灭
                    ++ckIdx;
                    break;
                case N00:
                    break;
                case DOT:
                    cc = cmap(dir[ckIdx+2]);
                    switch(cc)
                    {
                    case SEP:  //   "/../
                    case N00:
                        ckIdx+=2;
                        --m_mvIdx;
                        this->cdUp();
                        lastSep = m_mvIdx;
                        //++m_mvIdx;
                        continue;
                        break;
                    default:
                        m_filePath[m_mvIdx++] = dir[ckIdx];
                        break; //allow .. inside path
                    }
                    break;
                default:
                    m_filePath[m_mvIdx++] = dir[ckIdx];
                    break;
                }
            }
            else
            {
                m_filePath[m_mvIdx++] = dir[ckIdx];
            }
            break;
        case N00:
            JAssert(false);
        case CTL:  //
            break;
#ifdef WIN32
        case MRV:
        case COL:
#endif
        default:
            return kRtnInvalidCharacter;
        }
        ++ckIdx;
        if(m_mvIdx > kMaxFilePathLength-2) return kRtnInternalBuffer;
    }

    if(m_mvIdx>0 && cmap(m_filePath[m_mvIdx-1])==SEP)
    {  //以SEP结尾的要退后一步.
        --m_mvIdx;
    }
    if(pfname)
    {
        m_filePath[m_mvIdx] = N00;
        *pfname = &m_filePath[lastSep+1];
        m_mvIdx = lastSep;
    }
    return kRtnSuccess;
}

JinDirRtn JinFileDir::setWith(JinDirPredef def)
{
    switch(def)
    {
    case kDirCurr:
    {
        if(sCurrDirSet[0])
        {
            return fromPath(sCurrDirSet);
        }
#ifdef JANDROID
        return this->fromPath("/sdcard");
#elif defined(JWIN)
        char buf[kMaxFilePathLength];
        DWORD rtn = ::GetCurrentDirectoryA(kMaxFilePathLength,buf);
        if(rtn==0 || rtn>kMaxFilePathLength)
        {
            return kRtnInternalBuffer;
        }
        return fromPath(buf);
#else  //unix/linux
        char buf[kMaxFilePathLength];
        char* rtn = realpath(".",buf);
        if(rtn)
        {
            return fromPath(rtn);
        }
        else
        {
            return kRtnErrParse;
        }
#endif
    }
        break;
    case kDirHome:
    {
#ifdef JANDROID
        return this->fromPath("/sdcard");
#elif defined(JWIN)
        char* rtn = getenv("USERPROFILE");
        if(rtn)
        {
            return fromPath(rtn);
        }
        else
        {
            return kRtnErrParse;
        }
#else //unix/linux
        char* rtn = getenv("HOME");
        if(rtn)
        {
            return fromPath(rtn);
        }
        else
        {
            char buf[kMaxFilePathLength];
            rtn = realpath("~",buf);
            if(rtn)
            {
                return fromPath(rtn);
            }
            else
            {
                return kRtnErrParse;
                //return fromPath("/root");
            }
        }
#endif
    }
        break;
    case kDirTemp:
    {
#ifdef JANDROID
        return fromPath("/data/local/tmp");
#elif defined(JWIN)
        char* rtn = getenv("TEMP");
        if(rtn)
        {
            return fromPath(rtn);
        }
        else
        {
            return kRtnErrParse;
        }
#else //unix/linux
        return fromPath("/tmp");
#endif
    }
        break;
    case kDirExe:
    {
#ifdef JWIN
        char buf[kMaxFilePathLength];
        DWORD rtn = ::GetModuleFileNameA(NULL,buf,kMaxFilePathLength);
        if(rtn==0)
        {
            return kRtnInternalBuffer;
        }
        else
        {
            CCTypeStr exename = NULL;
            return fromPath(buf,&exename);
        }
#else
        char curDirBuf[kMaxFilePathLength] = {0};
        ssize_t vlen = readlink("/proc/self/exe",curDirBuf,kMaxFilePathLength);
        if(vlen>0)
        {
            CCTypeStr exename = NULL;
            return fromPath(curDirBuf,&exename);
        }
        else
        {
#ifdef JANDROID
            return this->fromPath("/sdcard");
#else
            return kRtnErrParse;
#endif
        }
#endif

    }
        break;
    }  //switch
    JAssert(false);
    return kRtnTypeNotSet;
}

size_t JinFileDir::length()
{
    if(m_mvIdx == (size_t)(-1)) return 0;
    return m_mvIdx+1;
}
char JinFileDir::sCurrDirSet[kMaxFilePathLength] = {0};
void JinFileDir::sSetCurrDir(CCTypeStr curdir)
{
    JinFileDir dir(curdir);
    CCTypeStr str = dir.fullPath();
    if(str)
    {
        strncpy(sCurrDirSet,str,kMaxFilePathLength);
    }
}
//mkdir -p tmpdir/{trunk/sources/{includes,docs},branches,tags}
//tmpdir
//-|trunk
//--|sources
//---|includes
//---|docs
//-|branches
//-|tags        //TODO implement this!
bool JinFileDir::mkdir(const char* path, bool p)
{
    JAssert(path);
    int ret = 0;
    ret = jmkdir(path);
    if(ret != 0 && p)
    {
        size_t pathlen = strlen(path)+1;
        char *workPath = JNewAry(char,pathlen);
        if(workPath==NULL)return false;
        memset(workPath,0,pathlen);
        for(size_t i=0;i<pathlen;i++)
        {
            if(path[i]==SYSSEP && i!=0)
            {
#ifdef JWIN
                if( (_access( workPath, 0 )) == -1 )
                {
                    ret = jmkdir(workPath);
                    if(ret!=0) {   goto label_p_check_done;  }
                }
#else
                struct stat infoStat;
                if(0==stat(workPath,&infoStat))
                {
                    if(!S_ISDIR(infoStat.st_mode))
                    {   goto label_p_check_done;  }
                }
                else
                {
                    //ENOENT         参数file_name指定的文件不存在
                    //ENOTDIR        路径中的目录存在但却非真正的目录
                    //ELOOP          欲打开的文件有过多符号连接问题，上限为16符号连接
                    //EFAULT         参数buf为无效指针，指向无法存在的内存空间
                    //EACCESS        存取文件时被拒绝
                    //ENOMEM         核心内存不足
                    //ENAMETOOLONG   参数file_name的路径名称太长
                    if(errno==ENOENT)
                    {
                        ret = jmkdir(workPath);
                        if(ret!=0) {   goto label_p_check_done;  }
                    }
                    else
                    {   goto label_p_check_done;  }
                }
#endif
            }
            workPath[i] = path[i];
        }
        ret = jmkdir(path);
label_p_check_done:
        JDelAry(workPath);
    }
    return (ret == 0);

}
