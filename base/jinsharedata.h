#ifndef JINSHAREDATA_H_201409212112
#define JINSHAREDATA_H_201409212112
#include "jinformatint.h"

/* ******************************
 *
 * 缓冲区简单管理，可以方便以参数方式传递而不用担心内存拷贝
 * 依赖: -无-
 *
 * ******************************/

class JinShareData
{
public:
    JinShareData();
    virtual ~JinShareData();

    //会因为malloc问题，pref为0，attach这样的对象失败。（不失败的话，对象内存释放时机无法控制.
    bool attach();
    void detach();  //TODO 线程问题,同时detach会有不预期的问题..
    int32_t ref();
private:
    int32_t *pref;

};

#endif // JINSHAREDATA_H
