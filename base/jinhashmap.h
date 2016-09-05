#ifndef JINHASHMAP_H_201410032329
#define JINHASHMAP_H_201410032329

#include "jinformatint.h"
#include <stddef.h>
#include <string.h>

/* ******************************
 *
 * 哈希map（字典）实现,目标是高效插入，高效移除，高效索引
 * 依赖: jinassert jinmemorycheck jinsharedata
 * 特性：1.默认构造函数创建一个空对象，使用前必须init
 * 2. 需要预置数组init，数组越大效率越好越费内存
 * 3. 由调用者填入指导值，数组长度在适当情况做适当扩展。
 * 4. 浅拷贝 赋值也会引用,相当于说改一处全改
 * 5. 模板类 在头文件完全实现
 * 特别注意，如果key是char*,必须考虑buffer有效性问题,
 *   最好让value是一个结构体，key在value里保持着
 *
 * 测试：目前粗略测试在10000量级内和std相当，更高量级内存占用偏大但查找时间占优》
 *
 * ******************************/

#include "jinassert.h"
#include "jinsharedata.h"
#include "jinmemorycheck.h"

//哈希用的集合分母，很早前就看到过了，现在已经不知道是怎么算出来的了.
#ifdef JDEBUG
    static const int jhm_primeNums = 29;
#else
    static const int jhm_primeNums = 28;
#endif
static const unsigned long jhm_primeList[jhm_primeNums] =
{
#ifdef JDEBUG
  7ul,
#endif
  53ul,         97ul,         193ul,       389ul,       769ul,
  1543ul,       3079ul,       6151ul,      12289ul,     24593ul,
  49157ul,      98317ul,      196613ul,    393241ul,    786433ul,
  1572869ul,    3145739ul,    6291469ul,   12582917ul,  25165843ul,
  50331653ul,   100663319ul,  201326611ul, 402653189ul, 805306457ul,
  1610612741ul, 3221225473ul, 4294967291ul
};
#ifndef ABSDIFF
#define ABSDIFF(x,y) ((x)>(y)?((x)-(y)):((y)-(x)))
#endif

class __JinHashKeyConvert
{
public:
    __JinHashKeyConvert(const char *ch)
        : isNum_(false), buf_((const unsigned char*)ch)
    {size_ = strlen(ch);}
    //__JinHashKeyConvert(unsigned char *ch, size_t sz)
    //    : size_(sz), isNum_(false), buf_(ch){}
    __JinHashKeyConvert(void *ptr)
        : isNum_(true), size_((size_t)ptr){}
    __JinHashKeyConvert(int32_t num)
        : isNum_(true), size_(num){}
    __JinHashKeyConvert(uint32_t num)
        : isNum_(true), size_(num){}
    __JinHashKeyConvert(int64_t num)
        : isNum_(true), size_(num){}
    __JinHashKeyConvert(uint64_t num)
        : isNum_(true), size_(num){}
public:
    inline bool isNum()const {return isNum_;}
    inline size_t size()const {return size_;}
    inline uint64_t number()const {return size_;}
    inline const unsigned char* buf() const {return isNum_?NULL:buf_;}
    inline bool operator == (const __JinHashKeyConvert& other)
    {
        if(this->isNum() != other.isNum() ) return false;
        if(!this->isNum())
        {
            if(this->size() != other.size() ) return false;
            return (0==memcmp(this->buf(),other.buf(),this->size()));
        }
        if(this->number() != other.number() ) return false;
        return true;
    }

private:
    bool isNum_;
    uint64_t size_;
    const unsigned char* buf_;

};


// BKDR变种 取更大的seed 返回值取ulong长度随系统
static unsigned long BKDRHashHex(const unsigned char *str, size_t len)
{
    const unsigned int seed = 13131; // 31 131 1313 13131 131313 etc..
    unsigned long hash = 0;

    for(size_t i=0;i<len;++i)
    {
        hash = hash * seed + (*str++);
    }

    return hash;
}
template <typename KeyType>
uint32_t JinHashDefault(const KeyType &key)
{
    __JinHashKeyConvert convert(key);
    if(convert.isNum())
    {
        return convert.number() & 0x7FFFFFFF;
    }
    else
    {
        return BKDRHashHex(convert.buf(),convert.size()) & 0x7FFFFFFF;
    }
}

