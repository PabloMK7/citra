// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

enum ThreadPriority : u32 {
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
    THREADSTATUS_RUNNING,        ///< Currently running
    THREADSTATUS_READY,          ///< Ready to run
    THREADSTATUS_WAIT_ARB,       ///< Waiting on an address arbiter
    THREADSTATUS_WAIT_SLEEP,     ///< Waiting due to a SleepThread SVC
    THREADSTATUS_WAIT_IPC,       ///< Waiting for the reply from an IPC request
    THREADSTATUS_WAIT_SYNCH_ANY, ///< Waiting due to WaitSynch1 or WaitSynchN with wait_all = false
    THREADSTATUS_WAIT_SYNCH_ALL, ///< Waiting due to WaitSynchronizationN with wait_all = true
    THREADSTATUS_WAIT_HLE_EVENT, ///< Waiting due to an HLE handler pausing the thread
    THREADSTATUS_DORMANT,        ///< Created but not yet made ready
    THREADSTATUS_DEAD            ///< Run to completion, or forcefully terminated
};

enum class ThreadWakeupReason {
    Signal, // The thread was woken up by WakeupAllWaitingThreads due to an object signal.
    Timeout // The thread was woken up due to a wait timeout.
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
     * @param owner_process The parent process for the thread
     * @return A shared pointer to the newly created thread
     */
    static ResultVal<SharedPtr<Thread>> Create(std::string name, VAddr entry_point, u32 priority,
                                               u32 arg, s32 processor_id, VAddr stack_top,
                                               SharedPtr<Process> owner_process);

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

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    /**
     * Gets the thread's current priority
     * @return The current thread's priority
     */
    u32 GetPriority() const {
        return current_priority;
    }

    /**
     * Sets the thread's current priority
     * @param priority The new priority
     */
    void SetPriority(u32 priority);

    /**
     * Boost's a thread's priority to the best priority among the thread's held mutexes.
     * This prevents priority inversion via priority inheritance.
     */
    void UpdatePriority();

    /**
     * Temporarily boosts the thread's priority until the next time it is scheduled
     * @param priority The new priority
     */
    void BoostPriority(u32 priority);

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
     * Retrieves the index that this particular object occupies in the list of objects
     * that the thread passed to WaitSynchronizationN, starting the search from the last element.
     * It is used to set the output value of WaitSynchronizationN when the thread is awakened.
     * When a thread wakes up due to an object signal, the kernel will use the index of the last
     * matching object in the wait objects list in case of having multiple instances of the same
     * object in the list.
     * @param object Object to query the index of.
     */
    s32 GetWaitObjectIndex(WaitObject* object) const;

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

    /*
     * Returns the address of the current thread's command buffer, located in the TLS.
     * @returns VAddr of the thread's command buffer.
     */
    VAddr GetCommandBufferAddress() const;

    /**
     * Returns whether this thread is waiting for all the objects in
     * its wait list to become ready, as a result of a WaitSynchronizationN call
     * with wait_all = true.
     */
    bool IsSleepingOnWaitAll() const {
        return status == THREADSTATUS_WAIT_SYNCH_ALL;
    }

    std::unique_ptr<ARM_Interface::ThreadContext> context;

    u32 thread_id;

    u32 status;
    u32 entry_point;
    u32 stack_top;

    u32 nominal_priority; ///< Nominal thread priority, as set by the emulated application
    u32 current_priority; ///< Current thread priority, can be temporarily changed

    u64 last_running_ticks; ///< CPU tick when thread was last running

    s32 processor_id;

    VAddr tls_address; ///< Virtual address of the Thread Local Storage of the thread

    /// Mutexes currently held by this thread, which will be released when it exits.
    boost::container::flat_set<SharedPtr<Mutex>> held_mutexes;

    /// Mutexes that this thread is currently waiting for.
    boost::container::flat_set<SharedPtr<Mutex>> pending_mutexes;

    SharedPtr<Process> owner_process; ///< Process that owns this thread

    /// Objects that the thread is waiting on, in the same order as they were
    // passed to WaitSynchronization1/N.
    std::vector<SharedPtr<WaitObject>> wait_objects;

    VAddr wait_address; ///< If waiting on an AddressArbiter, this is the arbitration address

    std::string name;

    /// Handle used as userdata to reference this object when inserting into the CoreTiming queue.
    Handle callback_handle;

    using WakeupCallback = void(ThreadWakeupReason reason, SharedPtr<Thread> thread,
                                SharedPtr<WaitObject> object);
    // Callback that will be invoked when the thread is resumed from a waiting state. If the thread
    // was waiting via WaitSynchronizationN then the object will be the last object that became
    // available. In case of a timeout, the object will be nullptr.
    std::function<WakeupCallback> wakeup_callback;

private:
    Thread();
    ~Thread() override;
};

/**
 * Sets up the primary application thread
 * @param entry_point The address at which the thread should start execution
 * @param priority The priority to give the main thread
 * @param owner_process The parent process for the main thread
 * @return A shared pointer to the main thread
 */
SharedPtr<Thread> SetupMainThread(u32 entry_point, u32 priority, SharedPtr<Process> owner_process);

/**
 * Returns whether there are any threads that are ready to run.
 */
bool HaveReadyThreads();

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
 * Stops the current thread and removes it from the thread_list
 */
void ExitCurrentThread();

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

} // namespace Kernel
