// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"

#include "core/core.h"
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

class Thread : public Kernel::Object {
public:
    static ResultVal<SharedPtr<Thread>> Create(std::string name, VAddr entry_point, s32 priority,
        u32 arg, s32 processor_id, VAddr stack_top, u32 stack_size);

    std::string GetName() const override { return name; }
    std::string GetTypeName() const override { return "Thread"; }

    static const HandleType HANDLE_TYPE = HandleType::Thread;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    inline bool IsRunning() const { return (status & THREADSTATUS_RUNNING) != 0; }
    inline bool IsStopped() const { return (status & THREADSTATUS_DORMANT) != 0; }
    inline bool IsReady() const { return (status & THREADSTATUS_READY) != 0; }
    inline bool IsWaiting() const { return (status & THREADSTATUS_WAIT) != 0; }
    inline bool IsSuspended() const { return (status & THREADSTATUS_SUSPEND) != 0; }
    inline bool IsIdle() const { return idle; }

    ResultVal<bool> WaitSynchronization() override;

    s32 GetPriority() const { return current_priority; }
    void SetPriority(s32 priority);

    u32 GetThreadId() const { return thread_id; }

    void Stop(const char* reason);
    /// Resumes a thread from waiting by marking it as "ready".
    void ResumeFromWait();

    Core::ThreadContext context;

    u32 thread_id;

    u32 status;
    u32 entry_point;
    u32 stack_top;
    u32 stack_size;

    s32 initial_priority;
    s32 current_priority;

    s32 processor_id;

    WaitType wait_type;
    Object* wait_object;
    VAddr wait_address;

    std::vector<SharedPtr<Thread>> waiting_threads;

    std::string name;

    /// Whether this thread is intended to never actually be executed, i.e. always idle
    bool idle = false;

private:
    Thread() = default;
};

/// Sets up the primary application thread
SharedPtr<Thread> SetupMainThread(s32 priority, u32 stack_size);

/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule();

/// Arbitrate the highest priority thread that is waiting
Thread* ArbitrateHighestPriorityThread(Object* arbiter, u32 address);

/// Arbitrate all threads currently waiting...
void ArbitrateAllThreads(Object* arbiter, u32 address);

/// Gets the current thread
Thread* GetCurrentThread();

/**
 * Puts the current thread in the wait state for the given type
 * @param wait_type Type of wait
 * @param wait_object Kernel object that we are waiting on, defaults to current thread
 */
void WaitCurrentThread(WaitType wait_type, Object* wait_object = GetCurrentThread());

/**
 * Schedules an event to wake up the specified thread after the specified delay.
 * @param thread The thread to wake after the delay.
 * @param nanoseconds The time this thread will be allowed to sleep for.
 */
void WakeThreadAfterDelay(Thread* thread, s64 nanoseconds);

/**
 * Puts the current thread in the wait state for the given type
 * @param wait_type Type of wait
 * @param wait_object Kernel object that we are waiting on
 * @param wait_address Arbitration address used to resume from wait
 */
void WaitCurrentThread(WaitType wait_type, Object* wait_object, VAddr wait_address);



/**
 * Sets up the idle thread, this is a thread that is intended to never execute instructions,
 * only to advance the timing. It is scheduled when there are no other ready threads in the thread queue
 * and will try to yield on every call.
 * @returns The handle of the idle thread
 */
Handle SetupIdleThread();
/// Initialize threading
void ThreadingInit();

/// Shutdown threading
void ThreadingShutdown();

} // namespace
