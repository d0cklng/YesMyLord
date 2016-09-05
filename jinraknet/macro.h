#ifndef __MACRO_DEFINES_H
#define __MACRO_DEFINES_H

/// 各种宏定义
#ifndef QT_VERSION
#ifdef JWIN
#define Q_OS_WIN
#endif
#define Q_UNUSED(x) #x
#ifdef JMSVC
#define Q_CC_MSVC
#endif
#endif
//----------------------------------------------------------------------------
/// 自删除的接口定义，在类声明内部使用后，调用者可以直接调用该接口的Destroy()以销毁之

//----------------------------------------------------------------------------
/// 如果没有声明NULL，则定义为0
#ifndef NULL
#   define NULL 0
#endif//NULL

#ifndef TRUE
#	define TRUE 1
#endif

#ifndef FALSE
#	define FALSE 0
#endif

#ifndef MAX_PATH
#   define MAX_PATH 260
#endif

#ifndef MAX_PATH_UTF8
#   define MAX_PATH_UTF8 350
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

#define tDelete(p)    if(p){delete p;p=0;}
#define tDelArray(p) if(p){delete []p;p=0;}
#define tDestroy(p) if(p){p->Destroy();p=0;}


#define _STATIC_FACTORY_DECLARATIONS(x) static x* GetInstance(void); \
    static void DestroyInstance( x *i);
//    virtual void Destroy()=0;
#define _STATIC_FACTORY_DEFINITIONS(x,y) x* x::GetInstance(void) {return new y;} \
    void x::DestroyInstance( x *i) {delete ( y* ) i;}
//    void y::Destroy() {delete this;}

#define _DYNAMIC_FACTORY_DECLARATIONS(x) static x* Create(int module);\
    virtual void Destroy()=0;
#define _DYNAMIC_FACTORY_DEFINITIONS(x,y) static x* GetInstance(void){return new y;}; \
    void Destroy() {delete this;};

#endif//__MACRO_DEFINES_H
