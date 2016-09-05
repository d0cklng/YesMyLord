#include "jinsetting.h"
#include <stdlib.h>
#include "jinfiledir.h"

JinSetting::JinSetting()
{
}

JinString JinSetting::getConfigPath(const char* organization, const char* application)
{
    // Unix and Mac OS X:  $HOME/.config/organization/application
    // Embedded Linux: $HOME/Settings/
    // Windows: %APPDATA%\organization\application
    // 函数如果失败，就沦为本地路径了..
    JinString path;
#ifdef JWIN
    char* appdata = getenv("APPDATA");
    if(appdata)
    {
        path = appdata;
        path += JSYSSEP;
    }
    else
    {
        char full[350];
        appdata = _fullpath(full,".",350);
        if(appdata)
        {
            path = appdata;
            path += JSYSSEP;
        }
    }
#else
    char* appdata = getenv("HOME");
    if(appdata)
    {
        path = appdata;
        path += JSYSSEP".config"JSYSSEP;
    }
    else
    {
        char curpath[512];
        appdata = realpath(".",curpath);
        if(appdata)
        {
            path = appdata;
            path += JSYSSEP;
            //free(curpath);
        }
    }
#endif
    if(organization){
        path += organization;
        path += JSYSSEP;
    }
    if(application){
        path += application;
        path += JSYSSEP;
    }
    return path;
}



/* windows 常见环境变量大全
%ALLUSERSPROFILE%=C:\Documents and Settings\All Users 列出所有用户profile文件位置
%APPDATA%=C:\Documents and Settings\xx\Application Data 列出应用程序数据的默认存放位置
%CD%=C:\Documents and Settings\xx 列出当前目录。
%CLIENTNAME%=Console 列出联接到终端服务会话时客户端的NETBIOS名。
%CMDCMDLINE% 列出启动当前cmd.exe 所使用的命令行。
%CMDEXTVERSION% 命令出当前命令处理程序扩展版本号。
%CommonProgramFiles%=C:\Program Files\Common Files 列出了常用文件的文件夹路径。
%COMPUTERNAME%=xx 列出了计算机名。
%ComSpec%=C:\WINDOWS\system32\cmd.exe  列出了可执行命令外壳 （命令处理程序）的路径。
%DATE% 列出当前日期。
%ERRORLEVEL% 列出了最近使用的命令的错误代码。
%HOMEDRIVE%=C: 列出用户主目录所在的驱动器盘符。
%HOMEPATH%=\Documents and Settings\xx 列出用户主目录的完整路径。
%HOMEDRIVE%%HOMEPATH%\桌面 桌面
%HOMEDRIVE%%HOMEPATH%\「开始」菜单 开始菜单
%INCLUDE%=D:\Program Files\Microsoft Visual Studio .NET 2003\SDK\v1.1\include\ vc用户变量
%LIB%=D:\Program Files\Microsoft Visual Studio .NET 2003\SDK\v1.1\Lib\ vc用户变量
%LOGONSERVER%=\\xx 列出有效的当前登录会话的域名控制器名。
%luapath%=F:\hero\bin lua用户变量
%LUA_DEV%=d:\Program Files\Lua\5.1 lua用户变量
%LUA_PATH%=;;d:\Program Files\Lua\5.1\lua\?.luac lua用户变量
%NUMBER_OF_PROCESSORS%=2 列出了计算机安装的处理器数。
%OS%=Windows_NT  列出操作系统的名字。(WindowsXP 和Windows2000 列为Windows_NT.)
%Path%=C:\WINDOWS\system32;C:\WINDOWS;d:\Program Files\Lua\5.1;d:\Program Files\Lua\5.1\clibs  列出了可执行文件的搜索路径。
%PATHEXT%=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.PSC1;.wlua;.lexe  列出操作系统认为可被执行的文件扩展名。
%PROCESSOR_ARCHITECTURE%=x86  列出了处理器的芯片架构。
%PROCESSOR_IDENTIFIER%=x86 Family 6 Model 15 Stepping 13, GenuineIntel  列出了处理器的描述。
%PROCESSOR_LEVEL%=6  列出了计算机的处理器的型号。
%PROCESSOR_REVISION%=0f0d 列出了处理器的修订号。
%ProgramFiles%=C:\Program Files  列出了ProgramFiles 文件夹的路径。应用程序的默认安装目录
%PROMPT%=$P$G  列出了当前命令解释器的命令提示设置。
%RANDOM% 列出界于0 和32767 之间的随机十进制数。
%SESSIONNAME%=Console 列出连接到终端服务会话时的连接和会话名。
%ssuser%=xx  vss用户变量
%SystemDrive%=C: 列出了Windows 启动目录所在驱动器。
%SystemRoot%=C:\WINDOWS  列出了Windows 启动目录的位置。
%TEMP%=C:\DOCUME~1\xx\LOCALS~1\Temp  列出了当前登录的用户可用应用程序的默认临时目录。
%TMP%=C:\DOCUME~1\xx\LOCALS~1\Temp  列出了当前登录的用户可用应用程序的默认临时目录。
%TIME%  列出当前时间。
%USERDOMAIN%=xx   列出了包含用户帐号的域的名字。
%USERNAME%=xx  列出当前登录的用户的名字。
%USERPROFILE%=C:\Documents and Settings\xx  列出当前用户Profile 文件位置。
%VS71COMNTOOLS%=D:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\ vc用户变量
%VS90COMNTOOLS%=D:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools\  vc用户变量
%windir%=C:\WINDOWS   列出操作系统目录的位置
*/
