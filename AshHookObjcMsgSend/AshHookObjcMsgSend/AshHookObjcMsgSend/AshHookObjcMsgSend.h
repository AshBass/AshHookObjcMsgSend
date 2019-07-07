
#ifndef AshHookObjcMsgSend_h
#define AshHookObjcMsgSend_h

#include <stdio.h>
#include <objc/objc.h>
#include <objc/runtime.h>

typedef struct {
    __unsafe_unretained Class class;
    SEL selector;
    uint64_t time; /// 单位 us
    int depth;
} AshCallRecord;

AshCallRecord * getCallRecords(void);
int getRecordNumber(void);

void ashStartHook(void);
void ashStopHook(void);

#endif /* AshHookObjcMsgSend_h */
