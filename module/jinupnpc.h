#ifndef JINUPNPC_H
#define JINUPNPC_H
#include "jinnetaddress.h"
#include "jinthread.h"
//#include "jinsingleton.h"


//只能通过attach,detach来增减引用，不可直接删除。
//这个类不要做回调。线程函数里面不要依赖打日志
class UPNPDroppableData;
class JinUPnPc
{
public:
    JinUPnPc();
    ~JinUPnPc();

    //以下函数都可能返回invalid对象表示不成功.
    JinNetAddr GetLanAddr();   //获取得到自己的局域地址.
    JinNetAddr GetOuterAddr(); //获得路由设备出口地址.
    JinNetPort GetOuterPort(); //绑定成功的出口端口.
    JinNetPort GetInnerPort();  //设置用于做映射的inner

    //尝试打开upnp端口映射.提高打孔成功.
    //不关注结果返回,不关心外部开了啥端口,会根据自己局域ip找合适的.
    void OpenPort(const JinNetPort &innerPort);

    static THREADFUNCRTN OpenUPNPDroppable(THREADFUNCPARAM);

private:
    enum JUPNPState{JUS_IDLE,JUS_DOING,JUS_FAILED,JUS_DONE};
    THREADHANDLE handle_;
private:
    UPNPDroppableData *data_;
};


//typedef JinSingleton<JinUPnPc> JinUPNP;

#endif // JINUPNPC_H
