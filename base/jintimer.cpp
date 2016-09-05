#include "jintimer.h"
#include "jindatatime.h"
#include <string.h>

#define NODE_ADD(node1, node2)	(((node1) + (node2)) % cycleNodes_)
#define NODE_SUB(node1, node2)	(((node1) + cycleNodes_ - (node2)) % cycleNodes_)

#ifdef _DEBUG
#define CHECK_LOOP(node) {if(node){CycleNode *tmp = node->next; for(;tmp;tmp = tmp->next) JAssert(tmp != node);} }
#else
#define CHECK_LOOP(node)
#endif

JinTimer::JinTimer(int pridict, int cyclePeriod, int granularity)
    : granularity_(granularity)
    , cycleNodes_(cyclePeriod/granularity)
    , serial_(INVALIDTIMERID)
    , nodes_(NULL)
    , lastNode_(0)
    , distance_(0)
    , curTimespot_(0)
{
    //timers_.init(pridict); //TODO
    memPool_.SetPageSize(sizeof(struct TimerX)*pridict*2);
    nodes_ = JNewAry(CycleNode*,cycleNodes_);
    memset(nodes_, 0, sizeof(CycleNode*) * cycleNodes_);
    this->curTimespot_ = JinDateTime::tickCount();
    //this->threadID_ = sd_get_self_taskid();
}

JinTimer::~JinTimer()
{
    //force cancel all running timer
    // 不取消也不会泄露。因为mempool会回收他们.
    //OR just assert?!
    // JAssert(timers_.size()==0);

    //本该关timer前干掉所有timer.到这里还没关的,不会再触发了.
    std::map<TimerID,TriggerMsg*>::iterator it;
    while(timeout_.size())
    {
        it = timeout_.begin();
        TriggerMsg* msg = it->second;
        memPool_.Release(msg,__FILE__,__LINE__);
        timeout_.erase(it);
    }
    while(timers_.size())
    {
        it = timers_.begin();
        TriggerMsg* msg = it->second;
        timers_.erase(it);
        bool ret = eraseTimer(msg);
        JAssert2(ret,"EraseTimer");
        memPool_.Release(msg,__FILE__,__LINE__);
    }

    JDelAry(nodes_);
}

bool JinTimer::insert(uint32_t timeout, TriggerMsg *data)
{
    //保证精度
    uint64_t node_inv = (timeout + curTimespot_ % granularity_) / granularity_;            // 经过多少定时器周期定时器被触发.

    /* calculate from last_node */
    uint64_t layer = (distance_ + node_inv) / cycleNodes_;                                 // 定时器在时间轮轮转多少圈之后触发.
    int32_t node_index = (lastNode_ + distance_ + node_inv) % cycleNodes_;     // 计算定时器插入时间轮的那个槽中,这个是根据什么插入的.

    /* find its node */
    CycleNode *pnode = nodes_[node_index];
    CycleNode *plastnode = nodes_[node_index];

    //CHECK_LOOP(nodes_[node_index]);
    //LOG_DEBUG("timer-item(timeout: %u, node_inv: %llu), ready to find a fit position(last_node: %llu, distance: %llu, layer: %llu, node_index: %llu",
    //          timeout, node_inv, lastNode_, distance_, layer, node_index);


    // 让超时越长的定时器排在后面.
    while (pnode && (layer > pnode->interval))
    { //判断layer更大更深要逐层深入.
        plastnode = pnode;
        layer -= pnode->interval;
        pnode = pnode->next;
    }

    CycleNode *pnewnode = NULL;  //下面计算具体的停留node，没有的话要创造.
    if (pnode && (layer == pnode->interval))
    {
        pnewnode = pnode;       // 找到已存在的node，载荷要水平排在这个node里了.
    }
    else/* !pnode || layer < pnode->interval */
    {
        pnewnode = memPool_.Allocate(__FILE__,__LINE__);
        if(pnewnode==NULL)return false;
        memset(pnewnode,0,sizeof(TimerX));
        pnewnode->interval =(uint32_t) layer;
        if (pnode)
        {
            pnode->interval -= (uint32_t) layer;      // 插入一个定时器之后，后面定时器占领的时间长度需要剪短，让后面插入的定时器正确排序.
        }
        if (plastnode != pnode)
        {
            plastnode->next = pnewnode;
        }
        else /* parent of new-node is g_timer[node_index] */
        {
            nodes_[node_index] = pnewnode;         // 槽中没有任何定时器.
        }

        pnewnode->next = pnode;
    }

    CHECK_LOOP(nodes_[node_index]);

    if(pnewnode->infoListHead)
    {
        pnewnode->infoListTail->next = data;
        pnewnode->infoListTail = data;
    }
    else
    {
        pnewnode->infoListHead = data;
        pnewnode->infoListTail = data;
    }

    data->hindex = node_index;

    return true;
}

