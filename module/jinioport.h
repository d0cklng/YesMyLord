#ifndef JINIOPORT_H
#define JINIOPORT_H

#include "jinasyncdef.h"
//#include "jinfile.h"

#ifdef AE_USING_IOCP
typedef HANDLE HdPort;
#else
#include "ev.h"
# ifndef TRUE
#  define TRUE 1
#  define FALSE 0
typedef int BOOL;
typedef uint32_t DWORD;
# endif
typedef unsigned long ULONG_PTR,*PULONG_PTR;
//struct LibevPortInfo;
//typedef struct LibevPortInfo *HdPort;
struct HandlePortContent;
typedef struct HandlePortContent *HdPort;

typedef struct _OVERLAPPED
{
    //uint64_t offset;
    ev_io evio;  //must at first!
    HdPort port;
}OVERLAPPED,*LPOVERLAPPED;

extern int evMixIO(HdPort port,JinAsynData* data);
#endif
typedef uint64_t U64PTR;

extern HdPort CreateIoPort();
extern bool BindToPort(HdObject obj, HdPort port, ULONG_PTR ck);
extern int  DropFromPort(HdObject obj, HdPort port);
extern bool GetQueuedCompletion(HdPort port, uint32_t* bytesTransfer,
                                PULONG_PTR user,LPOVERLAPPED* ppoverlapped,
                                uint32_t timeout);
extern bool PostQueuedCompletion(HdPort port,uint32_t bytesTransfer,
                                 ULONG_PTR ck,LPOVERLAPPED poverlapped);
//extern BOOL CancelHandleIO(HdObject obj);
extern void CloseIoPort(HdPort port);


#endif // JINIOPORT_H
