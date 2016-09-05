#if !defined(__Client_ISinkForNetEngine_h)
#define __Client_ISinkForNetEngine_h

#include "INetEngine.h"
//class INetEngine;


//使用NetEngine的人需要为他实现这些函数
class ISinkForNetEngine
{
public:
    /// 消息传递
    /// \param[in] src 消息来源于哪个NetEngine
    /// \param[in] id 消息来源于哪个NetID
    /// \param[in] addr4 消息来源于那个地址
    /// \param[in] packet 消息主体
    /// \param[in] DataLenInPack 消息主体的数据长度,不包括Head首1字节
    /// \param[in] head 表示发送时使用的是哪个头
    /// \return 返回大于等于0时函数返回后将保留包数据不释放,OnDelegateFree立即被触发,告诉调用者必要信息,将来利用该信息调用ManualFree释放
    /// \note 因为OnDelegateFree的默认实现.如果没有继承这个函数,返回任意值都会立即释放包.
   virtual int OnNetMsg(INetEngine* src,NetID id,struct sockaddr_in addr4,NetEngineMsg *packet, const int DataLenInPack ,int head)=0;
   //virtual void OnUpdate()=0;

   /// 当OnNetMsg返回大于等于0时,这个函数被调用,告知上层将来使用一,二参来释放空间.
   /// \param[in] iEngine 同OnNetMsg时的src
   /// \param[in] free 释放空间时的参数
   /// \param[in] netpacket 同OnNetMsg的packet
   /// \param[in] lenFromNetMsg 消息主体的数据长度,不包括Head首1字节.也就是除去PackType的长度
   /// \param[in] number OnNetMsg的返回值 大于等于0
   virtual void OnDelegateFree(INetEngine* iEngine,void* free,NetEngineMsg *netpacket,const int lenFromNetMsg,int number)
   {	//默认实现依然是立即释放
      Q_UNUSED (netpacket);
      Q_UNUSED (lenFromNetMsg);
      Q_UNUSED (number);
       iEngine->ManualFree(free);
   }

    virtual bool OnDatagram(char* netpack, int len, uint32_t naddr, NetPort nport)
    {  //返回true raknet将继续处理包,否则就忽略了.
        return true;
    }
};

#endif