size_t JinTimer::popExpire(int32_t index, int32_t layer)
{
    size_t ret_val = 0;
    CycleNode *pnode = nodes_[index];
    CycleNode *last_node = NULL;

    CHECK_LOOP(nodes_[index]);
    while (pnode)
    {
        if ((pnode->interval > (uint32_t)layer)
                || ((layer > 0) && (pnode->interval == (uint32_t)layer)))
        {
            pnode->interval -= layer;
            break;
        }

        layer -= pnode->interval;

        /* timeout */
        TriggerMsg* msg = pnode->infoListHead;
        while(msg)
        {
            ++ret_val;
            timers_.erase(msg->id);  //转交给timeout_
            pnode->infoListHead = msg->next;   //这行和下一行是为了正确数据，在autoRestart重用msg的时候有意义.
            if(pnode->infoListHead==NULL)pnode->infoListTail=NULL;
            msg->next = NULL;
            timeout_[msg->id] = msg;
            msg = pnode->infoListHead;
        }

        last_node = pnode;
        pnode = pnode->next;
        memPool_.Release(last_node,__FILE__,__LINE__);
    }

    nodes_[index] = pnode;
    CHECK_LOOP(nodes_[index]);

    return ret_val;
}

size_t JinTimer::popAllExpire()
{
    /* caculate layer */
    int64_t layer = distance_ / cycleNodes_;                                // 当前是第几个轮转周期.
    int64_t cur_idx = (lastNode_ + distance_) % cycleNodes_;     // 当前指向哪个槽.
    int64_t count = NODE_SUB(cur_idx, lastNode_);                         // 两个槽之间的距离.


    //LOG_DEBUG("try to pop expire timer:  last_node: %llu, distance: %llu, layer: %llu", lastNode_, distance_, layer);

    size_t ret_val = 0;
    int32_t i = 0;
    int32_t index = 0;
    for (i = 0; i < count; i++)
    {
        index = (int32_t) (lastNode_ + i) % cycleNodes_;
        ret_val += popExpire(index, (int32_t)layer + 1);
    }

    count = cycleNodes_ - count;
    if (layer > 0)
    {
        for (i = 0; i < count; i++)
        {
            index = (int32_t) (cur_idx + i) % cycleNodes_;
            ret_val += popExpire(index, (int32_t)layer);
        }
    }
    else
    {/* layer == 0, only pop current node, need not to travel all left node */
        ret_val += popExpire((int32_t) cur_idx, 0);
    }

    /* skip g_cur_node becase it has been popped */
    lastNode_ = cur_idx;
    distance_ = 0;

    return ret_val;
}

