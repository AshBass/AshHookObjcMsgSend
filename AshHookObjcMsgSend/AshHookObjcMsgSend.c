
#include "AshHookObjcMsgSend.h"
#include <malloc/_malloc.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include "fishhook.h"

#if defined(__arm64__)

/// 是否需要记录
static bool callRecordEnabled = false;

#pragma mark - 结构体相关

static AshCallRecord *callRecords; /// 数组
static int recordNumber; /// AshCallRecord 记录了多少条
static int recordAlloc; /// AshCallRecord 记录数据大小，可扩充

AshCallRecord * getCallRecords(void) {
    return callRecords;
}

int getRecordNumber(void) {
    return recordNumber;
}

static pthread_key_t threadKey;

typedef struct {
    id obj;
    Class class;
    SEL cmd;
    uint64_t time; // 单位 us
    uintptr_t lr; // 程序链接寄存器，保存跳转返回信息地址
} AshHookRecod;

typedef struct {
    AshHookRecod *stack;
    int allocatedLength; /// AshThreadStack 大小，用来判断是否需要扩充
    int index;
    bool isMainThread;
} AshThreadStack;

static inline AshThreadStack * getThreadStack() {
    AshThreadStack *threadStask = (AshThreadStack *)pthread_getspecific(threadKey);
    if (threadStask == NULL) {
        threadStask = (AshThreadStack*)malloc(sizeof(AshThreadStack));
        threadStask->stack = (AshHookRecod *)calloc(128, sizeof(AshHookRecod));
        threadStask->allocatedLength = 64;
        threadStask->index = -1;
        threadStask->isMainThread = pthread_main_np();
        /// 使用 pthread_setspecific 让 threadStask 与当前线程绑定 ，threadStask 与线程一一对应
        pthread_setspecific(threadKey, threadStask);
    }
    return threadStask;
}

static void releaseStack(void *ptr) {
    AshThreadStack *threadStask = (AshThreadStack *)ptr;
    if (!threadStask) return;
    if (threadStask->stack) free(threadStask->stack);
    free(threadStask);
}

static inline void pushStack(id obj, Class class, SEL selector, uintptr_t lr) {
    AshThreadStack *threadStack = getThreadStack();
    if (threadStack) {
        int nextIndex = (++threadStack->index);
        if (nextIndex >= threadStack->allocatedLength) {
            threadStack->allocatedLength += 64;
            threadStack->stack = (AshHookRecod *)realloc(threadStack->stack, threadStack->allocatedLength * sizeof(AshHookRecod));
        }
        AshHookRecod *newRecord = &threadStack->stack[nextIndex];
        newRecord->obj = obj;
        newRecord->class = class;
        newRecord->cmd = selector;
        newRecord->lr = lr;
        
        struct timeval now;
        gettimeofday(&now, NULL);
        newRecord->time = (now.tv_sec % 100) * 1000000 + now.tv_usec;
    }
}

static inline uintptr_t popStack() {
    AshThreadStack *threadStack = getThreadStack();
    int currentIndex = threadStack->index;
    int nextIndex = threadStack->index--;
    AshHookRecod *stack = &threadStack->stack[nextIndex];
    
    if (callRecordEnabled) {
        struct timeval now;
        gettimeofday(&now, NULL);
        uint64_t time = (now.tv_sec % 100) * 1000000 + now.tv_usec;
        if (time < stack->time) {
            time += 100 * 1000000;
        }
        uint64_t cost = time - stack->time;
        if (!callRecords) {
            recordAlloc = 1024;
            callRecords = malloc(sizeof(AshCallRecord) * recordAlloc);
        }
        recordNumber++;
        if (recordNumber >= recordAlloc) {
            recordAlloc += 1024;
            callRecords = realloc(callRecords, sizeof(AshCallRecord) * recordAlloc);
        }
        AshCallRecord *callRecord = &callRecords[recordNumber - 1];
        callRecord->class = stack->class;
        callRecord->depth = currentIndex;
        callRecord->selector = stack->cmd;
        callRecord->time = cost;
    }
    
    return stack->lr;
}

