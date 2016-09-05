#ifndef ISINKFORLORD_H
#define ISINKFORLORD_H
#include "lorderror.h"

struct NetPacket;
class IXLorb
{
public:
    virtual bool DoSend(NetPacket* pack, int DataLenInPack, NetID id, uint16_t tunnid=0) = 0;
    virtual void DoClose(const NetID &id) = 0;

    //通知发生了错误
    virtual void OnError(LordError err, bool fromOutward=true) = 0;
};

#endif // ISINKFORLORD_H