bool JinTimer::eraseTimer(TriggerMsg *msg)
{
    //JAssert((hindex >= 0) && (hindex < cycleNodes_));
    int32_t hindex = msg->hindex;
    CHECK_LOOP(nodes_[hindex]);
    CycleNode *pnode = nodes_[hindex];
    CycleNode *plastnode = pnode;
    while(pnode)
    {
        TriggerMsg* lastMsgNode = NULL;
        TriggerMsg* msgNode = pnode->infoListHead;
        while(msgNode)
        {
            if(msgNode == msg)
            {
                if(lastMsgNode)lastMsgNode->next = msgNode->next;
                if(msgNode == pnode->infoListHead) pnode->infoListHead = msgNode->next;
                if(msgNode == pnode->infoListTail) pnode->infoListTail = lastMsgNode;
                msgNode->next = NULL;
                CHECK_LOOP(nodes_[hindex]);
                if(pnode->infoListHead == NULL)  //node已没有数据，销毁node
                {
                    JAssert(pnode->infoListHead == pnode->infoListTail);
                    if (pnode == nodes_[hindex])
                    {
                        nodes_[hindex] = pnode->next;
                        if (nodes_[hindex])
                        {
                            nodes_[hindex]->interval += pnode->interval;
                        }
                    }
                    else
                    {
                        plastnode->next = pnode->next;
                        if (plastnode->next)
                        {
                            plastnode->next->interval += pnode->interval;
                        }
                    }
                    memPool_.Release(pnode,__FILE__,__LINE__);
                }
                CHECK_LOOP(nodes_[hindex]);
                return true;
            }
            lastMsgNode = msgNode;
            msgNode = msgNode->next;
        }
        plastnode = pnode;
        pnode = pnode->next;
    }

    return false;
}

//int32_t JinTimer::erase_from_timer_with_timeout(void *comparator_data, data_comparator comp_fun, void **data)
//{
//    LIST_ITERATOR list_it = LIST_BEGIN(m_infinite_list);
//    for (; list_it != LIST_END(m_infinite_list); list_it = LIST_NEXT(list_it))
//    {
//        if (comp_fun(comparator_data, LIST_VALUE(list_it)) == 0)
//        {
//            if (data)
//            {
//                *data = LIST_VALUE(list_it);
//            }
//            list_erase(&m_infinite_list, list_it);
//            return SUCCESS;
//        }
//    }
//    return INVALID_TIMER_INDEX;
//}

//int32_t JinTimer::erase_from_timer_with_all_index(void *comparator_data, data_comparator comp_fun, void **data)
//{
//    int32_t ret_val = INVALID_TIMER_INDEX;
//    int32_t idx = 0;
//    for (idx = 0; idx < cycleNodes_; idx++)
//    {
//        ret_val = erase_from_timer_with_valid_index(comparator_data, comp_fun, idx, data);
//        if (SUCCESS == ret_val)
//        {
//            if (data && (*data))
//            {
//                break;
//            }
//        }
//    }

//    if (cycleNodes_ == idx)
//    {
//        ret_val = erase_from_timer_with_timeout(comparator_data, comp_fun, data);
//    }

//    return ret_val;
//}

//int32_t JinTimer::erase_from_timer(void *comparator_data, data_comparator comp_fun, int32_t timer_index, void **data)
//{
//    int32_t ret_val = INVALID_TIMER_INDEX;

//    if (data)
//    {
//        *data = NULL;
//    }

//    if ((timer_index >= 0) && (timer_index < cycleNodes_))
//    {
//        ret_val = erase_from_timer_with_valid_index(comparator_data, comp_fun, timer_index, data);
//    }
//    else if (timer_index == kTimerIndexNone)
//    {
//        ret_val = erase_from_timer_with_all_index(comparator_data, comp_fun, data);
//    }
//    else if (timer_index == kTimerIndexInfinite)
//    {
//        ret_val = erase_from_timer_with_timeout(comparator_data, comp_fun, data);
//    }

//    return ret_val;
//}

//uint32_t get_current_timestamp()
//{
//	return curTimespot_;
//}

