#include "lorderror.h"
#include <stdio.h>

const char *LordErr::toStr(LordError err)
{
    switch(err)
    {
    case LeSuccess:
        return "success";
    case LeOutOfMemory:
        return "out of memory";
    case LeProxyBindErr:
        return "proxy bind() error";
    case LeProxyListenErr:
        return "proxy listen() error";
    case LeProxyAcceptErr:
        return "proxy accept() error";
    case LeUnexpectCompress:
        return "unexpect compress type";
    case LeDecompressFail:
        return "decompress fail";
    case LeCompreeNegotiationFail:
        return "compress negotiation fail";
    case LeBadFlowAckByA:
        return "bad ack at flow control recv by outward";

    case LeSendGreeingErr:
        return "send err while greeting";
    case LeSendReplyAckErr:
        return "send err while reply ack";

    default:
        sprintf(innerBuffer,"%d(LordErr not format)",(int)err);
        return innerBuffer;
    }
}

char LordErr::innerBuffer[128]={0};
