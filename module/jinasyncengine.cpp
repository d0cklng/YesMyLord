#include "jinnetstartup.h"
#include "jinasyncengine.h"
#include <string.h>  //memset
#include <stdio.h>
#include "jinassert.h"
#include "jinthread.h"
#include "jinmutex.h"
#include "jinlogger.h"
#ifndef JWIN
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#endif

struct JinAsynHandleDetail
{
    IAsynObj* obj;
    HdObject handle;  //因为obj可能清理，obj->handle会失效，handle用于识别延后处理的数据集
    JinMutex* fileOpMutex;  //WINDOWS下可以不创建这个对象 产生不锁的效果. 是为了防止对同一个handle同时做都写，file seek被移乱.
    volatile bool isCancel;
    int opCount;

};

class ExtraIOWorker : public JinThread
{
public:
    ExtraIOWorker(HdPort handle, JinMutex* mutex, JinSignaledEvent* signal, DataList* dataList)
        : JinThread()
//    #ifndef WIN32
        , portHandle_(handle)
        , mutex_(mutex)
        , signal_(signal)
        , dataList_(dataList)
    {
        this->setThreadName("ExtraIOWorker");
    }
    ~ExtraIOWorker()
    {
        this->stop();
    }

protected:
    void afterStart(uint64_t threadId,const char* threadName)
    {
        JDLog("threadid:%llu,threadName=[%s]", threadId, threadName);
    }

    void beforeStop(uint64_t threadId,const char* threadName)
    {
        JDLog("threadid:%llu,threadName=[%s]", threadId, threadName);
    }

    virtual bool period()
    {
        JinAsynData* data = NULL;
        do
        {
            signal_->wait(WAITUNTILWORLDEND);

            do
            {
                mutex_->lock();
                if(dataList_->size()==0)
                {
                    mutex_->unlock();
                    break;
                }
                else
                {
                    DataList::iterator it = dataList_->begin();
                    while(it!=dataList_->end())
                    {
                        data = *it;
                        if(data->type == kOpQuitIoWorker)
                        { //worker out.
                            JDLog("got kOpQuitIoWorker t=%"PRIu64,JinThread::getThreadId());
#ifndef AE_USING_IOCP
                            while(dataList_->size()>1)
                            {
                                it = dataList_->begin();
                                JinAsynData* cancelData = *it;
                                JAssert(cancelData->type != kOpQuitIoWorker);  //only one kOpQuitIoWorker can at queue.
                                dataList_->erase(it);
                                PostQueuedCompletion(portHandle_,-1,(ULONG_PTR)cancelData->aop,(LPOVERLAPPED)cancelData);
                                //只有发生fileOpMutex使对同一handle的操作被搁置，才可能发生。
                                //cancel掉所有op
                            }
#endif
                            mutex_->unlock();
                            signal_->set();  //more woker should notice and quit!
                            JDLog(">worker out.<",NULL);
                            return false;
                        }
#ifndef AE_USING_IOCP
                        if(data->aop->fileOpMutex)
                        {
                            if(!data->aop->fileOpMutex->tryLock())
                            {
                                ++it;
                                data = 0;
                                continue;
                            }
                        }
#endif
                        it = dataList_->erase(it);
                        break;
                    }
                    if(it!=dataList_->end())
                        signal_->set();  //more woker should notice!
                    mutex_->unlock();
                    if(!data) break;
                }

\
                uint32_t postByteTransfer = -1;
                switch(data->type)
                {
#ifndef AE_USING_IOCP
                case kFileRead:
                {
                    if(JinFile::seek(data->handle,*(uint64_t*)&data->overlap.evio)){//data->overlap.offset)){
                        JinFile::read(data->handle,(char*)data->buffer->buff(data->tranSize),data->opSize,postByteTransfer);
                    }
                    //PostQueuedCompletion(portHandle_,bytesTransfer,(ULONG_PTR)aop,(LPOVERLAPPED)data);
                    break;
                }
                case kFileWrite:
                {
                    if(JinFile::seek(data->handle,*(uint64_t*)&data->overlap.evio)){//data->overlap.offset)){
                        JinFile::write(data->handle,(char*)data->buffer->buff(data->tranSize),data->opSize,postByteTransfer);
                    }
                    break;
                }
#endif
                case kFileOpen:
                {
                    char *path = (char*)data->buffer->buff();
                    HdJFile hd;
                    data->opSize = 0;  //期待0 如果返回0则匹配,成功
                    data->tranSize = (uint32_t)JinFile::open(path,(JFAccessFlag)data->opSize,(JFOpenFlag)data->tranSize,hd,true);
                    //data->user64 = (uint64_t)hd;  //aop->obj 可能失效 不能直接给aop->obj->handle
                    data->handle = (HdObject)hd;
                    postByteTransfer = data->tranSize;
                    break;
                }
                case kFileClose:
                {
                    HdJFile hd = (HdJFile)data->handle;
                    data->opSize = 0;
                    data->tranSize = JinFile::close(hd);
                    postByteTransfer = 0;
                    break;
                }
                case kOpQuitIoWorker:
                    JAssert2(false,"data->type kOpQuitIoWorker");
                    break;
                default:
                    JAssert3(false,"error data->type=",&data->type);
                    break;
                }

#ifdef AE_USING_IOCP
                JAssert(data->type!=kFileRead && data->type!=kFileWrite);
                AsynHandle aop = (AsynHandle)data->user64;
#else
                if(data->aop->fileOpMutex)
                {
                    data->aop->fileOpMutex->unlock();
                }
                AsynHandle aop = data->aop;
#endif
                //放后post是避免早post后data被处理释放，后面访问data->aop非法.
                PostQueuedCompletion(portHandle_,postByteTransfer,(ULONG_PTR)aop,(LPOVERLAPPED)data);

            }while(data);
        }
        while(true);
        return false;
    }

private:
    HdPort portHandle_;
    JinMutex* mutex_;
    JinSignaledEvent* signal_;
    DataList* dataList_;
};