#include <stdio.h>
void JinTimer::refresh()
{
    uint64_t last_spot = curTimespot_;
    curTimespot_ = JinDateTime::tickCount();
    //printf("  %llu  ",curTimespot_);

    if (curTimespot_ < last_spot) {return;}
    uint64_t node_inv = (curTimespot_ - last_spot + last_spot % granularity_) / granularity_;      //距离上次有多少个定时器周期.
    distance_ += node_inv;

    //if(node_inv > 100){
        //char err_msg[128];
        //sprintf(err_msg,"Warning: time interval=%llu between twice 'poll_timer' is too long. by zeyu.",
        //        (curTimespot_ - last_spot));
        //LOG_ERROR(err_msg);
    //}
    //LOG_DEBUG("refresh timer:  last_spot: %llu, cur_spot: %llu, node_inv: %llu, distance: %llu",
    //last_spot, curTimespot_, node_inv, distance_);
    return;
}

TimerID JinTimer::start(TriggerMsg* tmsg)
{
    if(!insert(tmsg->interval, tmsg))
    {
        memPool_.Release(tmsg,__FILE__,__LINE__);
        return INVALIDTIMERID;
    }
    JAssert(timers_.find(tmsg->id) == timers_.end());
    timers_[tmsg->id] = tmsg;
    return tmsg->id;
}

TimerID JinTimer::start(uint32_t interval, bool autoRestart, timeout_callback cb, void *user, uint64_t user64)
{
    //JAssert(sd_get_self_taskid() == threadID_);

    TriggerMsg* tmsg = NULL;
    tmsg = memPool_.Allocate(__FILE__,__LINE__);
    if(tmsg==NULL)return INVALIDTIMERID;
    memset(tmsg,0,sizeof(TimerX));

    tmsg->id = ++serial_;
    tmsg->interval = interval;
    tmsg->user = user;
    tmsg->user64 = user64;
    tmsg->callback = cb;
    tmsg->autoRestart = autoRestart;

    return start(tmsg);
}

bool JinTimer::cancel(TimerID id)
{
    TriggerMsg *tmsg = NULL;
    //LOG_DEBUG("timer:%p, timer_id:%llu cancel...", this, id);

    std::map<TimerID,TriggerMsg*>::iterator it = timers_.find(id);
    if(it != timers_.end())
    {
        tmsg = it->second;
        timers_.erase(it);
        bool ret = eraseTimer(tmsg);
        JAssert2(ret,"EraseTimer");
        memPool_.Release(tmsg,__FILE__,__LINE__);
    }
    else
    {
        it = timeout_.find(id);
        if(it != timeout_.end())
        {
            tmsg = it->second;
            timeout_.erase(it);
            memPool_.Release(tmsg,__FILE__,__LINE__);
        }
        else
        {
            return false;
        }
    }
    return true;
}

void JinTimer::poll()
{
    //JAssert(sd_get_self_taskid() == threadID_);
    refresh();
    size_t tuCount = popAllExpire();
    JAssert(tuCount == timeout_.size());

    std::map<TimerID,TriggerMsg*>::iterator it = timeout_.begin();
    while(it!= timeout_.end())
    {
        TriggerMsg* tmsg = it->second;
        //it = timeout_.erase(it);   居然不支持iterator返回.
        timeout_.erase(it);
        --tuCount;
        TimerID restartID = INVALIDTIMERID;
        if(tmsg->autoRestart){  //先restart，这样在callback过程才有可能cancel
            restartID = start(tmsg);
            JAssert(tmsg->id == restartID);
        }
        tmsg->callback(tmsg->id,tmsg->user,tmsg->user64);
        if(restartID==INVALIDTIMERID){
            memPool_.Release(tmsg,__FILE__,__LINE__); //非autoRestart到这里要释放信息.
        }

        it = timeout_.begin();  //也好，不管是否有删，都拿begin()
        //if(tuCount!=timeout_.size())
        //{   //callback的时候可能删了timeout的timer
        //    it = timeout_.begin();
        //}
    }
    //JAssert(tuCount == 0); 可能callback期间取消了timer,导致tuCount大于0
    return;
}
