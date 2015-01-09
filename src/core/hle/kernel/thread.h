// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/mem_map.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/result.h"

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
    WAITTYPE_EVENT,
    WAITTYPE_THREADEND,
    WAITTYPE_MUTEX,
    WAITTYPE_SYNCH,
    WAITTYPE_ARB,
    WAITTYPE_TIMER,
};

namespace Kernel {

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, u32 arg, s32 processor_id,
    u32 stack_top, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size=Kernel::DEFAULT_STACK_SIZE);

/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule();

/// Stops the current thread
ResultCode StopThread(Handle thread, const char* reason);

/**
 * Retrieves the ID of the specified thread handle
 * @param thread_id Will contain the output thread id
 * @param handle Handle to the thread we want
 * @return Whether the function was successful or not
 */
ResultCode GetThreadId(u32* thread_id, Handle handle);

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle);

/// Arbitrate the highest priority thread that is waiting
Handle ArbitrateHighestPriorityThread(u32 arbiter, u32 address);

/// Arbitrate all threads currently waiting...
void ArbitrateAllThreads(u32 arbiter, u32 address);

/// Gets the current thread handle
Handle GetCurrentThreadHandle();

/**
 * Puts the current thread in the wait state for the given type
 * @param wait_type Type of wait
 * @param wait_handle Handle of Kernel object that we are waiting on, defaults to current thread
 */
void WaitCurrentThread(WaitType wait_type, Handle wait_handle=GetCurrentThreadHandle());

/**
 * Schedules an event to wake up the specified thread after the specified delay.
 * @param handle The thread handle.
 * @param nanoseconds The time this thread will be allowed to sleep for.
 */
void WakeThreadAfterDelay(Handle handle, s64 nanoseconds);

/**
 * Puts the current thread in the wait state for the given type
 * @param wait_type Type of wait
 * @param wait_handle Handle of Kernel object that we are waiting on, defaults to current thread
 * @param wait_address Arbitration address used to resume from wait
 */
void WaitCurrentThread(WaitType wait_type, Handle wait_handle, VAddr wait_address);

/// Put current thread in a wait state - on WaitSynchronization
void WaitThread_Synchronization();

/// Get the priority of the thread specified by handle
ResultVal<u32> GetThreadPriority(const Handle handle);

/// Set the priority of the thread specified by handle
ResultCode SetThreadPriority(Handle handle, s32 priority);

/**
 * Sets up the idle thread, this is a thread that is intended to never execute instructions,
 * only to advance the timing. It is scheduled when there are no other ready threads in the thread queue
 * and will try to yield on every call.
 * @returns The handle of the idle thread
 */
Handle SetupIdleThread();

/// Whether the current thread is an idle thread
bool IsIdleThread(Handle thread);

/// Initialize threading
void ThreadingInit();

/// Shutdown threading
void ThreadingShutdown();

} // namespace