JinAsyncEngine::JinAsyncEngine()
    : hdPort_(NULL)
    //, dataPool_(NULL)
    //, workers_(NULL)
    //, workerCount_(0)
    , uninitFlag_(false)
{
    JinNetStartup::Create();
    memset(workers_,0,sizeof(ExtraIOWorker *)*WORKERCOUNT);
}

JinAsyncEngine::~JinAsyncEngine()
{
    uninit();
    JinNetStartup::Free();
    //JDLog(">~JinAsyncEngine done<");
}

void JinAsyncEngine::uninit()
{
    if(hdPort_){
        uninitFlag_ = true;
        //[代码保留]如果这里把队列未做的活取消掉
        //那么这些活没办法完成.这本身就是异常,活做不完就火上浇油了.
        /*extraWorkMutex_->lock();    //所以暂时不动,那么所有投递动作都会做完.
        while(dataList_->size()!=0){
            data = dataList_->front();
            dataList_->pop_front();
            //...more
        }
        extraWorkMutex_->unlock();*/

        //推两个特别的消息给两个线程让他们结束
        JDLog("push and wait worker stop..",NULL);
        JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
        if(data){
            memset(data,0,sizeof(JinAsynData));
            data->type = kOpQuitIoWorker;
            extraWorkMutex_.lock();
            //JDLog("extraWorkList_.push_back(%p)",data);
            extraWorkList_.push_back(data); //推送一个消息，worker线程看到后只退出不删除.
            extraWorkMutex_.unlock();
            extraWorkSignal_.set();
            //JinThread::msleep(500);  //TODO  main t not wait woker safe out!!!
            for(size_t i=0;i<WORKERCOUNT;i++)
            {
                if(workers_[i] == NULL)continue;
                JDLog("JDelete worker %u",i);
                JDelete(workers_[i]);
                JDLog("JDelete worker %u ** after. ",i);
            }
            JAssert(extraWorkList_.size()==1);
            extraWorkList_.clear();
            dataPool_.Release(data,__FILE__,__LINE__);
        }

        while(driveEngine());  //回光返照，处理完所有未尽事宜！
        CloseIoPort(hdPort_);
        hdPort_ = NULL;
    }
}

void JinAsyncEngine::checkOpCountMayDoFree(AsynHandle aop)
{
    if(aop->opCount == 0)
    {
        if(aop->fileOpMutex)   JDelete(aop->fileOpMutex);
        //DropFromPort(aop->obj->handle_,hdPort_);
        //可能已处理掉 aop->obj->handle为SOCKINV（-1）
        DropFromPort(aop->handle,hdPort_);
        JDelete(aop);
    }
}

