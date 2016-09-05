#ifndef JINMEMPOOL_H
#define JINMEMPOOL_H

#include "jinmemorycheck.h"
#include "jinassert.h"

#define _DISABLE_MEMORY_POOL     //调试用,调试内存泄露!!

// DS_MEMORY_POOL_MAX_FREE_PAGES must be > 1
#define DS_MEMORY_POOL_MAX_FREE_PAGES 4

/// Very fast memory pool for allocating and deallocating structures that don't have constructors or destructors.
/// Contains a list of pages, each of which has an array of the user structures
template <typename MemoryBlockType>
class JinMemPool
{
public:
    struct Page;
    struct MemoryWithPage
    {
        MemoryBlockType userMemory;
        Page *parentPage;
    };
    struct Page
    {
        MemoryWithPage** availableStack;
        int availableStackSize;
        MemoryWithPage* block;
        Page *next, *prev;
    };

    JinMemPool();
    ~JinMemPool();
    void SetPageSize(int size); // Defaults to 16384 bytes
    MemoryBlockType *Allocate(const char *file, unsigned int line);
    void Release(MemoryBlockType *m, const char *file, unsigned int line);
    void Clear(const char *file, unsigned int line);

    int GetAvailablePagesSize(void) const {return availablePagesSize;}
    int GetUnavailablePagesSize(void) const {return unavailablePagesSize;}
    int GetMemoryPoolPageSize(void) const {return memoryPoolPageSize;}
protected:
    int BlocksPerPage(void) const;
    void AllocateFirst(void);
    bool InitPage(Page *page, Page *prev, const char *file, unsigned int line);

    // availablePages contains pages which have room to give the user new blocks.  We return these blocks from the head of the list
    // unavailablePages are pages which are totally full, and from which we do not return new blocks.
    // Pages move from the head of unavailablePages to the tail of availablePages, and from the head of availablePages to the tail of unavailablePages
    Page *availablePages, *unavailablePages;
    int availablePagesSize, unavailablePagesSize;
    int memoryPoolPageSize;
};

template<class MemoryBlockType>
JinMemPool<MemoryBlockType>::JinMemPool()
{
#ifndef _DISABLE_MEMORY_POOL
    //AllocateFirst();
    availablePagesSize=0;
    unavailablePagesSize=0;
    memoryPoolPageSize=16384;
#endif
}
template<class MemoryBlockType>
JinMemPool<MemoryBlockType>::~JinMemPool()
{
#ifndef _DISABLE_MEMORY_POOL
    Clear(__FILE__,__LINE__);
#endif
}

template<class MemoryBlockType>
void JinMemPool<MemoryBlockType>::SetPageSize(int size)
{
    memoryPoolPageSize=size;
}

