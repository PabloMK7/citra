// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

enum ThreadStatus {
    THREADSTATUS_RUNNING = 1,
    THREADSTATUS_READY = 2,
    THREADSTATUS_WAIT = 4,
    THREADSTATUS_SUSPEND = 8,
    THREADSTATUS_DORMANT = 16,
    THREADSTATUS_DEAD = 32,

    THREADSTATUS_WAITSUSPEND = THREADSTATUS_WAIT | THREADSTATUS_SUSPEND
};

struct ThreadContext {
    void reset();

    u32 reg[16];
    u32 cpsr;
    u32 pc;
};

void __KernelThreadingInit();
void __KernelThreadingShutdown();

//const char *__KernelGetThreadName(SceUID threadID);
//
//void __KernelSaveContext(ThreadContext *ctx);
//void __KernelLoadContext(ThreadContext *ctx);

//void __KernelSwitchContext(Thread *target, const char *reason);