template <typename KeyType>
bool JinHashIsEqual(const KeyType& a, const KeyType& b)
{
    return (a==b);
}

template<typename KeyType,typename ValueType,
         uint32_t (*JinHashFunc)(const KeyType &key) = JinHashDefault<KeyType>,
         bool (*JinHashIsEqual)(const KeyType &a, const KeyType &b) = JinHashIsEqual<KeyType> >
class JinHashMap
{
public:
    JinHashMap()
        : data_(NULL)
    {}  //空对象
    ~JinHashMap(){
        if(data_) data_->detach();
    }
    JinHashMap(const JinHashMap &o)
        : data_(NULL)
    {
        data_ = o.data_;
        if(data_){
            if(!data_->attach())
                data_ = NULL;
        }
    }
    //使用前手工调用init传入预期数量规模以优化性能.
    bool init(uint32_t countPridict=0){
        if(data_)return true;
        data_ = JNew(JinHashMapData,countPridict);
        if(data_==NULL || !data_->isInitOk())return false;
        return true;
    }
    JinHashMap& operator=(const JinHashMap& o){
        if(data_)data_->detach();
        data_ = o.data_;
        if(data_)
        {
            if(!data_->attach())
                data_ = NULL;
        }
        return *this;
    }

    ValueType& operator[](const KeyType &key)
    {
        if(data_==NULL) this->init();
        if(!data_->has(key))
        {
            ValueType *vt = new ValueType;
            data_->insert(key,*vt);
            delete vt;
        }
        return data_->find(key)->value;

        //ValueType vt;
        ////if(data_==NULL && !this->init())return vt;
        //return data_->get(key,vt);
    }

    bool isNull(){
        return (data_==NULL);
    }

    //如果key存在则失败.
    bool insert(const KeyType &key,const ValueType &value)
    {
        if(data_==NULL && !this->init())return false;
        return data_->insert(key,value);
    }
    //如果key存在则覆盖.
    bool set(const KeyType &key,const ValueType &value)
    {
        if(data_==NULL && !this->init())return false;
        return data_->set(key,value);
    }
    //减少查找次数，最好直接用get.
    bool has(const KeyType &key)
    {
        if(data_==NULL && !this->init())return false;
        return data_->has(key);
    }

    ValueType get(const KeyType &key,ValueType defaultValue)
    {
        if(data_==NULL)return defaultValue;
        return data_->get(key,defaultValue);
    }

    //如果key不存在返回false
    bool remove(const KeyType &key)     //TODO hash空间扩容有，收缩要不要?
    {
        //JAssert(data_);
        if(data_==NULL)return false;
        return data_->remove(key);
    }
    typedef unsigned int (*jhm_HashFunc)(KeyType key);
    void setHashFunc(jhm_HashFunc hashFunc)  //TODO 现在的实现，似乎没有意义了.
    {
        JAssert(hashFunc);
        if(data_==NULL && !this->init())return false;
        return data_->setHashFunc(hashFunc);
    }
    inline size_t size() const
    {
        if(data_)return data_->size();
        else return 0;
    }
    inline void clear()
    {
        if(data_) data_->clear();
    }

    //template <typename KeyType,typename ValueType>
    struct JinHashMapNode
    {
        KeyType key;
        ValueType value;
        unsigned int hash;
        //struct JinHashMapNode *below;  //用于duplicate hash key
        struct JinHashMapNode *prev;  //用于枚举
        struct JinHashMapNode *next;  //用于枚举 同时用于duplicate hash
    };
    //当把next用作below的时候，需要区分next是同一个hash pos的重复键还是下一个pos。见nodeBelow函数.

    class JinHashMapData : public JinShareData
    {
    public:
        JinHashMapData(uint32_t countPridict)
            : root_(NULL)
            , size_(0)
            , hashMap_(NULL)
            , prime_(0)
        {
            if(countPridict>0){
                uint32_t min = (uint32_t)-1;
                for(int i=0;i<jhm_primeNums;i++){
                    uint32_t dif = ABSDIFF(jhm_primeList[i],countPridict);
                    if(dif<min) {
                        min = dif;
                    }
                    else {
                        prime_ = i-1;
                        break;
                    }
                }
            }

            size_t sz = sizeof(JinHashMapNode *)*jhm_primeList[prime_];
            hashMap_ = (JinHashMapNode **)JMalloc(sz);
            if(hashMap_){
                root_ = JNew(JinHashMapNode); //(JinHashMapNode*)JMalloc(sizeof(JinHashMapNode));
                if(root_){
                    root_->hash = 0;
                    root_->value = 0;
                    root_->key = 0;
                    root_->prev = root_;
                    root_->next = root_;
                    memset(hashMap_,0,sz);
                    size_ = 0;
                    return;
                }
                JFree(hashMap_);
                hashMap_ = NULL;
            }
        }
        ~JinHashMapData()
        {
            clear();
            if(root_){
                JDelete(root_);//JFree(root_);
            }
            if(hashMap_){
                JFree(hashMap_);
            }
        }

