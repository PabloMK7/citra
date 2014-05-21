// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

enum ThreadPriority {
    THREADPRIO_HIGHEST      = 0,
    THREADPRIO_DEFAULT      = 16,
    THREADPRIO_LOWEST       = 31,
};

enum ThreadProcessorId {
    THREADPROCESSORID_0     = 0xFFFFFFFE,
    THREADPROCESSORID_1     = 0xFFFFFFFD,
    THREADPROCESSORID_ALL   = 0xFFFFFFFC,
};

namespace Kernel {

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, s32 processor_id,
    u32 stack_top, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule(const char* reason);

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle);

/// Gets the current thread
Handle GetCurrentThread();

/// Put current thread in a wait state - on WaitSynchronization
void WaitThread_Synchronization();

/// Initialize threading
void ThreadingInit();

/// Shutdown threading
void ThreadingShutdown();

} // namespace