#pragma mark - objMsgSend相关

__unused static id (*origObjcMsgSend)(id, SEL, ...);

void beforeObjcMsgSend(id obj, SEL selector, uintptr_t lr) {
    pushStack(obj, object_getClass(obj), selector, lr);
}

uintptr_t afterObjcMsgSend() {
    return popStack();
}

/// 将 value 函数地址给到 x12 ，然后使用 b 方法跳转 （b可以为blr 、br等）
#define call(b, value) \
__asm volatile ("stp x8, x9, [sp, #-16]!\n"); \
__asm volatile ("mov x12, %0\n" :: "r"(value)); \
__asm volatile ("ldp x8, x9, [sp], #16\n"); \
__asm volatile (#b " x12\n");

/// 每条指令意思 ：栈空间地址-16，开辟新的空间存储 x0 ~ x7 的值
#define save() \
__asm volatile ( \
"stp x8, x9, [sp, #-16]!\n" \
"stp x6, x7, [sp, #-16]!\n" \
"stp x4, x5, [sp, #-16]!\n" \
"stp x2, x3, [sp, #-16]!\n" \
"stp x0, x1, [sp, #-16]!\n");

/// 每条指令意思 ：取出值，然乎栈空间地址+16
#define load() \
__asm volatile ( \
"ldp x0, x1, [sp], #16\n" \
"ldp x2, x3, [sp], #16\n" \
"ldp x4, x5, [sp], #16\n" \
"ldp x6, x7, [sp], #16\n" \
"ldp x8, x9, [sp], #16\n" );

#define ret() __asm volatile ("ret\n");

/**
 arm64 前8个参数为 x0 ~ x7 ，其余由栈传递
 */
__attribute__((__naked__))
static void ashObjcMsgSend() {
    
    /// 保存 x0 ~ x7 的进入栈中
    save()
    
    /// 为了将lr跳转地址传到 beforeObjcMsgSend 中
    __asm volatile ("mov x2, lr\n");
    __asm volatile ("mov x3, x4\n");
    
    /// 执行beforeObjcMsgSend方法
    call(blr, &beforeObjcMsgSend)
    
    /// 取出原来的 x0 ~ x7
    load()
    
    /// 执行objcMsgSend
    call(blr, origObjcMsgSend)
    
    /// 保存 x0 ~ x7 的进入栈中
    save()
    
    /// 执行afterObjcMsgSend方法后
    call(blr, &afterObjcMsgSend)
    
    /// x0 是 afterObjcMsgSend 的返回值，要把它放入lr中，这样 AshObjcMsgSend 的返回地址就和原来的一样了
    __asm volatile ("mov lr, x0\n");
    
    /// 取出原来的 x0 ~ x7
    load()
    
    /// 结束
    ret()
}

void ashStartHook(void) {
    callRecordEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        /// 线程退出时，会调用 releaseStack 释放分配的缓存，参数是 threadKey 所关联的数据
        pthread_key_create(&threadKey, &releaseStack);
        struct rebinding objcMsgSend = {"objc_msgSend", ashObjcMsgSend, (void**)&origObjcMsgSend};
        struct rebinding rebind[1] = {objcMsgSend};
        rebind_symbols(rebind, 1);
    });
}

void ashStopHook(void) {
    callRecordEnabled = false;
}

#else

void ashStartHook(void) {
    printf("仅支持arm64架构\n");
}

void ashStopHook(void) {
    printf("仅支持arm64架构\n");
}

AshCallRecord * getCallRecords(void) {
    printf("仅支持arm64架构\n");
    return NULL;
}

int getRecordNumber(void) {
    printf("仅支持arm64架构\n");
    return 0;
}

#endif


