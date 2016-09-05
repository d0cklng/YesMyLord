#ifndef JINLINKEDLIST_H
#define JINLINKEDLIST_H

#include "jinformatint.h"
#include <stddef.h>

/* ******************************
 *
 * 链表实现，承载queue、stack、deque的实现
 * 使用根节点串双向循环列表实现
 * 依赖: jinmemorycheck jinassert jinsharedata
 * 特征： 1.默认构造函数创建一个空对象，使用前必须init
 * 2. 浅拷贝 赋值也会引用,相当于说改一处全改
 * 3. 模板类 在头文件完全实现
 *
 * ******************************/

#include "jinassert.h"
#include "jinsharedata.h"
#include "jinmemorycheck.h"


template <typename TplDataType>
class JinLinkedList
{
public:
    JinLinkedList(): data_(NULL){}  //空对象
    ~JinLinkedList(){
        if(data_) data_->detach();
    }
    JinLinkedList(const JinLinkedList &o): data_(NULL){
        data_ = o.data_;
        if(!data_->attach())
            data_ = NULL;
    }
    //这个函数使空对象成为，成为空队列。这里面会发生内存申请。如果仍是空对象返回false
    bool init(){
        if(data_)return true;
        data_ = JNew(JinLinkedListData);
        if(data_==NULL || !data_->isInitOk())return false;
        return true;
    }
    JinLinkedList& operator=(const JinLinkedList& o){
        if(data_)data_->detach();
        data_ = o.data_;
        if(!data_->attach())
            data_ = NULL;
        return *this;
    }
    //注意0个元素不意味着isNULL,仅在默认构造的情况下发生。
    bool isNull(){
        return (data_==NULL);
    }

    inline TplDataType& front(){
        JAssert(data_);
        return data_->front();
    }

    bool push_front(const TplDataType &data){
        if(data_==NULL && !this->init())return false;
        return data_->push_front(data);
    }
    void pop_front(){
        JAssert(data_);
        data_->pop_front();
    }
    inline TplDataType& back(){
        JAssert(data_);
        return data_->back();
    }
    bool push_back(const TplDataType &data){
        if(data_==NULL && !this->init())return false;
        return data_->push_back(data);
    }
    void pop_back(){
        JAssert(data_);
        data_->pop_back();
    }
    JinLinkedList& absorb(JinLinkedList* tail)
    {  //吞噬tail,tail的头接在此链尾,tail变空链.
        this->data_->absorb(tail->data_);
        return *this;
    }
    inline size_t size(){
        if(data_)return data_->size();
        else return 0;
    }
    inline void clear(){
        if(data_) data_->clear();
    }

    struct JinLinkedListNode
    {
        TplDataType data;
        struct JinLinkedListNode *prev;
        struct JinLinkedListNode *next;
    };

    class JinLinkedListData : public JinShareData
    {
    public:
        JinLinkedListData()
            : root_(NULL)
            , size_(0)
        {
            root_ = JNew(JinLinkedListNode);
            if(root_){
                root_->prev = root_;
                root_->next = root_;
            }
            size_ = 0;
        }
        ~JinLinkedListData()
        {
            clear();
            if(root_){
                JDelete(root_);
            }
        }
        inline bool isInitOk()
        {
            return (root_!=NULL);
        }
        inline TplDataType& front()
        {
            return root_->next->data;
        }
        bool push_front(const TplDataType &data)
        {
            JinLinkedListNode *node = JNew(JinLinkedListNode);
            if(node){
                node->data = data;
                node->next = root_->next;
                node->prev = root_;
                //root_->next = node;
                node->prev->next = node;
                node->next->prev = node;
                ++size_;
                return true;
            }
            return false;
        }
        void pop_front()
        {
           JinLinkedListNode *node = root_->next;
           if(node!=root_){
               --size_;
               node->prev->next = node->next;
               node->next->prev = node->prev;
               JDelete(node);
           }
        }