template<class MemoryBlockType>
MemoryBlockType* JinMemPool<MemoryBlockType>::Allocate(const char *file, unsigned int line)
{
#ifdef _DISABLE_MEMORY_POOL
    return (MemoryBlockType*) JMalloc2(sizeof(MemoryBlockType), file, line);
#else

    if (availablePagesSize>0)
    {
        MemoryBlockType *retVal;
        Page *curPage;
        curPage=availablePages;
        retVal = (MemoryBlockType*) curPage->availableStack[--(curPage->availableStackSize)];
        if (curPage->availableStackSize==0)
        {
            --availablePagesSize;
            availablePages=curPage->next;
            JAssert(availablePagesSize==0 || availablePages->availableStackSize>0);
            curPage->next->prev=curPage->prev;
            curPage->prev->next=curPage->next;

            if (unavailablePagesSize++==0)
            {
                unavailablePages=curPage;
                curPage->next=curPage;
                curPage->prev=curPage;
            }
            else
            {
                curPage->next=unavailablePages;
                curPage->prev=unavailablePages->prev;
                unavailablePages->prev->next=curPage;
                unavailablePages->prev=curPage;
            }
        }

        JAssert(availablePagesSize==0 || availablePages->availableStackSize>0);
        return retVal;
    }

    availablePages = (Page *) JMalloc2(sizeof(Page), file, line);
    if (availablePages==0)
        return 0;
    availablePagesSize=1;
    if (InitPage(availablePages, availablePages, file, line)==false)
        return 0;
    // If this assert hits, we couldn't allocate even 1 block per page. Increase the page size
    JAssert(availablePages->availableStackSize>1);

    return (MemoryBlockType *) availablePages->availableStack[--availablePages->availableStackSize];
#endif
}
template<class MemoryBlockType>
void JinMemPool<MemoryBlockType>::Release(MemoryBlockType *m, const char *file, unsigned int line)
{
#ifdef _DISABLE_MEMORY_POOL
    JFree2(m, file, line);
    return;
#else
    // Find the page this block is in and return it.
    Page *curPage;
    MemoryWithPage *memoryWithPage = (MemoryWithPage*)m;
    curPage=memoryWithPage->parentPage;

    if (curPage->availableStackSize==0)
    {
        // The page is in the unavailable list so move it to the available list
        curPage->availableStack[curPage->availableStackSize++]=memoryWithPage;
        unavailablePagesSize--;

        // As this page is no longer totally empty, move it to the end of available pages
        curPage->next->prev=curPage->prev;
        curPage->prev->next=curPage->next;

        if (unavailablePagesSize>0 && curPage==unavailablePages)
            unavailablePages=unavailablePages->next;

        if (availablePagesSize++==0)
        {
            availablePages=curPage;
            curPage->next=curPage;
            curPage->prev=curPage;
        }
        else
        {
            curPage->next=availablePages;
            curPage->prev=availablePages->prev;
            availablePages->prev->next=curPage;
            availablePages->prev=curPage;
        }
    }
    else
    {
        curPage->availableStack[curPage->availableStackSize++]=memoryWithPage;

        if (curPage->availableStackSize==BlocksPerPage() &&
            availablePagesSize>=DS_MEMORY_POOL_MAX_FREE_PAGES)
        {
            // After a certain point, just deallocate empty pages rather than keep them around
            if (curPage==availablePages)
            {
                availablePages=curPage->next;
                JAssert(availablePages->availableStackSize>0);
            }
            curPage->prev->next=curPage->next;
            curPage->next->prev=curPage->prev;
            availablePagesSize--;
            JFree2(curPage->availableStack, file, line );
            JFree2(curPage->block, file, line );
            JFree2(curPage, file, line );
        }
    }
#endif
}
template<class MemoryBlockType>
void JinMemPool<MemoryBlockType>::Clear(const char *file, unsigned int line)
{
#ifdef _DISABLE_MEMORY_POOL
    return;
#else
    Page *cur, *freed;

    if (availablePagesSize>0)
    {
        cur = availablePages;
#ifdef _MSC_VER
#pragma warning(disable:4127)   // conditional expression is constant
#endif
        while (true)
        // do
        {
            JFree2(cur->availableStack, file, line );
            JFree2(cur->block, file, line );
            freed=cur;
            cur=cur->next;
            if (cur==availablePages)
            {
                JFree2(freed, file, line );
                break;
            }
            JFree2(freed, file, line );
        }// while(cur!=availablePages);
    }

    if (unavailablePagesSize>0)
    {
        cur = unavailablePages;
        while (1)
        //do
        {
            JFree2(cur->availableStack, file, line );
            JFree2(cur->block, file, line );
            freed=cur;
            cur=cur->next;
            if (cur==unavailablePages)
            {
                JFree2(freed, file, line );
                break;
            }
            JFree2(freed, file, line );
        } // while(cur!=unavailablePages);
    }

    availablePagesSize=0;
    unavailablePagesSize=0;
#endif
}
template<class MemoryBlockType>
int JinMemPool<MemoryBlockType>::BlocksPerPage(void) const
{
    return memoryPoolPageSize / sizeof(MemoryWithPage);
}
template<class MemoryBlockType>
bool JinMemPool<MemoryBlockType>::InitPage(Page *page, Page *prev, const char *file, unsigned int line)
{
    int i=0;
    const int bpp = BlocksPerPage();
    page->block=(MemoryWithPage*) JMalloc2(memoryPoolPageSize, file, line);
    if (page->block==0)
        return false;
    page->availableStack=(MemoryWithPage**)JMalloc2(sizeof(MemoryWithPage*)*bpp, file, line);
    if (page->availableStack==0)
    {
        JFree2(page->block, file, line );
        return false;
    }
    MemoryWithPage *curBlock = page->block;
    MemoryWithPage **curStack = page->availableStack;
    while (i < bpp)
    {
        curBlock->parentPage=page;
        curStack[i]=curBlock++;
        i++;
    }
    page->availableStackSize=bpp;
    page->next=availablePages;
    page->prev=prev;
    return true;
}

#endif // JINMEMPOOL_H
