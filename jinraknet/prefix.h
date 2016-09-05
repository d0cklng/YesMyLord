#ifndef _PREFIX_H
#define _PREFIX_H

#ifdef QT_VERSION
#include <QtCore/qglobal.h>
#else
#include "macro.h"
#endif

#if defined (Q_CC_MSVC)
#pragma warning(disable : 4996 4819)
 //示例pragma warning( disable : 4507 34; once : 4385; error : 164 )
 //上C4819保存unicode的警告 4996sprintf不安全的警告
#endif

#ifdef Q_OS_WIN  //window老大难问题,同时包含winsock和windows时必须先包含winsock2
#include <winsock2.h>
#include <windows.h>
#if QT_VERSION==0x050001  //window中会重新定义min,所以马上undef min
#undef min
#endif
#endif

//***************
//此文件是做一些修正性工作,同时将qt的全局导入. 即include qglobal.h
//***************

#if QT_VERSION==0x050000
#define NOMINMAX   //Bug Qt5.0.0+MSVC2010 QDateTime有所谓的语法错误
#endif

#if QT_VERSION==0x050001 || QT_VERSION==0x050002 || QT_VERSION==0x050101
#undef min
#endif


//#ifdef declarative_debug  //统一我们的debug宏
//#define _ODEBUG
//#endif


#endif
