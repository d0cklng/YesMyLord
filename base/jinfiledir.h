#ifndef JINFILEDIR_H
#define JINFILEDIR_H
#include <stddef.h>
#ifdef JWIN
#define JSYSSEP "\\"
#else
#define JSYSSEP "/"
#endif
enum JinDirRtn
{
    kRtnTypeNotSet = -5,
    kRtnCdUpTop = -4,   //top level can't cd up any more.
    kRtnInternalBuffer = -3,  //internal buffer not long enough.
    kRtnInvalidCharacter = -2,
    kRtnErrParse = -1,
    kRtnSuccess = 0,
    kMaxFilePathLength = 512
};
enum JinDirPredef
{
    kDirCurr,
    kDirHome,
    kDirTemp,
    kDirExe,
};
typedef const char* CCTypeStr;
typedef CCTypeStr *PCCType;

class JinFileDir
{
public:
    JinFileDir();
    JinFileDir(CCTypeStr path);
    JinFileDir(CCTypeStr path, bool withFileName);
    JinDirRtn cd(CCTypeStr dir);
    JinDirRtn cdUp();
    //只接受相对路径,dir本身要已经准备好的.
    bool mk(CCTypeStr subPath);

    //当前path完整路径加上filename，filename允许Null或空串，返回串没有/结尾
    CCTypeStr fullPath(CCTypeStr filename=NULL);
    //如果提供pfname，path最后不以SEP结尾的部分将被当作文件名
    JinDirRtn fromPath(CCTypeStr path, PCCType pfname=NULL);
    JinDirRtn setWith(JinDirPredef def);
    size_t length();

    static void sSetCurrDir(CCTypeStr curdir); //设置当前目录，后面使用SdDir会用它.
    static char sCurrDirSet[kMaxFilePathLength];

    //path只接受完整全路径.  p=true递归创建.  已存在时可能返回的是false
    static bool mkdir(CCTypeStr path, bool p=true);
protected:
    //如果提供pfname，dir最后不以SEP结尾的部分将被当作文件名
    JinDirRtn _cd(CCTypeStr dir, PCCType pfname=NULL);
private:
    char m_filePath[kMaxFilePathLength];
    size_t m_mvIdx;

};

#endif // JINFILEDIR_H
