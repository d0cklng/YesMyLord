#ifndef CLIENTINFOALLOCATOR_H
#define CLIENTINFOALLOCATOR_H
#include "jinnetaddress.h"
#include "jindatatime.h"
#include "INetEngine.h"
#include "jinlinkedlist.h"

#ifndef ClientInfoMoreType
typedef void *InfoMoreType;
#define ClientInfoMoreType
#endif

class ClientInfo
{
public:
    enum State{kConn,kLogin} state;
    NetID id;
    JinNetAddr fromAddr;
    JinNetPort fromPort;
    T64 firstGlance;  //first contact time
    InfoMoreType more;
private:
    friend class ClientInfoAllocator;
    ClientInfo();
    ~ClientInfo();
#ifdef JDEBUG
    bool flagOnUse;
#endif
};

//考虑客户端连入和离开，可能频繁创建和销毁clientInfo结构，
//所以Allocator用于将没用的info先留一下，减少创建次数
class ClientInfoAllocator
{
public:
    //大于dropSet 时清理保留enoughSet
    ClientInfoAllocator(size_t enoughSet=100, size_t dropSet=200);
    ~ClientInfoAllocator();
    ClientInfo* alloc();
    void back(ClientInfo* info);
private:
    size_t enoughSet_;
    size_t dropSet_;
    JinLinkedList<ClientInfo*> cached_;
};

#endif // CLIENTINFOALLOCATOR_H
