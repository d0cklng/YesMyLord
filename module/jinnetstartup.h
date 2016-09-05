
#ifndef JINWSASTARTUP_H
#define JINWSASTARTUP_H
#ifdef JWIN
#include <winsock2.h>
//#include <Mstcpip.h>
#include <Mswsock.h>
#include <windows.h>
//#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include "jinsingleton.h"

class JinNetStartup : public JinSingleton<JinNetStartup>
{
public:
    JinNetStartup();
    ~JinNetStartup();
    inline bool valid(){ return weStart_; }
private:
    bool weStart_;
};

#endif // JINWSASTARTUP_H
