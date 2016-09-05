#ifndef JINERRORCODE_H
#define JINERRORCODE_H

#define MAKE_EC(jin_err,sys_err) (sys_err*100+jin_err)

enum  JinSocketErrorCode
{
    kTSucc = 0,
    kTSocketError = 1,
    kTBindError = 2,
    kTAcceptError = 3,
    kTConnectError = 4,
    kTSendError = 5,
    kTRecvError = 6,
    kTDisconnectError = 7,
    kTSocketAccept = 8,  //make socket for accept error
    kTAsynInit = 10,
};

#endif // JINERRORCODE_H
