#ifndef JINSINGLETON_H
#define JINSINGLETON_H

/* ***************************************
 *
 * 有两种方式使用这个类,一种是class直接继承JinSingleton<class>用于全局静态类
 * 一种是typedef  JinSingleton<_class> class用于按需创建的单例对象
 *
 * ***************************************/

#include "jinmemorycheck.h"

template<typename T>
class JinSingleton
{
public:
    ~JinSingleton()
    {
        Free();
    }

    static T* Create()
    {
        T* &ret = instance();
        if(ret == NULL){

            //va_list ap;
            //va_start(ap, dbgStr);
            ret = JNew(T);
            //va_end(ap);
            //printf("ret=%p\n",ret);
            if(ret) ref() = 1;
        }
        else{
            ++ ref();
        }
        return ret;
    }

    //当存在全局singleton对象，进这个get时memory check还没初始化JNew JFree会崩溃.
    //如果要阻止静态单例的用法，则保留！
    static T* Get()
    {
        T* &ret = instance();
        return ret;
    }

    static void Free()
    {
        T* &ret = instance();
        if(!ret)return;
        if(-- ref() == 0)
        {
            JDelete(ret);
            //delete ret;
            instance() = NULL;
            ret = 0;
        }
    }

protected:
    static T* &instance()
    {
        static T* obj = 0;
        return obj;
    }
    static size_t & ref()
    {
        static size_t refer = 0;
        return refer;
    }
};



#endif // JINSINGLETON_H
