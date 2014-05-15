// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

enum ThreadStatus {
    THREADSTATUS_RUNNING    = 1,
    THREADSTATUS_READY      = 2,
    THREADSTATUS_WAIT       = 4,
    THREADSTATUS_SUSPEND    = 8,
    THREADSTATUS_DORMANT    = 16,
    THREADSTATUS_DEAD       = 32,

    THREADSTATUS_WAITSUSPEND = THREADSTATUS_WAIT | THREADSTATUS_SUSPEND
};

struct ThreadContext {
    void reset();

    u32 reg[16];
    u32 cpsr;
    u32 pc;
};

class Thread;

Thread* __KernelCreateThread(UID& id, UID module_id, const char* name, u32 priority, u32 entrypoint,
    u32 arg, u32 stack_top, u32 processor_id, int stack_size=0x4000);

UID __KernelCreateThread(UID module_id, const char* name, u32 priority, u32 entry_point, u32 arg, 
    u32 stack_top, u32 processor_id, int stack_size=0x4000);

void __KernelResetThread(Thread *t, int lowest_priority);
void __KernelChangeReadyState(Thread *thread, UID thread_id, bool ready);
void __KernelChangeReadyState(UID thread_id, bool ready);
Thread* __KernelNextThread();
void __KernelSaveContext(ThreadContext *ctx);
void __KernelLoadContext(ThreadContext *ctx);
void __KernelSwitchContext(Thread *target, const char *reason);
bool __KernelSwitchToThread(UID thread_id, const char *reason);
UID __KernelSetupRootThread(UID module_id, int arg, int prio, int stack_size=0x4000);
int __KernelRotateThreadReadyQueue(int priority=0);

void __KernelThreadingInit();
void __KernelThreadingShutdown();

//const char *__KernelGetThreadName(SceUID threadID);
//
//void __KernelSaveContext(ThreadContext *ctx);
//void __KernelLoadContext(ThreadContext *ctx);

//void __KernelSwitchContext(Thread *target, const char *reason);