#ifndef XPLOW_H
#define XPLOW_H
#include "jinnetaddress.h"

class PlowListenSocket;
class XPlow
{
public:
    XPlow(JinNetAddr outside,
          JinNetPort outport,
          JinNetAddr inside,
          JinNetPort srvport,
          JinNetAddr srvaddr = JinNetAddr("127.0.0.1"));
    ~XPlow();
    bool start();
    void stop();
private:
    JinNetAddr outside_;
    JinNetPort outport_;
    JinNetAddr inside_;
    JinNetPort srvport_;
    JinNetAddr srvaddr_;
    PlowListenSocket *listenSock_;
    bool isStart_;
};

#endif // XPLOW_H