        inline bool isInitOk()
        {
            return hashMap_&&root_;
        }

        //如果key存在则失败.
        bool insert(const KeyType &key,const ValueType &value)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            if(node)return false;

            if(beforeInsert())
            {
                pos = hash % jhm_primeList[prime_];
            }
            return insert(key,value,hash,pos);
        }
        //如果key存在则覆盖.
        bool set(const KeyType &key,const ValueType &value)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            if(node)
            { //其他不变.
                node->value = value;
                return true;
            }

            if(beforeInsert())
            {
                pos = hash % jhm_primeList[prime_];
            }
            return insert(key,value,hash,pos);
        }
        //减少查找次数，最好直接用get.
        bool has(const KeyType &key)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            return (node!=NULL);
        }

        ValueType& get(const KeyType &key, ValueType& defaultValue)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            if(!node)return defaultValue;
            else return node->value;
        }
        //如果key不存在返回false.
        bool remove(const KeyType &key)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            if(!node)return false;
            if(hashMap_[pos]==node){
                hashMap_[pos] = nodeBelow(node);
            }
            removeNode(node);
            return true;

        }
        JinHashMapNode* find(const KeyType &key)
        {
            unsigned int hash = JinHashFunc(key);
            unsigned int pos = hash % jhm_primeList[prime_];
            JinHashMapNode* node = findKey(key,pos);
            if(!node)return root_;
            return node;
        }


        inline size_t size() const
        {
            return size_;
        }
        inline void clear()
        {
            while(size_>0)
            {
                removeNode(root_->next);
            }

        }
        inline JinHashMapNode* begin()
        {
            return root_->next;
        }
        inline JinHashMapNode* end()
        {
            return root_;
        }
        inline JinHashMapNode* erase(JinHashMapNode* toErase)
        {
            JinHashMapNode* next = toErase->next;
            removeNode(toErase);
            return next;
        }

    protected:
        //JinHashMapNode* findKey(KeyType &key,unsigned int pos,JinHashMapNode **nodeAbove=NULL)
        JinHashMapNode* findKey(const KeyType &key,unsigned int pos)
        {
            JinHashMapNode *node = hashMap_[pos];
            while(node)
            {
                //if(node->key==key)return node;  //const char* == not work as expect!!
                //if(__JinHashKeyConvert(node->key) == __JinHashKeyConvert(key))return node;
                //if(node->key == key) return node;
                if(JinHashIsEqual(node->key,key)) return node;
                //if(nodeAbove)*nodeAbove = node;
                node = nodeBelow(node);//node->below;
            }
            return NULL;
        }

        inline JinHashMapNode* nodeBelow(JinHashMapNode* node)
        {
            //return node->below;
            //我使用的办法是判读next所处的内存地址，是否hashMap后的地址,--不可行
            //if(node->next){
            //    int d = node->next - hashMap_;
            //    if(d<0 || d>=jhm_primeList[prime_]*sizeof(JinHashMapNode*))return node->next;
            //}
            if(node->next && node->next!=root_ &&
                    (node->hash % jhm_primeList[prime_] ==
                     node->next->hash % jhm_primeList[prime_]))
            {   return node->next;  }
            return NULL;
        }

        bool beforeInsert()  //发生resize则返回true.
        {
            if(size_>jhm_primeList[prime_]*0.9)
            {
                //试着重新分配空间.
                JinHashMapNode **newHashMap ;
                size_t count = jhm_primeList[prime_+1];
                newHashMap = (JinHashMapNode **)JMalloc(sizeof(JinHashMapNode *)*count);
                if(newHashMap){
                    //将node转移到新的root.
                    memset(newHashMap,0,sizeof(JinHashMapNode *)*count);
                    //把root_取下来.
                    JinHashMapNode *oldRootNext = root_->next;
                    root_->prev->next = NULL;
                    root_->next = root_;
                    root_->prev = root_;
                    //JinHashMapNode newRoot;
                    //newRoot.prev = &newRoot;
                    //newRoot.next = &newRoot;
                    JinHashMapNode *nodeWalk = oldRootNext;
                    do
                    {
                        JAssert(nodeWalk!=root_); //有node的时候才会来重分配，所以一定不是root_
                        JinHashMapNode *walkNext = nodeWalk->next;

                        unsigned int pos = nodeWalk->hash % count;
                        JinHashMapNode *beInsert = newHashMap[pos]?newHashMap[pos]:root_;
                        nodeWalk->next = beInsert;
                        nodeWalk->prev = beInsert->prev;
                        nodeWalk->next->prev = nodeWalk;
                        nodeWalk->prev->next = nodeWalk;
                        newHashMap[pos] = nodeWalk;

                        nodeWalk = walkNext;
                    }
                    while(nodeWalk!=NULL); //nodeWalk!=root_

                    JFree(hashMap_);
                    hashMap_ = newHashMap;
                    ++prime_;
                    return true;
                }
            }
            return false;
        }

        bool insert(const KeyType& key,const ValueType& value,unsigned int hash,unsigned int pos)
        {
            JinHashMapNode *node = JNew(JinHashMapNode);//(JinHashMapNode*)JMalloc(sizeof(JinHashMapNode));
            if(!node)return false;
            node->hash = hash;
            node->key = key;
            node->value = value;

            JinHashMapNode *beInsert = hashMap_[pos]?hashMap_[pos]:root_;
            node->next = beInsert;
            node->prev = beInsert->prev;
            node->next->prev = node;
            node->prev->next = node;
            ++size_;

            //node->below = hashMap_[pos];
            hashMap_[pos] = node;

            return true;
        }

        void removeNode(JinHashMapNode* node, int idx=-1)
        {
            JAssert(node!=root_);
            --size_;

            unsigned int pos;
            if(idx>0){
                pos = (unsigned int)idx;
            }
            else {
                unsigned int hash = JinHashFunc(node->key);
                pos = hash % jhm_primeList[prime_];
            }
            if(hashMap_[pos]==node){
                hashMap_[pos] = nodeBelow(node);
            }

            node->prev->next = node->next;
            node->next->prev = node->prev;
            JDelete(node); //JFree(node);
        }
    private:
        JinHashMapNode *root_;
        size_t size_;
        JinHashMapNode **hashMap_;
        int prime_;
    };


    class iterator
    {
    public:
        iterator():node_(NULL){}  //,it_(NULL)
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

        KeyType& first(){
            return node_->key;
        }
        ValueType& second(){
            return node_->value;
        }

        iterator& operator++(){  // ++A;
            node_ = node_->next;
            return *this;
        }
        iterator& operator++( int ){  //  A++;
            iterator oldit(node_);  //TODO 这里返回局部变量会有警告 避免使用后加加
            node_ = node_->next;
            return oldit;
            //iterator tmp(*this);
            //++(*this);
            //return tmp;

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

        //protected:
        explicit iterator(JinHashMapNode* node):node_(node){}
    private:
        friend class JinHashMap;
        JinHashMapNode* node_;
        //iterator *it_;
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

    iterator find(const KeyType &key)
    {
        JAssert(data_);
        return iterator(data_->find(key));
    }

    iterator erase(iterator& it)
    {
        JAssert(data_);
        return iterator(data_->erase(it.node_));
    }

private:
    //friend class iterator;
    JinHashMapData *data_;
};


#endif // JINHASHMAP_H

/* 举例
 * protected:
    static uint32_t JinKey160HashFunc(const JinKey160& key);
    static uint32_t JinStoreValueHash(const JinBuffer& buffer);
    static bool JinStoreValueIsEqual(const JinBuffer &a, const JinBuffer &b);
private:
    typedef JinHashMap<JinBuffer,int,JinStoreValueHash,JinStoreValueIsEqual> ValueGroup;
    struct ValueStruct
    {
        ValueStruct(int =0){}
        JinKey160 hashKey;
        ValueGroup values;
        //first time
        //last update
    };

    JinHashMap<JinKey160,ValueStruct,JinKey160HashFunc> store_;
    */
