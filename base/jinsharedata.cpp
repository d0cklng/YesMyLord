#include "jinsharedata.h"
//#include "jinalloc.h"
#include "jinmemorycheck.h"

#include "jinmutex.h"

JinMutex gShareDataLock;

JinShareData::JinShareData()
    : pref(0)
{
    pref = (int32_t*)JMalloc(sizeof(int32_t));
    //pref = (int32_t*)jMalloc(sizeof(int32_t),_FILE_AND_LINE_);
    if(pref){ *pref = 1; }
}

JinShareData::~JinShareData()
{
    if(pref){ JFree(pref); }
    //jFree(pref,_FILE_AND_LINE_);
}

bool JinShareData::attach()
{
    gShareDataLock.lock();
    if(pref){ ++ *pref; gShareDataLock.unlock();return true;}
    else{ gShareDataLock.unlock(); return false; }
}

void JinShareData::detach()
{
    gShareDataLock.lock();
    if(pref){
        if(-- *pref == 0){
            JDelete(this);
        }
    }
    else{
        JDelete(this);
    }
    gShareDataLock.unlock();
}

int32_t JinShareData::ref()
{
    return *pref;
}
