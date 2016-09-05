#include "jinmemorycheck.h"
#ifdef JDEBUG
//#include "jinformatint.h"
#include <map>
#include "jinassert.h"
#include "jinmutex.h"
#include "jinhashmap.h"
//using namespace std;
#include "jinlogger.h"

struct AllocInfo
{
    size_t len;
    const char *file;
    unsigned int line;
};
bool gMemoryCheckOnWorking = false;
typedef  std::map<void*,AllocInfo>  LeakMapType;
class JinMemoryCheck
{
public:
    JinMemoryCheck()
    {
        //leakMap_.init(65536);
        leakMap_.clear();
        gMemoryCheckOnWorking = true;
    }

    ~JinMemoryCheck()
    {
        this->output();
        gMemoryCheckOnWorking = false;
    }


    void onAlloc(void *alloced, size_t size, const char *file, unsigned int line)
    {
        AllocInfo ai;
        ai.len = size;
        ai.file = file;
        ai.line = line;
        mutex_.lock();
        leakMap_[alloced] = ai; //leakMap_.set(alloced,ai);
        mutex_.unlock();
    }

    void onFree(void *alloced, const char *file, unsigned int line)
    {
        LeakMapType::iterator it;
        mutex_.lock();
        it = leakMap_.find(alloced);
        JAssert4(it!=leakMap_.end(),file,line);
        leakMap_.erase(it);
        mutex_.unlock();
        //if(it!=leakMap_.end()){
        //    leakMap_.erase(it);
        //}
    }

    size_t output(size_t* count=0)
    {
        size_t leakTotal = 0;
        LeakMapType::iterator it = leakMap_.begin();
        while(it!=leakMap_.end())
        {
            //AllocInfo ai = it.second();
            AllocInfo& ai = it->second;
            leakTotal += ai.len;
            JSLog("leak:%s:%u(%"PRISIZE") p=%p",ai.file,ai.line,ai.len,it->first);
            ++it;
        }

        if(leakMap_.size()>0){
            JSLog("\n**************************************"
                  "\n* %"PRISIZE" bytes memory leak detected."
                  "\n**************************************",leakTotal);
        }
        else{
            JSLog("\n**************************************"
                  "\n* no memory leak detected!"
                  "\n**************************************");
        }
        if(count)*count = leakMap_.size();
        return leakTotal;
    }

private:
    LeakMapType leakMap_;
    JinMutex mutex_;
};

JinMemoryCheck globalMemoryCheck;
void* jinAlloc(void *alloced, size_t size, const char *file, unsigned int line)
{
    if(!gMemoryCheckOnWorking)return alloced;
    if(alloced){
        globalMemoryCheck.onAlloc(alloced,size,file,line);
    }
    return alloced;
}
void jinFree(void *tobefree, const char *file, unsigned int line)
{
    if(!gMemoryCheckOnWorking)return;
    globalMemoryCheck.onFree(tobefree,file,line);
}

bool __dumpWithExpect(size_t leakCount,size_t leakBytes)
{
    size_t count,bytes;
    bytes = globalMemoryCheck.output(&count);
    return (count==leakCount && bytes == leakBytes);
}
#endif
