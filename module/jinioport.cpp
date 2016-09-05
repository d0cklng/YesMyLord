#include "jinioport.h"
#include "jinformatint.h"
#include "jinhashmap.h"
#include "jinlinkedlist.h"
#include "jinmemorycheck.h"
//#include "jinsignaledevent.h"
#include "jinmutex.h"
#include "jinlogger.h"
#include "jinasyncengine.h"  //for JinAsynData

#ifndef AE_USING_IOCP
#include "ev.h"
//#include<sys/types.h>
//#include<sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>

struct IOEvent
{
    //IOType type;
    HdObject handle;
    ULONG_PTR completeKey;
    uint32_t bytesTransfer;
    LPOVERLAPPED poverlapped;
};

struct HandlePortContent
{
    //struct ev_loop* loop;  //must at first slot
    EV_P;
    JinHashMap<HdObject,ULONG_PTR> SetOfCompleteKey;
    ev_async asyncEv;
    //Post操作的可能是不同线程，所以需要保护。
    //pend动作只是主线程推进和取出处理，所以不保护
    JinHashMap<ev_io*,LPOVERLAPPED> ioPend;
    JinLinkedList<IOEvent*> readyEvents;
    JinMutex readyMutex;
};

#endif

//===============================
#ifndef AE_USING_IOCP
void PushComplete(HdPort port, HdObject handle, uint32_t bytesTransfer,LPOVERLAPPED poverlapped,ULONG_PTR completeKey)
{
    IOEvent* ioEvent = (IOEvent*)JMalloc(sizeof(IOEvent));
    memset(ioEvent,0,sizeof(IOEvent));
    ioEvent->bytesTransfer = bytesTransfer;
    ioEvent->handle = handle;
    ioEvent->completeKey = completeKey;
    ioEvent->poverlapped = poverlapped;

    port->readyMutex.lock();
    port->readyEvents.push_back(ioEvent);
    port->readyMutex.unlock();

    ev_async_send(port->loop,&port->asyncEv);
}
void CallbackEV(EV_P, struct ev_async *watcher, int revents)
{
    UNUSED(watcher);
    UNUSED(revents);
    ev_unloop(loop,EVUNLOOP_ONE);
}
void CallbackEvIo( EV_P, ev_io *io_ev, int revents )
{
    UNUSED(revents);
    ev_io_stop(EV_A, io_ev);
    JinAsynData* data = (JinAsynData*)io_ev;
#ifdef JDEBUG
    bool rmRet =
#endif
    data->overlap.port->ioPend.remove(io_ev);
    JAssert(rmRet==true);
    JAssert(data->overlap.port->SetOfCompleteKey.has(data->handle));
    ULONG_PTR ck = data->overlap.port->SetOfCompleteKey.get(data->handle,0);
    switch(data->type)
    {
    case kSockAccept:
    {
        sockaddr* saddr = (sockaddr*)data->buffer->buff();
        socklen_t len = sizeof(sockaddr);
        int newfd = accept(data->handle,saddr,&len);
        if(newfd!=-1)
        {
            data->user = (void*)(long)newfd;
            newfd = data->opSize; // newfd as bytetransfer.
        }
        PushComplete(data->overlap.port,data->handle,newfd,&data->overlap,ck);
    }
        break;
    case kSockConnect:
    {
        int error = 0;
        socklen_t len = sizeof(error);
        if(getsockopt(data->handle, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {   error = -1;  }  //if error == 0, means conn success.
        PushComplete(data->overlap.port,data->handle,error,&data->overlap,ck);
    }
        break;
    case kSockSend:
    {
        size_t buflen = data->opSize;
        uint32_t bufPos = data->tranSize;
        void* buf = data->buffer->buff(bufPos);
        int sended = 0;
        do
        {
            sended = send(data->handle,buf,buflen,0);
        }
        while(sended < 0 && errno == EINTR);
        if(sended < 0){
            JELog("error send() %d",errno);
            sended = 0;
        }
        PushComplete(data->overlap.port,data->handle,sended,&data->overlap,ck);
    }
        break;
    case kSockRecv:
    {
        size_t buflen = data->opSize;
        uint32_t bufPos = data->tranSize;
        void* buf = data->buffer->buff(bufPos);
        int recved = 0;
        do
        {
            recved = recv(data->handle,buf,buflen,0);
        }
        while(recved < 0 && errno == EINTR);
        if(recved < 0){
            JELog("error recv() %d",errno);
            recved = 0;
        }
        PushComplete(data->overlap.port,data->handle,recved,&data->overlap,ck);
    }
        break;
    case kSockSendTo: //see postSendTo
    {
        sockaddr_in tarAddr;
        tarAddr.sin_family = AF_INET;
        tarAddr.sin_addr.s_addr = (uint32_t)data->user64;
        tarAddr.sin_port = (uint16_t)(long)data->user;

        size_t buflen = data->opSize;
        uint32_t bufPos = data->tranSize;
        void* buf = data->buffer->buff(bufPos);
        int sended = 0;
        do
        {
            sended = sendto(data->handle,buf,buflen,0,
                            (const struct sockaddr*)&tarAddr,
                            sizeof(struct sockaddr));
        }
        while(sended < 0 && errno == EINTR);
        PushComplete(data->overlap.port,data->handle,sended,&data->overlap,ck);
    }
        break;
    case kSockRecvFrom:
    {
        size_t buflen = data->opSize - sizeof(sockaddr);
        uint32_t bufPos = data->tranSize;
        void* buf = data->buffer->buff(bufPos);
        JDLog("recvBuf=%p,pos=%u,len=%"PRISIZE,buf,bufPos,buflen);
        //int addr_len = sizeof(struct sockaddr_in);
        sockaddr* sa = (sockaddr*)(data->buffer->buffc(bufPos+buflen));
        int iByte = (int) sizeof(sockaddr);
        int recved = 0;
        do
        {
            recved = recvfrom(data->handle,buf,buflen,0,sa,(socklen_t*) &iByte);
        }
        while(recved < 0 && errno == EINTR);
        PushComplete(data->overlap.port,data->handle,recved,&data->overlap,ck);
    }
        break;
    case kSockDisconnect:
    {
        //int ret = close(data->handle);
        PushComplete(data->overlap.port,data->handle,0,&data->overlap,ck);
    }
        break;
    default:
        JAssert2(false,"unexpect data->type @evio-cb");
    }
}
//关于LPOVERLAPPED最干净的做法是把send recv需要的参数存起来等io ready的时候使用.
//但是为了简便，这里以来到了jinasyncengine的jinasyndata直接使用而不再存来取去了.
int evMixIO(HdPort port,JinAsynData* data)
{
    data->overlap.port = port;
    switch(data->type)
    {
    case kSockAccept:
    case kSockRecv:
    case kSockRecvFrom:
    {
        ev_io_init(&(data->overlap.evio),CallbackEvIo,data->handle,EV_READ);
    }
        break;
    case kSockConnect:
    {
        ev_io_init(&(data->overlap.evio),CallbackEvIo,data->handle,EV_WRITE);

        sockaddr_in tarAddr;
        tarAddr.sin_family = AF_INET;
        tarAddr.sin_addr.s_addr = data->user64;//rawIP;
        tarAddr.sin_port = (uint16_t)(long)data->user;//rawPort;
        JAssert(sizeof(struct sockaddr)==sizeof(sockaddr_in));

        int ret = connect(data->handle,(struct sockaddr*)&tarAddr,sizeof(struct sockaddr));
        if(ret != -1 || errno != EINPROGRESS)
        {
            JAssert(ret != 0);
            return ret;
        }
    }
        break;
    case kSockSend:
    case kSockSendTo:
    case kSockDisconnect: //要一个写信号然后关掉，因为我认为发送信号意味着发送已经差不多了.
    {
        ev_io_init(&(data->overlap.evio),CallbackEvIo,data->handle,EV_WRITE);
    }
        break;
    default:
        JAssert2(false,"unexpect data type @evMixIO");
        return -1;
    }
    port->ioPend.set(&(data->overlap.evio),&data->overlap);
    ev_io_start(port->loop,&(data->overlap.evio));
    return 0;
}
#endif

HdPort CreateIoPort()
{
#ifdef AE_USING_IOCP
    return CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
#else
    HdPort port= JNew(HandlePortContent);
    if(port)
    {
        port->loop = ev_default_loop(0);
        ev_init(&port->asyncEv,CallbackEV);
        ev_async_start(port->loop,&port->asyncEv);
    }
    return port;
#endif
}

bool BindToPort(HdObject obj, HdPort port, ULONG_PTR ck)
{
    JDLog("BindToPort obj=%ld",(long)obj);
#ifdef AE_USING_IOCP
    return (NULL!=CreateIoCompletionPort(obj,port,ck,0));
#else
    return port->SetOfCompleteKey.set(obj,ck);
    //ev_set_userdata(port->loop,port);
    //return sBindInfoEv.set(obj,port); //TODO!!
#endif
}

int DropFromPort(HdObject obj, HdPort port)
{
    int ret=0;
    JDLog("handle=%ld",(long)obj);
#ifdef AE_USING_IOCP
    //当CloseHandle的时候应该会清理掉吧.
    WSACancelAsyncRequest(obj);
    UNUSED(port);
#else
    JDLog("DropFromPort obj=%ld",(long)obj);
    // JAssert(port->SetOfCompleteKey.has(obj));

    ULONG_PTR ck = port->SetOfCompleteKey.get(obj,0);
    JAssert(ck!=0);
    port->SetOfCompleteKey.remove(obj);

    //移除未激发的异步投递. ...
    JinHashMap<ev_io*,LPOVERLAPPED>::iterator it = port->ioPend.begin();
    while(it!=port->ioPend.end())
    {
        LPOVERLAPPED po = it.second();
        JinAsynData* data = (JinAsynData*)po;
        JAssert(po->port == port);
        if(data->handle == obj && po->port==port)
        {
            JDLog("remove io=%p still pending",it.first());
            ++ret;
            ev_io_stop(po->port->loop,it.first());
            it = port->ioPend.erase(it);  //vv投递一个bytetransfer=0的完成消息.
            //PushComplete(port,(IOType)data->type,0,po,(void*)(long)data->handle); //TODO type error
            PushComplete(port,data->handle,0,po,ck);
        }
        else
        {
            ++it;
        }
    }
#endif
    return ret;
}

bool GetQueuedCompletion(HdPort port, uint32_t *bytesTransfer,
                         PULONG_PTR ck, LPOVERLAPPED *ppoverlapped,
                         uint32_t timeout)
{
#ifdef AE_USING_IOCP
    return (TRUE==GetQueuedCompletionStatus(port,(LPDWORD)bytesTransfer,ck,ppoverlapped,timeout));
#else
    if(timeout == INFINITE)  {
        ev_loop(port->loop,0);
    }
    else {
        ev_loop_timeout(port->loop,
                (timeout>0)?EVRUN_ONCE:EVRUN_NOWAIT,
                timeout);
    }

    //port->readySignal.wait(INFINITE);
    port->readyMutex.lock();
    if(port->readyEvents.size()==0){
        port->readyMutex.unlock();
        *bytesTransfer = 0;
        *ck = 0;
        *ppoverlapped = NULL;
        return false;
    }
    IOEvent* event = port->readyEvents.front();
    port->readyEvents.pop_front();
    if(port->readyEvents.size())
    {    //由于多次send只产生一次
        //JDLog(" {asend} ");
        ev_async_send(port->loop,&port->asyncEv);
    }
    port->readyMutex.unlock();

    *bytesTransfer = event->bytesTransfer;
    *ck = event->completeKey;
    *ppoverlapped = event->poverlapped;
    //if(event->eio){    } //TODO
    JFree(event);
    return true;
#endif

}

bool PostQueuedCompletion(HdPort port, uint32_t bytesTransfer, ULONG_PTR ck, LPOVERLAPPED poverlapped)
{
#ifdef AE_USING_IOCP
    return (TRUE==PostQueuedCompletionStatus(port,(DWORD)bytesTransfer,ck,poverlapped));
#else
    PushComplete(port,HDNULL,bytesTransfer,poverlapped,ck);
    return true;
#endif
}

void CloseIoPort(HdPort port)
{
#ifdef AE_USING_IOCP
    CloseHandle(port);
#else
    ev_async_stop(port->loop,&port->asyncEv);
#ifdef JDEBUG
    JinHashMap<ev_io*,LPOVERLAPPED>::iterator it = port->ioPend.begin();
    while(it!=port->ioPend.end())
    {
        LPOVERLAPPED po = it.second();
        JinAsynData* data = (JinAsynData*)po;
        JAssert(po->port == port);
        JDLog("{hd=%ld, opSize=%u, tranSize=%u, type=%d}",
               (long)data->handle,data->opSize,data->tranSize,data->type);
        ++it;
    }
#endif
    JAssert(port->ioPend.size() == 0);  //通常是因为没有关闭一些东西直接关引擎导致.
    JAssert(port->readyEvents.size() == 0);
    JAssert(port->SetOfCompleteKey.size() == 0);
    //port->SetOfCompleteKey.clear();
    JDelete(port);
#endif
}

//BOOL CancelHandleIO(HdObject obj)
//{
//#ifdef AE_USING_IOCP //?? not work @ win
//   return CancelIo(obj);
//    //return ;
//#else
//    UNUSED(obj);
//    return TRUE;  //TODO ??
//#endif
//}
