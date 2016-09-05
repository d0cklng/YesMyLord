#ifndef IMPLICTLIBRARYLOAD_H
#define IMPLICTLIBRARYLOAD_H

#ifdef QT_VERSION
#include <QString>
#include <QLibrary>
#include <QCoreApplication>


static  bool _i_LoadLibrary(QLibrary &dlib,const QString name,const QString runpath=QString())
{  //根据名字,先从当前运行目录runpath找,然后系统目录找.如果找到,开启
    QString path;
    if(runpath.isEmpty()){
        path = QCoreApplication::applicationDirPath() + "/" + name;
    }
    else{
        if(runpath.endsWith('/'))path = runpath + name;
        else path = runpath + "/" + name;
    }
    dlib.setFileName(path);
    if(!dlib.load())
    {
        dlib.setFileName(name);
        dlib.load();
    }
    return dlib.isLoaded();
}


//使我们特定形式的dll由静态加载成为动态加载
//BODY指定实体名字,FUNC指定dll导出函数的主题名字,DLL指定最终DLL文件名
#define ImplictLibraryLoad(BODY,FUNC,DLL)    \
extern "C"\
{\
    typedef I##BODY* (*ptr_Create##BODY)();\
    typedef void (*ptr_Destroy##BODY)(I##BODY* instance);\
}\
static int count##BODY = 0;\
static QLibrary lib_##BODY;\
static ptr_Create##BODY func_Create##BODY=0;\
static ptr_Destroy##BODY func_Destroy##BODY=0;\
static I##BODY* DyCreate##BODY(QString path="")\
{\
    I##BODY* i##BODY=0;\
    if(!func_Create##BODY)\
    {\
        if(!lib_##BODY.isLoaded() && !_i_LoadLibrary(lib_##BODY,#DLL,path))return 0;\
        func_Create##BODY = (ptr_Create##BODY)lib_##BODY.resolve("Create"#FUNC);\
        if(!func_Create##BODY)return 0;\
    }\
    i##BODY = func_Create##BODY();\
    if(i##BODY)count##BODY++;\
    return i##BODY;\
}\
static void DyDestroy##BODY(I##BODY* instance)\
{\
    if(!func_Destroy##BODY)\
    {\
        if(!lib_##BODY.isLoaded())return;\
        func_Destroy##BODY = (ptr_Destroy##BODY)lib_##BODY.resolve("Destroy"#FUNC);\
        if(!func_Destroy##BODY)return;\
    }\
    if(instance)count##BODY--;\
    func_Destroy##BODY(instance);\
    if(count##BODY==0)\
    {\
        func_Create##BODY=NULL;\
        func_Destroy##BODY=NULL;\
    }\
    return ;\
}























#endif  //endif QT_VERSION

#endif