//void JinAsyncEngine::freeAsynData(JinAsynData *data, const char *file, unsigned int line)
//{
//    switch(data->type)
//    {
//    case kFileOpen:
//    case kSockAccept:
//        if(data->buf) JFree(data->buf);
//        break;
//    default:
//        break;
//    }
//    dataPool_.Release(data,file,line);
//}


bool JinAsyncEngine::init()
{
    if(!JinNetStartup::Get()->valid()) return false;

    if(hdPort_ == NULL)
    {
        if(!extraWorkSignal_.init())return false;
        hdPort_ = CreateIoPort();
        if(hdPort_ == NULL)return false;
        size_t count = WORKERCOUNT;
        for(size_t i=0;i<count;i++)
        {
            workers_[i] = JNew(ExtraIOWorker,hdPort_,&extraWorkMutex_,&extraWorkSignal_,&extraWorkList_);
            //if(workers_[i]==NULL)return false; 可以不成功,全不成功则异步打开文件无法工作
            if(workers_[i]){
                if(!workers_[i]->start()){
                    JDelete(workers_[i]);
                    workers_[i] = NULL;
                }
            }
            if(workers_[i]==NULL){
                --i;--count;  //使成功的往前靠.
            }
        }
    }
    return true;
}

AsynHandle JinAsyncEngine::attach(IAsynObj *obj)
{
    if(uninitFlag_)return NULL;
    JAssert(obj!=NULL);
    if(obj==NULL) return NULL;

    if(obj->aop_ != 0)
    {   //fileopen相当于attach，open成功会调真正的attach 二次attach.
        bool ret = false;
        JDLog("attach twice, be sure only valid for file-open. obj=%"PRIu64,(uint64_t)obj->hfile_);
        JAssert(obj->handle_!=0);
#ifndef AE_USING_IOCP
        obj->aop_->fileOpMutex = JNew(JinMutex);
#endif
        obj->aop_->handle = obj->handle_;
        ret = BindToPort(obj->handle_,hdPort_,(ULONG_PTR)obj->aop_);
        if(ret) return obj->aop_;
        else {
            if(obj->aop_->fileOpMutex)   JDelete(obj->aop_->fileOpMutex);
            JDelete(obj->aop_);
        }
    }
    else
    {
        if(obj->handle_)
        {
            bool ret = false;
            JAssert(obj->aop_ == NULL);
            AsynHandle ao = JNew(JinAsynHandleDetail);
            if(!ao)return NULL;
            memset(ao,0,sizeof(JinAsynHandleDetail));
#ifndef AE_USING_IOCP
            ao->fileOpMutex = JNew(JinMutex);
#endif
            ao->obj = obj;
            ao->handle = ao->obj->handle_;  //增加的目的是延时关闭情况下匹配socket，obj->socket会清掉
            ret = BindToPort(obj->handle_,hdPort_,(ULONG_PTR)ao);
            if(ret) return ao;
            else {
                if(ao->fileOpMutex)   JDelete(ao->fileOpMutex);
                JDelete(ao);
            }
        }
    }
    return NULL;
}

void JinAsyncEngine::detach(AsynHandle aop)
{
    aop->isCancel = true;
    if(aop->handle!=0)
    {
        int countDropped = DropFromPort(aop->handle,hdPort_);
        JDLog("io canceled=%d",countDropped);
    }
    if(aop->opCount == 0)
    {
        if(aop->fileOpMutex)   JDelete(aop->fileOpMutex);
        JDelete(aop);
    }
}

AsynHandle JinAsyncEngine::fileOpen(IAsynObj *obj, const char *path, JFAccessFlag acc, JFOpenFlag ope, void *user)
{
    //这里 fileopen相当于attach，但是没有ao->handle_, ao->mutex，没有BindToPort
    JAssert(obj->aop_ == NULL);
    if(uninitFlag_)return NULL;
    AsynHandle ao = JNew(JinAsynHandleDetail);
    if(!ao)return NULL;
    memset(ao,0,sizeof(JinAsynHandleDetail));
    ao->obj = obj;
    JAssert(obj->handle_==HDNULL);
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data){
        if(ao->fileOpMutex)   JDelete(ao->fileOpMutex);
        JDelete(ao);
        return NULL;
    }
    memset(data,0,sizeof(JinAsynData));
    data->type = kFileOpen;
    size_t pathlen = strlen(path);
    data->buffer = JNew(JinAsynBuffer,pathlen+1);
    if(data->buffer && data->buffer->ref()>0){
        strcpy((char*)data->buffer->buff(),path);
    }
    else{
        if(data->buffer)JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
        return NULL;
    }
    data->opSize = (uint32_t)acc;
    data->tranSize = (uint32_t)ope;
    data->user = user;