        inline TplDataType& back()
        {
            return root_->prev->data;
        }
        bool push_back(const TplDataType &data)
        {
            JinLinkedListNode *node = JNew(JinLinkedListNode);
            if(node){
                node->data = data;
                node->next = root_;
                node->prev = root_->prev;
                node->prev->next = node;
                node->next->prev = node;
                ++size_;
                return true;
            }
            return false;
        }
        void pop_back()
        {
            JinLinkedListNode *node = root_->prev;
            if(node!=root_){
                --size_;
                node->prev->next = node->next;
                node->next->prev = node->prev;
                JDelete(node);
            }
        }
        bool insertBefore(JinLinkedListNode *n,const TplDataType &data)
        {
            JinLinkedListNode *node = JNew(JinLinkedListNode);
            if(node){
                node->data = data;
                node->next = n;
                node->prev = n->prev;
                node->prev->next = node;
                node->next->prev = node;
                ++size_;
                return true;
            }
            return false;
        }
        bool insertAfter(JinLinkedListNode *n,const TplDataType &data)
        {
            JinLinkedListNode *node = JNew(JinLinkedListNode);
            if(node){
                node->data = data;
                node->next = n->next;
                node->prev = n;
                node->prev->next = node;
                node->next->prev = node;
                ++size_;
                return true;
            }
            return false;
        }

        void absorb(JinLinkedListData *behind)
        {
            behind->root_->next->prev = root_->prev;
            root_->prev->next = behind->root_->next;

            behind->root_->prev->next = root_;
            root_->prev = behind->root_->prev;

            behind->root_->next = behind->root_;
            behind->root_->prev = behind->root_;

            size_ += behind->size_;
            behind->size_ = 0;
        }

        inline size_t size()
        {
            return size_;
        }
        inline void clear()
        {
            while(size_>0)
            {
                this->pop_front();
            }
        }
        inline JinLinkedListNode* begin()
        {
            return root_->next;
        }
        inline JinLinkedListNode* end()
        {
            return root_;
        }
        inline JinLinkedListNode* erase(JinLinkedListNode* s)
        {
            JAssert(s!=root_);
            if(s!=root_)
            {
                --size_;
                JinLinkedListNode *node = s->next;
                s->prev->next = node;
                node->prev = s->prev;
                JDelete(s);
                return node;
            }
            return s;
        }

    private:
        JinLinkedListNode *root_;
        size_t size_;

    };

    class iterator
    {
    public:
        iterator():node_(NULL),it_(NULL){}
        iterator(const iterator &o){
            this->node_ = o.node_;
        }
        inline iterator& operator = ( const iterator& other ){
            this->node_ = other.node_;
            return *this;
        }
        inline bool operator != ( const iterator& other ){
            return (this->node_ != other.node_);
        }
        inline bool operator == ( const iterator& other ){
            return (this->node_ == other.node_);
        }
        TplDataType& operator*(){
            return node_->data;
        }
        iterator& operator++(){  // ++A;
            node_ = node_->next;
            return *this;
        }
        iterator& operator++( int ){  //  A++;
            iterator oldit(node_);  //TODO 这里返回局部变量会有警告 避免使用后加加.
            node_ = node_->next;
            return oldit;

            //iterator tmp(*this);
            //++(*this);
            //return tmp;

            //iterator oldit(node_); //实验代码，可否正常工作又无警告.
            //it_ = &oldit;
            //node_ = node_->next;
            //return *it_;

        }
        iterator& operator--(){  // --A;
            node_ = node_->prev;
            return *this;
        }
        iterator& operator--( int ){  // A--;
            iterator oldit(node_);
            node_ = node_->prev;
            return oldit;
        }

        JinLinkedListNode* innerNode(){return node_;}
        //protected:
        explicit iterator(JinLinkedListNode* node):node_(node){}
    private:
        friend class JinLinkedList;
        JinLinkedListNode* node_;
        iterator *it_;
    };

    iterator begin()
    {
        //JAssert(data_);
        if(!data_) return iterator(NULL);
        return iterator(data_->begin());
    }
    iterator end()
    {
        //JAssert(data_);
        if(!data_) return iterator(NULL);
        return iterator(data_->end());
    }
    iterator erase(iterator & it)
    {
        return iterator(data_->erase(it.innerNode()));
    }
    bool insertBefore(iterator & it,const TplDataType &data)
    {
        if(!data_)return push_back(data);
        return data_->insertBefore(it.innerNode(),data);
    }
    bool insertAfter(iterator & it,const TplDataType &data)
    {
        if(!data_)return push_back(data);
        return data_->insertAfter(it.innerNode(),data);
    }

private:
    friend class iterator;
    JinLinkedListData *data_;

};


#endif // JINLINKEDLIST_H
