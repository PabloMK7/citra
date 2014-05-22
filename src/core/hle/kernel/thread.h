// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

enum ThreadPriority {
    THREADPRIO_HIGHEST      = 0,    ///< Highest thread priority
    THREADPRIO_DEFAULT      = 16,   ///< Default thread priority for userland apps
    THREADPRIO_LOW          = 31,   ///< Low range of thread priority for userland apps
    THREADPRIO_LOWEST       = 63,   ///< Thread priority max checked by svcCreateThread
};

enum ThreadProcessorId {
    THREADPROCESSORID_0     = 0xFFFFFFFE,   ///< Enables core appcode
    THREADPROCESSORID_1     = 0xFFFFFFFD,   ///< Enables core syscore
    THREADPROCESSORID_ALL   = 0xFFFFFFFC,   ///< Enables both cores
};

enum ThreadStatus {
    THREADSTATUS_RUNNING        = 1,
    THREADSTATUS_READY          = 2,
    THREADSTATUS_WAIT           = 4,
    THREADSTATUS_SUSPEND        = 8,
    THREADSTATUS_DORMANT        = 16,
    THREADSTATUS_DEAD           = 32,
    THREADSTATUS_WAITSUSPEND    = THREADSTATUS_WAIT | THREADSTATUS_SUSPEND
};

enum WaitType {
    WAITTYPE_NONE,
    WAITTYPE_SLEEP,
    WAITTYPE_SEMA,
    WAITTYPE_EVENTFLAG,
    WAITTYPE_THREADEND,
    WAITTYPE_VBLANK,
    WAITTYPE_MUTEX,
    WAITTYPE_SYNCH,
};

namespace Kernel {

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, u32 arg, s32 processor_id,
    u32 stack_top, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule();

/// Puts a thread in the wait state for the given type/reason
void WaitCurThread(WaitType wait_type, const char* reason);

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle);

/// Gets the current thread handle
Handle GetCurrentThreadHandle();

/// Put current thread in a wait state - on WaitSynchronization
void WaitThread_Synchronization();

/// Initialize threading
void ThreadingInit();

/// Shutdown threading
void ThreadingShutdown();

} // namespace