#ifdef AE_USING_IOCP
    data->user64 = (uint64_t)ao;
#else
    data->aop = ao;
#endif
    bool ret;
    extraWorkMutex_.lock();
    //JDLog("extraWorkList_.push_back(%p)",data);
    ret = extraWorkList_.push_back(data);
    extraWorkMutex_.unlock();
    if(ret){
        extraWorkSignal_.set();
        ++ ao->opCount;
    }
    else {
        if(data->buffer) JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }

    return ao;
}

bool JinAsyncEngine::fileRead(AsynHandle aop, uint64_t from, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t toRead, void* user)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->handle_!=HDNULL);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifdef AE_USING_IOCP
    data->overlap.Offset = from&0xFFFFFFFF;
    data->overlap.OffsetHigh = (from>>32)&0xFFFFFFFF;
#else
    //data->overlap.offset = from;
    *(uint64_t*)&data->overlap.evio = from;
    data->aop = aop;
#endif
    data->type = kFileRead;
    data->handle = aop->obj->handle_;
    //data->buf = buf;
    data->buffer = JNew(JinAsynBuffer,buffer);
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = toRead;
    data->tranSize = bufPos;
    data->user = user;
    data->user64 = from;
#ifdef AE_USING_IOCP
    bool ret = true;
    if(FALSE == ::ReadFile(aop->obj->handle_,(LPVOID)buffer.buff(bufPos),(DWORD)toRead,NULL,(LPOVERLAPPED)data))
    {
        if(GetLastError()!=ERROR_IO_PENDING) ret = false;
    }
    if(ret){ ++ aop->opCount; }
#else
    bool ret;
    extraWorkMutex_.lock();
    JDLog("extraWorkList_.push_back(%p)",data);
    ret = extraWorkList_.push_back(data);
    extraWorkMutex_.unlock();
    if(ret){
        extraWorkSignal_.set();
        ++ aop->opCount;
    }
#endif
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::fileWrite(AsynHandle aop, uint64_t to, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t toWrite, void* user)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->handle_!=HDNULL);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifdef AE_USING_IOCP
    data->overlap.Offset = to&0xFFFFFFFF;
    data->overlap.OffsetHigh = (to>>32)&0xFFFFFFFF;
#else
    //data->overlap.offset = to;
    *(uint64_t*)&data->overlap.evio = to;
    data->aop = aop;
#endif
    data->type = kFileWrite;
    data->handle = aop->obj->handle_;
    //data->buf = buf;
    data->buffer = JNew(JinAsynBuffer,buffer);
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = toWrite;
    data->tranSize = bufPos;
    data->user = user;
    data->user64 = to;
#ifdef AE_USING_IOCP
    bool ret = true;
    if(FALSE == WriteFile(aop->obj->handle_,(LPCVOID)buffer.buff(bufPos),(DWORD)toWrite,NULL,(LPOVERLAPPED)data))
    {
        if(GetLastError()!=ERROR_IO_PENDING) ret = false;
    }
    if(ret){ ++ aop->opCount; }
#else
    bool ret;
    extraWorkMutex_.lock();
    JDLog("extraWorkList_.push_back(%p)",data);
    ret = extraWorkList_.push_back(data);
    extraWorkMutex_.unlock();
    if(ret){
        extraWorkSignal_.set();
        ++ aop->opCount;
    }
#endif
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::fileClose(AsynHandle aop, void *user)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->handle_!=HDNULL);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));

    data->type = kFileClose;
    data->handle = aop->obj->handle_;
    //data->aop = aop;
    data->user = user;
#ifdef AE_USING_IOCP
    data->user64 = (uint64_t)aop;
#else
    data->aop = aop;
