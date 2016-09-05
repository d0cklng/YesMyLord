#include "clientinfoallocator.h"
#include "jinassert.h"
#include "jinmemorycheck.h"
#include "jinlogger.h"

ClientInfo::ClientInfo()
{
    state = kConn;
    id = NETID_INVALID;
    firstGlance = 0;
    more = NULL;
#ifdef JDEBUG
    flagOnUse = false;
#endif
}

ClientInfo::~ClientInfo()
{

}

ClientInfoAllocator::ClientInfoAllocator(size_t enoughSet, size_t dropSet)
    : enoughSet_(enoughSet)
    , dropSet_(dropSet)
{
    JAssert(enoughSet<dropSet);
}

ClientInfoAllocator::~ClientInfoAllocator()
{
    JinLinkedList<ClientInfo*>::iterator it = cached_.begin();
    while(it != cached_.end())
    {
        ClientInfo* info = *it;
        ++it;
        JDelete(info);
    }
    cached_.clear();
}

ClientInfo *ClientInfoAllocator::alloc()
{
    ClientInfo *info = NULL;
    if(cached_.size())
    {
        info = cached_.back();
        cached_.pop_back();
    }
    else
    {
        info = JNew(ClientInfo);
        info->more = NULL;
    }
#ifdef JDEBUG
    if(info){
        JAssert(info->flagOnUse == false);
        info->flagOnUse = true;
    }
#endif
    return info;
}

void ClientInfoAllocator::back(ClientInfo *info)
{
#ifdef JDEBUG
    JAssert(info->flagOnUse == true);
    info->flagOnUse = false;
#endif
    JAssert(info->more == NULL);  //提醒要删除more.
    cached_.push_back(info);
    JDLog("cached size=%"PRISIZE,cached_.size());
    if(cached_.size() > dropSet_)
    {
        while(cached_.size() > enoughSet_)
        {
            ClientInfo *info = cached_.front();
            cached_.pop_front();
            JDelete(info);
        }
    }
}


