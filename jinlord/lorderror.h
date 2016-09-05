#ifndef LORDERROR_H
#define LORDERROR_H

typedef enum _LordError
{
    LeSuccess = 0,
    LeOutOfMemory,
    LeProxyBindErr,
    LeProxyListenErr,
    LeProxyAcceptErr,

    LeUnexpectCompress,
    LeDecompressFail,
    LeCompreeNegotiationFail,
    LeBadFlowAckByA,

    __LeSendError = 11100,
    LeSendGreeingErr,
    LeSendReplyAckErr,

    __LeRecvError = 11200,

}LordError;

class LordErr
{
public:
    static const char* toStr(LordError err);
protected:
    static char innerBuffer[128];
private:
    LordErr();
    ~LordErr();
};

#endif // LORDERROR_H
