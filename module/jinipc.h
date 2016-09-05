#ifndef JINIPC_H
#define JINIPC_H

#include "jinformatint.h"
#include "jinstring.h"
#include "jinudpsocket.h"
/*
 * 通过简短的共享内存表明一些信息,然后根据信息通过udp进行通信
 * 好处是①异步,符合其他模块的异步模型,
 * ②不存在端口冲突,
 * ③可以加入口令验证
 *
 * 开始都会绑定本地随机端口.如果是startHost将开辟一段共享内存填入一些信息
 * 如果startJoin则尝试读入共享内存然后获取并联络其中的端口
 * */
class JinShareMemory;
class JinSinkForIPC
{
public:
    virtual void OnIPCMsg(uint16_t id, void* buf, uint32_t len) = 0;
};

class JinIPC : public JinUdpSocket
{
public:
    JinIPC(const char* name);
    ~JinIPC();

    void setCallback(JinSinkForIPC* cb){callback_ = cb;}
    //开始主持构造时指定的名字,host不能主动发送,只能被动回复
    bool startHost(const char* psw, uint32_t len);
    //加入构造时指定的名字,会简单检查密码是否OK.来往数据经过简单密码XOR
    bool startJoin(const char* psw, uint32_t len);

    //Join模式直接发送给目标, Host模式仅可在OnIPCMsg时使用
    bool sendIPCMsg(const void* buf, uint32_t len);
    //不分模式直接发
    bool sendIPCMsg(uint16_t id,const void* buf, uint32_t len);

    void stop();
protected:
    //void OnSendTo(void *buf, uint32_t byteToSend,uint32_t byteSended,
    //              const JinNetAddr& addr, const JinNetPort& port);
    void OnRecvFrom(void *buf, uint32_t byteRecved,
                    const JinNetAddr& addr, const JinNetPort& port);

protected:
    void packetDoBlur(void *buf, uint32_t len); //使用psw_混淆,原buf上修改.
    //void packetRestore(void *buf, uint32_t len); //使用psw_还原,原buf被修改.
    uint16_t simplePswToU16(const char* psw, uint32_t len);
private:
    JinString name_;
    JinString psw_;
    JinShareMemory* sm_;
    bool isHost_;
    JinAsynBuffer recvBuf_;
    JinNetPort selfPort_; //自身绑定端口,不管是Host还是Join
    JinNetPort targetPort_; //目标端口,Join方式是固定的, Host方式是不确定的.
    JinSinkForIPC *callback_;

};

#endif // JINIPC_H
