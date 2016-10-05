// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <boost/container/flat_set.hpp>
#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/result.h"

enum ThreadPriority : s32 {
    THREADPRIO_HIGHEST = 0,       ///< Highest thread priority
    THREADPRIO_USERLAND_MAX = 24, ///< Highest thread priority for userland apps
    THREADPRIO_DEFAULT = 48,      ///< Default thread priority for userland apps
    THREADPRIO_LOWEST = 63,       ///< Lowest thread priority
};

enum ThreadProcessorId : s32 {
    THREADPROCESSORID_DEFAULT = -2, ///< Run thread on default core specified by exheader
    THREADPROCESSORID_ALL = -1,     ///< Run thread on either core
    THREADPROCESSORID_0 = 0,        ///< Run thread on core 0 (AppCore)
    THREADPROCESSORID_1 = 1,        ///< Run thread on core 1 (SysCore)
    THREADPROCESSORID_MAX = 2,      ///< Processor ID must be less than this
};

enum ThreadStatus {
    THREADSTATUS_RUNNING,    ///< Currently running
    THREADSTATUS_READY,      ///< Ready to run
    THREADSTATUS_WAIT_ARB,   ///< Waiting on an address arbiter
    THREADSTATUS_WAIT_SLEEP, ///< Waiting due to a SleepThread SVC
    THREADSTATUS_WAIT_SYNCH, ///< Waiting due to a WaitSynchronization SVC
    THREADSTATUS_DORMANT,    ///< Created but not yet made ready
    THREADSTATUS_DEAD        ///< Run to completion, or forcefully terminated
};

namespace Kernel {

class Mutex;
class Process;

class Thread final : public WaitObject {
public:
    /**
     * Creates and returns a new thread. The new thread is immediately scheduled
     * @param name The friendly name desired for the thread
     * @param entry_point The address at which the thread should start execution
     * @param priority The thread's priority
     * @param arg User data to pass to the thread
     * @param processor_id The ID(s) of the processors on which the thread is desired to be run
     * @param stack_top The address of the thread's stack top
     * @return A shared pointer to the newly created thread
     */
    static ResultVal<SharedPtr<Thread>> Create(std::string name, VAddr entry_point, s32 priority,
                                               u32 arg, s32 processor_id, VAddr stack_top);

    std::string GetName() const override {
        return name;
    }
    std::string GetTypeName() const override {
        return "Thread";
    }

    static const HandleType HANDLE_TYPE = HandleType::Thread;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    bool ShouldWait() override;
    void Acquire() override;

    /**
     * Gets the thread's current priority
     * @return The current thread's priority
     */
    s32 GetPriority() const {
        return current_priority;
    }

    /**
     * Sets the thread's current priority
     * @param priority The new priority
     */
    void SetPriority(s32 priority);

    /**
     * Temporarily boosts the thread's priority until the next time it is scheduled
     * @param priority The new priority
     */
    void BoostPriority(s32 priority);

    /**
     * Gets the thread's thread ID
     * @return The thread's ID
     */
    u32 GetThreadId() const {
        return thread_id;
    }

    /**
     * Resumes a thread from waiting
     */
    void ResumeFromWait();

    /**
    * Schedules an event to wake up the specified thread after the specified delay
    * @param nanoseconds The time this thread will be allowed to sleep for
    */
    void WakeAfterDelay(s64 nanoseconds);

    /**
     * Sets the result after the thread awakens (from either WaitSynchronization SVC)
     * @param result Value to set to the returned result
     */
    void SetWaitSynchronizationResult(ResultCode result);

    /**
     * Sets the output parameter value after the thread awakens (from WaitSynchronizationN SVC only)
     * @param output Value to set to the output parameter
     */
    void SetWaitSynchronizationOutput(s32 output);

    /**
     * Stops a thread, invalidating it from further use
     */
    void Stop();

    /*
     * Returns the Thread Local Storage address of the current thread
     * @returns VAddr of the thread's TLS
     */
    VAddr GetTLSAddress() const {
        return tls_address;
    }

    Core::ThreadContext context;

    u32 thread_id;

    u32 status;
    u32 entry_point;
    u32 stack_top;

    s32 nominal_priority; ///< Nominal thread priority, as set by the emulated application
    s32 current_priority; ///< Current thread priority, can be temporarily changed

    u64 last_running_ticks; ///< CPU tick when thread was last running

    s32 processor_id;

    VAddr tls_address; ///< Virtual address of the Thread Local Storage of the thread

    bool waitsynch_waited; ///< Set to true if the last svcWaitSynch call caused the thread to wait

    /// Mutexes currently held by this thread, which will be released when it exits.
    boost::container::flat_set<SharedPtr<Mutex>> held_mutexes;

    SharedPtr<Process> owner_process;                ///< Process that owns this thread
    std::vector<SharedPtr<WaitObject>> wait_objects; ///< Objects that the thread is waiting on
    VAddr wait_address;   ///< If waiting on an AddressArbiter, this is the arbitration address
    bool wait_all;        ///< True if the thread is waiting on all objects before resuming
    bool wait_set_output; ///< True if the output parameter should be set on thread wakeup

    std::string name;

    /// Handle used as userdata to reference this object when inserting into the CoreTiming queue.
    Handle callback_handle;

private:
    Thread();
    ~Thread() override;
};

/**
 * Sets up the primary application thread
 * @param entry_point The address at which the thread should start execution
 * @param priority The priority to give the main thread
 * @return A shared pointer to the main thread
 */
SharedPtr<Thread> SetupMainThread(u32 entry_point, s32 priority);

/**
 * Reschedules to the next available thread (call after current thread is suspended)
 */
void Reschedule();

/**
 * Arbitrate the highest priority thread that is waiting
 * @param address The address for which waiting threads should be arbitrated
 */
Thread* ArbitrateHighestPriorityThread(u32 address);

/**
 * Arbitrate all threads currently waiting.
 * @param address The address for which waiting threads should be arbitrated
 */
void ArbitrateAllThreads(u32 address);

/**
 * Gets the current thread
 */
Thread* GetCurrentThread();

/**
 * Waits the current thread on a sleep
 */
void WaitCurrentThread_Sleep();

/**
 * Waits the current thread from a WaitSynchronization call
 * @param wait_objects Kernel objects that we are waiting on
 * @param wait_set_output If true, set the output parameter on thread wakeup (for
 * WaitSynchronizationN only)
 * @param wait_all If true, wait on all objects before resuming (for WaitSynchronizationN only)
 */
void WaitCurrentThread_WaitSynchronization(std::vector<SharedPtr<WaitObject>> wait_objects,
                                           bool wait_set_output, bool wait_all);

/**
 * Waits the current thread from an ArbitrateAddress call
 * @param wait_address Arbitration address used to resume from wait
 */
void WaitCurrentThread_ArbitrateAddress(VAddr wait_address);

/**
 * Initialize threading
 */
void ThreadingInit();

/**
 * Shutdown threading
 */
void ThreadingShutdown();

/**
 * Get a const reference to the thread list for debug use
 */
const std::vector<SharedPtr<Thread>>& GetThreadList();

} // namespace