#endif
    bool ret;
    extraWorkMutex_.lock();
    JDLog("extraWorkList_.push_back(%p)",data);
    ret = extraWorkList_.push_back(data);
    extraWorkMutex_.unlock();
    if(ret){
        extraWorkSignal_.set();
        ++ aop->opCount;
    }
    else {
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

#ifdef AE_USING_IOCP
bool JinAsyncEngine::postAccept(AsynHandle aop, HdSocket accSock)
#else
bool JinAsyncEngine::postAccept(AsynHandle aop)
#endif
{
    JDLog("aop=%p count=%d",aop,aop->opCount);
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifdef AE_USING_IOCP
    const DWORD addressLength = sizeof(SOCKADDR_IN) + 16;
    const size_t bufEpAddrLen = addressLength*2;
#else
    const DWORD addressLength = sizeof(struct sockaddr_in);
    const size_t bufEpAddrLen = addressLength;
    data->aop = aop;
#endif
    data->type = kSockAccept;
    data->handle = aop->obj->handle_;
    data->buffer = JNew(JinAsynBuffer,bufEpAddrLen);
    if(data->buffer && data->buffer->ref()>0)
    {
        data->opSize = addressLength;
    }
    else{
        if(data->buffer)JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    //data->buf = (char*)JMalloc(bufEpAddrLen);
#ifdef AE_USING_IOCP
    data->user = (void*)accSock;
#else
    data->user = 0;
#endif
    //data->user64 = from;
    bool ret = true;
#ifdef AE_USING_IOCP
    //AcceptEx函数把两端的地址放入buf,后续需要另一函数来解析获得.
    //提供的buffer要能放第一块数据和2块地址,4参表示收第一块数据的大小，5，6参表示留给放地址空间
    //所以buffer的总空间应该是3,4,5参之和
    if(FALSE == winAcceptEx(aop->obj->hsocket_,accSock,
                            data->buffer->buff(),0, //(DWORD)bufEpAddrLen,
                            addressLength, addressLength,
                            NULL,(LPOVERLAPPED)data))
    {
        if(WSAGetLastError()!=ERROR_IO_PENDING) ret = false;
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){ ++ aop->opCount; }
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postConnect(AsynHandle aop, uint32_t rawIP, uint16_t rawPort)
{
    JDLog("aop=%p rawIP=%x rawIP=%hu",aop,rawIP, rawPort);
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockConnect;
    data->handle = aop->obj->handle_;
    data->user = (void*)(uintptr_t)rawPort;
    data->user64 = (uint64_t)rawIP;
    bool ret = true;
#ifdef AE_USING_IOCP
    sockaddr_in tarAddr;
    tarAddr.sin_family = AF_INET;
    tarAddr.sin_addr.s_addr = rawIP;
    tarAddr.sin_port = rawPort;
    JAssert(sizeof(sockaddr)==sizeof(sockaddr_in));
    if(FALSE == winConnectEx(aop->obj->hsocket_,(sockaddr*)&tarAddr,sizeof(sockaddr),(LPOVERLAPPED)data))
    {
        if(WSAGetLastError() != ERROR_IO_PENDING){ ret = false;  }
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){ ++ aop->opCount; }
    else {
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postDisconnect(AsynHandle aop)
{
    JDLog("aop=%p",aop);
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockDisconnect;
    data->handle = aop->obj->handle_;
    bool ret = true;

#ifdef AE_USING_IOCP
    if(FALSE == winDisconnectEx(aop->obj->hsocket_,(LPOVERLAPPED)data,0,0))
    {
        if(WSAGetLastError() != ERROR_IO_PENDING){ ret = false;  }
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){ ++ aop->opCount; }
    else {
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postSend(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteToSend)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockSend;
    data->handle = aop->obj->handle_;
    data->buffer = JNew(JinAsynBuffer,buffer);
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = byteToSend;
    data->tranSize = bufPos;
    bool ret = true;
#ifdef AE_USING_IOCP
    WSABUF wbuf;
    wbuf.buf = (char*)data->buffer->buff(bufPos);
    wbuf.len = byteToSend;
    if(SOCKET_ERROR == WSASend(aop->obj->hsocket_,&wbuf,1,NULL,0,(LPOVERLAPPED)data,NULL))
    {
        if(WSAGetLastError()!=WSA_IO_PENDING){ ret = false; }
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){ ++ aop->opCount; }
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postRecv(AsynHandle aop, JinAsynBuffer& buffer, const uint32_t bufPos, uint32_t byteMaxRecv)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockRecv;
    data->handle = aop->obj->handle_;
    data->buffer = JNew(JinAsynBuffer,buffer);
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = byteMaxRecv;
    data->tranSize = bufPos;
    bool ret = true;
#ifdef AE_USING_IOCP
    WSABUF wbuf;
    wbuf.buf = (char*)data->buffer->buff(bufPos);
    wbuf.len = byteMaxRecv;
    DWORD dwFlag = 0;
    if(SOCKET_ERROR == WSARecv(aop->obj->hsocket_,&wbuf,1,NULL,&dwFlag,(LPOVERLAPPED)data,NULL))
    {
        if(WSAGetLastError()!=WSA_IO_PENDING){ ret = false; }
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){ ++ aop->opCount; }
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postSendTo(AsynHandle aop, JinAsynBuffer &buffer, const uint32_t bufPos, uint32_t byteToSend,
                                uint32_t rawIP, uint16_t rawPort)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockSendTo;
    data->handle = aop->obj->handle_;
    data->buffer = JNew(JinAsynBuffer,buffer);
    //JAssert(data->buffer->ref()==2);  //debug
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = byteToSend;
    data->tranSize = bufPos;
    data->user = (void*)(uintptr_t)rawPort;
    data->user64 = (uint64_t)rawIP;
    bool ret = true;
#ifdef AE_USING_IOCP
    sockaddr_in tarAddr;
    tarAddr.sin_family = AF_INET;
    tarAddr.sin_addr.s_addr = rawIP;
    tarAddr.sin_port = rawPort;
    //int iByte = (int) sizeof(sockaddr);
    JAssert(sizeof(sockaddr)==sizeof(sockaddr_in));
    WSABUF wbuf;
    wbuf.buf = (char*)data->buffer->buff(bufPos);
    wbuf.len = byteToSend;
    if(SOCKET_ERROR == WSASendTo(aop->obj->hsocket_,&wbuf,1,NULL,0,
                                 (sockaddr*)&tarAddr,(int) sizeof(sockaddr),
                                 (LPOVERLAPPED)&data->overlap,NULL))
    {
        if(WSAGetLastError()!=WSA_IO_PENDING){ ret = false; }
    }
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){
        ++ aop->opCount; //JDLog("++opCount=%d",aop->opCount); //debug
    }
    else {
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::postRecvFrom(AsynHandle aop, JinAsynBuffer &buffer, const uint32_t bufPos, uint32_t byteMaxRecv)
{
    JAssert(!aop->isCancel);
    JAssert(aop->obj->hsocket_!=SOCKINV);
    if(uninitFlag_)return false;
    int iByte = (int) sizeof(sockaddr);   //末尾放地址，不要越界哟.
    JAssert((int)byteMaxRecv>iByte);
    if((int)byteMaxRecv<=iByte) return false;
    JinAsynData* data = dataPool_.Allocate(__FILE__,__LINE__);
    if(!data)return false;
    memset(data,0,sizeof(JinAsynData));
#ifndef AE_USING_IOCP
    data->aop = aop;
#endif
    data->type = kSockRecvFrom;
    data->handle = aop->obj->handle_;
    data->buffer = JNew(JinAsynBuffer,buffer);
    if(!data->buffer){
        dataPool_.Release(data,__FILE__,__LINE__);
        return false;
    }
    data->opSize = byteMaxRecv;
    data->tranSize = bufPos;
    bool ret = true;
#ifdef AE_USING_IOCP
    WSABUF wbuf;
    wbuf.buf = (char*)data->buffer->buff(bufPos);
    wbuf.len = byteMaxRecv-(uint32_t)iByte;
    sockaddr* sa = (sockaddr*)(data->buffer->buffc(bufPos+wbuf.len));
    DWORD dwFlag = 0;
    do
    {
        if(SOCKET_ERROR == WSARecvFrom(aop->obj->hsocket_,&wbuf,1,NULL,&dwFlag,
                                       sa,&iByte,(LPOVERLAPPED)data,NULL))
        {
            int last = WSAGetLastError();
            if(last == WSA_IO_PENDING)
            {    break;   }
            if(last == WSAECONNRESET || last == WSAENETRESET)
            {
                JELog("abide WSAGetLastError=%d",last);
                continue;
            }
            else
            {
                ret = false;
                break;
            }
        }
    }while(!ret);
#else
    ret = (evMixIO(hdPort_,data)==0);
#endif
    if(ret){
        ++ aop->opCount;  //JDLog("++opCount=%d.",aop->opCount);
    }
    else {
        JELog("recv error:%d",GetErrorCode());
        JDelete(data->buffer);
        dataPool_.Release(data,__FILE__,__LINE__);
    }
    return ret;
}

bool JinAsyncEngine::driveEngine(uint32_t timeout)
{
    bool ret = false;
    uint32_t bytesTransfer = 0;
    AsynHandle aop = NULL;
    JinAsynData* data = NULL;
    bool opSucc = true;

    //GetQueuedCompletionStatus 最后参数为0 不阻塞。
    ret = GetQueuedCompletion(hdPort_,&bytesTransfer,(PULONG_PTR)&aop,
                              (LPOVERLAPPED*)&data,timeout);
    if(ret == false)
    {
        //If *lpOverlapped is NULL, the function did not dequeue a completion packet from the completion port.
        // In this case, the function does not store information in the variables pointed to by the
        // lpNumberOfBytes and lpCompletionKey parameters, and their values are indeterminate.
        if(data == NULL)
        {   //If the function did not dequeue a completion packet because the wait timed out, GetLastError returns
            // WAIT_TIMEOUT.
            if(timeout==INFINITE){
                int se = GetErrorCode();
                JELog("Engine halt, last error:%d",se);
                JAssert(uninitFlag_);
            }
            JAssert(bytesTransfer==0);
            return false;
        }
        //If *lpOverlapped is not NULL and the function dequeues a completion packet for a failed I/O operation from
        // the completion port, the function stores information about the failed operation in the variables
        // pointed to by lpNumberOfBytes, lpCompletionKey, and lpOverlapped. To get extended error information,
        // call GetLastError.
        else
        {
            //UDP recvFrom的时候遭遇 bytesTransfer=0 GetLastError=1234.
            //ERROR_PORT_UNREACHABLE    1234 (0x4D2)   No service is operating at the destination network endpoint on the remote system.
            //目前的理解推测是目标明确表示拒绝,例如对方端口根本没开(开了但现在关了)的时候.
            JDLog("Occur error with socket: bytesTransfer=%u ecode=%d",bytesTransfer,GetErrorCode(false));
            opSucc = false;
        }
    }
    if(aop==NULL)return true;//break;  //某种原因收到了自己post的退出信号。
    JAssert(aop && data);
    --aop->opCount;
    //JDLog("--opCount=%d",aop->opCount);
    JAssert(aop->opCount>=0);
    //data->aop = aop;
    void* tranBuff = data->buffer ?
                data->buffer->buff(data->tranSize) : NULL;
    data->tranSize = bytesTransfer;
    if(!aop->isCancel)
    {
        //if(data->type == kFileOpen){  //特殊!
        //    aop->obj->hfile_ = (HdFile)data->user64;
        //}
        //由于有些操作（例如open）是用Extra线程做的，对于GetQueuedCompletion来说确实是操作成功.
        //opSucc为true，所以最后以双条件判定
        aop->obj->OnMainPull(opSucc&&(data->opSize==data->tranSize),
                             data->handle, data->type, tranBuff, data->opSize,
                             data->tranSize, data->user, data->user64);
    }
    else{
        JDLog(" {isCancel} %"PRIu64,(unsigned long long)aop->handle);
        //checkOpCountMayDoFree(aop);

        if(aop->opCount == 0)
        {
            if(aop->fileOpMutex)   JDelete(aop->fileOpMutex);
            JDelete(aop);
        }
    }
    if(data->buffer)
    {
        JDelete(data->buffer);
        data->buffer = NULL;
    }
#ifdef JDEBUG
    memset(data,0,sizeof(JinAsynData));
#endif
    dataPool_.Release(data,__FILE__,__LINE__);
    //freeAsynData(data,__FILE__,__LINE__);
    return true;
}

void JinAsyncEngine::awakePost()
{
    PostQueuedCompletion(hdPort_,0,0,NULL);
}


bool initAsyncEngine()
{
    JinAsyncEngine *ae = AE::Create();
    if(ae==0)return false;
    return ae->init();
}

bool driveAEngine(uint32_t timeout)
{
    return AE::Get()->driveEngine(timeout);
}

void uninitAsyncEngine()
{
    AE::Get()->uninit();
    AE::Free();
}

void awakeAEngine()
{
    AE::Get()->awakePost();
}

int GetErrorCode(bool issocket)
{
#ifdef JWIN
    if(issocket) return (int)WSAGetLastError();
    else return (int)GetLastError();
#else
    UNUSED(issocket);
    return errno;
#endif
}
#ifndef JWIN
int closesocket(HdSocket sock)
{
    return close(sock);
}

#endif
#ifdef JWIN
BOOL winConnectEx(
        _In_      SOCKET s,
        _In_      const struct sockaddr *name,
        _In_      int namelen,
        _In_      LPOVERLAPPED lpOverlapped)
{
    static LPFN_CONNECTEX lpfnConnectEx = NULL;
    DWORD  dwByte = 0;
    if(lpfnConnectEx == NULL)
    {
        GUID   GuidConnectEx = WSAID_CONNECTEX;
        if(SOCKET_ERROR ==  WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &GuidConnectEx, sizeof(GuidConnectEx),
                                     &lpfnConnectEx, sizeof(lpfnConnectEx),
                                     &dwByte, NULL, NULL))
        {
            JDLog("get ConnectEx failed: %d",(int)WSAGetLastError());
            return FALSE;
        }
    }

    return lpfnConnectEx(s,name,namelen,NULL,0,&dwByte,lpOverlapped);
}

BOOL winDisconnectEx(
        _In_  SOCKET hSocket,
        _In_  LPOVERLAPPED lpOverlapped,
        _In_  DWORD dwFlags,
        _In_  DWORD reserved)
{
    static LPFN_DISCONNECTEX lpfnDisconnectEx = NULL;
    DWORD  dwByte = 0;
    if(lpfnDisconnectEx == NULL)
    {
        GUID   GuidDisconnectEx = WSAID_DISCONNECTEX;
        if(SOCKET_ERROR ==  WSAIoctl(hSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &GuidDisconnectEx, sizeof(GuidDisconnectEx),
                                     &lpfnDisconnectEx, sizeof(lpfnDisconnectEx),
                                     &dwByte, NULL, NULL))
        {
            JDLog("get DisconnectEx failed: %d",(int)WSAGetLastError());
            return FALSE;
        }
    }

    return lpfnDisconnectEx(hSocket,lpOverlapped,dwFlags,reserved);
}

BOOL winAcceptEx(
        _In_   SOCKET sListenSocket,
        _In_   SOCKET sAcceptSocket,
        _In_   PVOID lpOutputBuffer,
        _In_   DWORD dwReceiveDataLength,
        _In_   DWORD dwLocalAddressLength,
        _In_   DWORD dwRemoteAddressLength,
        _Out_  LPDWORD lpdwBytesReceived,
        _In_   LPOVERLAPPED lpOverlapped)
{
    static LPFN_ACCEPTEX lpfnAcceptEx = NULL;
    DWORD  dwByte = 0;
    if(lpfnAcceptEx == NULL)
    {
        GUID   GuidAcceptEx = WSAID_ACCEPTEX;
        if(SOCKET_ERROR ==  WSAIoctl(sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &GuidAcceptEx, sizeof(GuidAcceptEx),
                                     &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                                     &dwByte, NULL, NULL))
        {
            JDLog("get AcceptEx failed: %d",(int)WSAGetLastError());
            return FALSE;
        }
    }

    return lpfnAcceptEx(sListenSocket,sAcceptSocket,lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,
                        dwRemoteAddressLength,lpdwBytesReceived,lpOverlapped);
}

//如果使用WSAIoctl获取AcceptEx,那么相应的GetAcceptExSockaddrs也必须是获取而来.
//http://blog.chinaunix.net/uid-9332282-id-1748173.html
void winGetAcceptExSockaddrs(SOCKET sListenSocket,
                             PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
                             DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
                             sockaddr **LocalSockaddr, LPINT LocalSockaddrLength,
                             sockaddr **RemoteSockaddr, LPINT RemoteSockaddrLength)
{
    static LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;
    DWORD dwByte = 0;
    if(lpfnGetAcceptExSockaddrs == NULL)
    {
        GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        if(SOCKET_ERROR ==  WSAIoctl(sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                     &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
                                     &lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs),
                                     &dwByte, NULL, NULL))
        {
            JDLog("get GetAcceptExSockaddrs failed: %d",(int)WSAGetLastError());
            return;
        }
    }

    return lpfnGetAcceptExSockaddrs(lpOutputBuffer, dwReceiveDataLength,
                                    dwLocalAddressLength, dwRemoteAddressLength,
                                    LocalSockaddr, LocalSockaddrLength,
                                    RemoteSockaddr, RemoteSockaddrLength);
}

#endif
