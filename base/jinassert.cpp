#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "jinassert.h"
#include "jinformatint.h"
#include "jinlogger.h"  //JELog
#include "jinthread.h"  //msleep
#ifdef JWIN
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#else
#include <execinfo.h>
#endif

#define AssertBufSize 5120
#ifndef JINSWAP
#define JINSWAP
#define SWAPINT(x,y) (x)^=(y);(y)^=(x);(x)^=(y)
#endif
const char _trans[] = "0123456789ABCDEF";
class JinAssert
{
public:
    JinAssert()
        : ptr_(0)
    {
        //printf("JinAssert\n");
    }

    ~JinAssert()
    {
        //printf("~JinAssert\n");
    }

    void trigger(const char *file,
                 const char* function,
                 unsigned int line,
                 const char* expr,
                 const char *msg = 0,
                 void *param = NULL)
    {
        //do some custom.
        stringToBufOut(file);
        paramBuf_[ptr_++] = ':';
        valueToBufOut(line,10);
        paramBuf_[ptr_++] = '[';
        stringToBufOut(function);
        paramBuf_[ptr_++] = ']';
        paramBuf_[ptr_++] = ',';
        stringToBufOut(expr);
        if(msg)
        {
            paramBuf_[ptr_++] = '?';
            stringToBufOut(msg);
        }
        paramBuf_[ptr_++] = ':';
        valueToBufOut((uint64_t)param);
        paramBuf_[ptr_++] = '|';
        if(param)
        {
            valueToBufOut(*(uint8_t*)param);
            paramBuf_[ptr_++] = ',';
            valueToBufOut(*(uint16_t*)param); //(SafeAlignGetPtrValue16(param));
            paramBuf_[ptr_++] = ',';
            valueToBufOut(*(uint32_t*)param); //(SafeAlignGetPtrValue32(param));
            paramBuf_[ptr_++] = ',';
            valueToBufOut(*(uint64_t*)param); //(SafeAlignGetPtrValue64(param));
        }
        paramBuf_[ptr_] = 0;
        JPLog("%s",paramBuf_);
        logAssertStack();

        JinThread::msleep(1000);
    }

protected:
    void valueToBufOut(uint64_t num, int radix=16)
    {
        uint16_t wstart = ptr_;
        do
        {
            paramBuf_[ptr_++] = _trans[num%radix];
            num /= radix;
        }
        while(num>0);
        radix = (ptr_-wstart);  //radix reuse.
        for(;radix>1;radix-=2)  //swap
        {
            SWAPINT(paramBuf_[wstart],paramBuf_[wstart+radix-1]);
            ++wstart;
        }
    }

    void stringToBufOut(const char* str)
    {
        uint16_t strp = 0;
        while(str[strp])
        {
            paramBuf_[ptr_++] = str[strp++];
        }
    }

private:
    char paramBuf_[AssertBufSize];
    uint16_t ptr_;
};

JinAssert gAssert_p;


void jAssertOut(const char *file,
             const char* function,
             unsigned int line,
             const char* expr,
             const char *msg,
             void *param)
{
    gAssert_p.trigger(file,function,line,expr,msg,param);
    JLOG_STOP();
}


void logAssertStack()
{
#ifdef JWIN
        void *stack[100];
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);
        WORD numberOfFrames = CaptureStackBackTrace(0, 100, stack, NULL);
        SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(TCHAR), 1);
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        IMAGEHLP_LINE *line = (IMAGEHLP_LINE *)calloc(sizeof(IMAGEHLP_LINE), 1);
        line->SizeOfStruct = sizeof(IMAGEHLP_LINE);
        printf("CaptureStackBackTrace() returned %hu addresses\n", numberOfFrames);
        for (int i = 0; i < numberOfFrames; i++)
        {
            SymFromAddr(process, (DWORD64)(stack[i]), NULL, symbol);
            SymGetLineFromAddr(process, (DWORD)(stack[i]), NULL, line);
            JELog("#%d 0x%0X %s:%u, Symbol:%s", i, symbol->Address, line->FileName, line->LineNumber, symbol->Name);
        }
        free(line);
        free(symbol);
#else
        int j, nptrs;
        void *buffer[100];
        char **strings;

        nptrs = backtrace(buffer, 100);
        printf("backtrace() returned %d addresses\n", nptrs);

        /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
           would produce similar output to the following: */

        strings = backtrace_symbols(buffer, nptrs);
        if (strings == NULL) {
            JELog("backtrace_symbols");
        }
        else
        {
            for (j = 0; j < nptrs; j++) {
                JELog("%s\n", strings[j]);
            }
        }

        free(strings);
#endif
}
