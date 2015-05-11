// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/thread_queue_list.h"

#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/result.h"
#include "core/mem_map.h"

namespace Kernel {

/// Event type for the thread wake up event
static int ThreadWakeupEventType;

bool Thread::ShouldWait() {
    return status != THREADSTATUS_DEAD;
}

void Thread::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");
}

// Lists all thread ids that aren't deleted/etc.
static std::vector<SharedPtr<Thread>> thread_list;

// Lists only ready thread ids.
static Common::ThreadQueueList<Thread*, THREADPRIO_LOWEST+1> ready_queue;

static Thread* current_thread;

// The first available thread id at startup
static u32 next_thread_id;

/**
 * Creates a new thread ID
 * @return The new thread ID
 */
inline static u32 const NewThreadId() {
    return next_thread_id++;
}

Thread::Thread() {}
Thread::~Thread() {}

Thread* GetCurrentThread() {
    return current_thread;
}

/**
 * Check if a thread is waiting on the specified wait object
 * @param thread The thread to test
 * @param wait_object The object to test against
 * @return True if the thread is waiting, false otherwise
 */
static bool CheckWait_WaitObject(const Thread* thread, WaitObject* wait_object) {
    if (thread->status != THREADSTATUS_WAIT_SYNCH)
        return false;

    auto itr = std::find(thread->wait_objects.begin(), thread->wait_objects.end(), wait_object);
    return itr != thread->wait_objects.end();
}

/**
 * Check if the specified thread is waiting on the specified address to be arbitrated
 * @param thread The thread to test
 * @param wait_address The address to test against
 * @return True if the thread is waiting, false otherwise
 */
static bool CheckWait_AddressArbiter(const Thread* thread, VAddr wait_address) {
    return thread->status == THREADSTATUS_WAIT_ARB && wait_address == thread->wait_address;
}

void Thread::Stop() {
    // Release all the mutexes that this thread holds
    ReleaseThreadMutexes(this);

    // Cancel any outstanding wakeup events for this thread
    CoreTiming::UnscheduleEvent(ThreadWakeupEventType, callback_handle);

    // Clean up thread from ready queue
    // This is only needed when the thread is termintated forcefully (SVC TerminateProcess)
    if (status == THREADSTATUS_READY){
        ready_queue.remove(current_priority, this);
    }

    status = THREADSTATUS_DEAD;
    
    WakeupAllWaitingThreads();

    // Clean up any dangling references in objects that this thread was waiting for
    for (auto& wait_object : wait_objects) {
        wait_object->RemoveWaitingThread(this);
    }
}

Thread* ArbitrateHighestPriorityThread(u32 address) {
    Thread* highest_priority_thread = nullptr;
    s32 priority = THREADPRIO_LOWEST;

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated...
    for (auto& thread : thread_list) {
        if (!CheckWait_AddressArbiter(thread.get(), address))
            continue;

        if (thread == nullptr)
            continue;

        if(thread->current_priority <= priority) {
            highest_priority_thread = thread.get();
            priority = thread->current_priority;
        }
    }

    // If a thread was arbitrated, resume it
    if (nullptr != highest_priority_thread) {
        highest_priority_thread->ResumeFromWait();
    }

    return highest_priority_thread;
}

void ArbitrateAllThreads(u32 address) {
    // Resume all threads found to be waiting on the address
    for (auto& thread : thread_list) {
        if (CheckWait_AddressArbiter(thread.get(), address))
            thread->ResumeFromWait();
    }
}

/// Boost low priority threads (temporarily) that have been starved
static void PriorityBoostStarvedThreads() {
    u64 current_ticks = CoreTiming::GetTicks();

    for (auto& thread : thread_list) {
        // TODO(bunnei): Threads that have been waiting to be scheduled for `boost_ticks` (or
        // longer) will have their priority temporarily adjusted to 1 higher than the highest
        // priority thread to prevent thread starvation. This general behavior has been verified
        // on hardware. However, this is almost certainly not perfect, and the real CTR OS scheduler
        // should probably be reversed to verify this.

        const u64 boost_timeout = 2000000;  // Boost threads that have been ready for > this long

        u64 delta = current_ticks - thread->last_running_ticks;

        if (thread->status == THREADSTATUS_READY && delta > boost_timeout && !thread->idle) {
            const s32 priority = std::max(ready_queue.get_first()->current_priority - 1, 0);
            thread->BoostPriority(priority);
        }
    }
}

/** 
 * Switches the CPU's active thread context to that of the specified thread
 * @param new_thread The thread to switch to
 */
static void SwitchContext(Thread* new_thread) {
    DEBUG_ASSERT_MSG(new_thread->status == THREADSTATUS_READY, "Thread must be ready to become running.");

    Thread* previous_thread = GetCurrentThread();

    // Save context for previous thread
    if (previous_thread) {
        previous_thread->last_running_ticks = CoreTiming::GetTicks();
        Core::g_app_core->SaveContext(previous_thread->context);

        if (previous_thread->status == THREADSTATUS_RUNNING) {
            // This is only the case when a reschedule is triggered without the current thread
            // yielding execution (i.e. an event triggered, system core time-sliced, etc)
            ready_queue.push_front(previous_thread->current_priority, previous_thread);
            previous_thread->status = THREADSTATUS_READY;
        }
    }

    // Load context of new thread
    if (new_thread) {
        current_thread = new_thread;

        ready_queue.remove(new_thread->current_priority, new_thread);
        new_thread->status = THREADSTATUS_RUNNING;

        // Restores thread to its nominal priority if it has been temporarily changed
        new_thread->current_priority = new_thread->nominal_priority;

        Core::g_app_core->LoadContext(new_thread->context);
        Core::g_app_core->SetCP15Register(CP15_THREAD_URO, new_thread->GetTLSAddress());
    } else {
        current_thread = nullptr;
    }
}

/**
 * Pops and returns the next thread from the thread queue
 * @return A pointer to the next ready thread
 */
static Thread* PopNextReadyThread() {
    Thread* next;
    Thread* thread = GetCurrentThread();

    if (thread && thread->status == THREADSTATUS_RUNNING) {
        // We have to do better than the current thread.
        // This call returns null when that's not possible.
        next = ready_queue.pop_first_better(thread->current_priority);
    } else  {
        next = ready_queue.pop_first();
    }

    return next;
}

void WaitCurrentThread_Sleep() {
    Thread* thread = GetCurrentThread();
    thread->status = THREADSTATUS_WAIT_SLEEP;
}

void WaitCurrentThread_WaitSynchronization(std::vector<SharedPtr<WaitObject>> wait_objects, bool wait_set_output, bool wait_all) {
    Thread* thread = GetCurrentThread();
    thread->wait_set_output = wait_set_output;
    thread->wait_all = wait_all;
    thread->wait_objects = std::move(wait_objects);
    thread->status = THREADSTATUS_WAIT_SYNCH;
}

void WaitCurrentThread_ArbitrateAddress(VAddr wait_address) {
    Thread* thread = GetCurrentThread();
    thread->wait_address = wait_address;
    thread->status = THREADSTATUS_WAIT_ARB;
}

// TODO(yuriks): This can be removed if Thread objects are explicitly pooled in the future, allowing
//               us to simply use a pool index or similar.
static Kernel::HandleTable wakeup_callback_handle_table;

/**
 * Callback that will wake up the thread it was scheduled for
 * @param thread_handle The handle of the thread that's been awoken
 * @param cycles_late The number of CPU cycles that have passed since the desired wakeup time
 */
static void ThreadWakeupCallback(u64 thread_handle, int cycles_late) {
    SharedPtr<Thread> thread = wakeup_callback_handle_table.Get<Thread>((Handle)thread_handle);
    if (thread == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid thread %08X", (Handle)thread_handle);
        return;
    }

    if (thread->status == THREADSTATUS_WAIT_SYNCH) {
        thread->SetWaitSynchronizationResult(ResultCode(ErrorDescription::Timeout, ErrorModule::OS,
                                                        ErrorSummary::StatusChanged, ErrorLevel::Info));

        if (thread->wait_set_output)
            thread->SetWaitSynchronizationOutput(-1);
    }

    thread->ResumeFromWait();
}

void Thread::WakeAfterDelay(s64 nanoseconds) {
    // Don't schedule a wakeup if the thread wants to wait forever
    if (nanoseconds == -1)
        return;

    u64 microseconds = nanoseconds / 1000;
    CoreTiming::ScheduleEvent(usToCycles(microseconds), ThreadWakeupEventType, callback_handle);
}

void Thread::ReleaseWaitObject(WaitObject* wait_object) {
    if (status != THREADSTATUS_WAIT_SYNCH || wait_objects.empty()) {
        LOG_CRITICAL(Kernel, "thread is not waiting on any objects!");
        return;
    }

    // Remove this thread from the waiting object's thread list
    wait_object->RemoveWaitingThread(this);

    unsigned index = 0;
    bool wait_all_failed = false; // Will be set to true if any object is unavailable

    // Iterate through all waiting objects to check availability...
    for (auto itr = wait_objects.begin(); itr != wait_objects.end(); ++itr) {
        if ((*itr)->ShouldWait())
            wait_all_failed = true;

        // The output should be the last index of wait_object
        if (*itr == wait_object)
            index = itr - wait_objects.begin();
    }

    // If we are waiting on all objects...
    if (wait_all) {
        // Resume the thread only if all are available...
        if (!wait_all_failed) {
            SetWaitSynchronizationResult(RESULT_SUCCESS);
            SetWaitSynchronizationOutput(-1);

            ResumeFromWait();
        }
    } else {
        // Otherwise, resume
        SetWaitSynchronizationResult(RESULT_SUCCESS);

        if (wait_set_output)
            SetWaitSynchronizationOutput(index);

        ResumeFromWait();
    }
}

void Thread::ResumeFromWait() {
    // Cancel any outstanding wakeup events for this thread
    CoreTiming::UnscheduleEvent(ThreadWakeupEventType, callback_handle);

    switch (status) {
        case THREADSTATUS_WAIT_SYNCH:
            // Remove this thread from all other WaitObjects
            for (auto wait_object : wait_objects)
                wait_object->RemoveWaitingThread(this);
            break;
        case THREADSTATUS_WAIT_ARB:
        case THREADSTATUS_WAIT_SLEEP:
            break;
        case THREADSTATUS_RUNNING:
        case THREADSTATUS_READY:
            DEBUG_ASSERT_MSG(false, "Thread with object id %u has already resumed.", GetObjectId());
            return;
        case THREADSTATUS_DEAD:
            // This should never happen, as threads must complete before being stopped.
            DEBUG_ASSERT_MSG(false, "Thread with object id %u cannot be resumed because it's DEAD.",
                GetObjectId());
            return;
    }
    
    ready_queue.push_back(current_priority, this);
    status = THREADSTATUS_READY;
}

/**
 * Prints the thread queue for debugging purposes
 */
static void DebugThreadQueue() {
    Thread* thread = GetCurrentThread();
    if (!thread) {
        LOG_DEBUG(Kernel, "Current: NO CURRENT THREAD");
    } else {
        LOG_DEBUG(Kernel, "0x%02X %u (current)", thread->current_priority, GetCurrentThread()->GetObjectId());
    }

    for (auto& t : thread_list) {
        s32 priority = ready_queue.contains(t.get());
        if (priority != -1) {
            LOG_DEBUG(Kernel, "0x%02X %u", priority, t->GetObjectId());
        }
    }
}

ResultVal<SharedPtr<Thread>> Thread::Create(std::string name, VAddr entry_point, s32 priority,
        u32 arg, s32 processor_id, VAddr stack_top) {
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = MathUtil::Clamp<s32>(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        LOG_WARNING(Kernel_SVC, "(name=%s): invalid priority=%d, clamping to %d",
            name.c_str(), priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }

    if (!Memory::GetPointer(entry_point)) {
        LOG_ERROR(Kernel_SVC, "(name=%s): invalid entry %08x", name.c_str(), entry_point);
        // TODO: Verify error
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    SharedPtr<Thread> thread(new Thread);

    thread_list.push_back(thread);
    ready_queue.prepare(priority);

    thread->thread_id = NewThreadId();
    thread->status = THREADSTATUS_DORMANT;
    thread->entry_point = entry_point;
    thread->stack_top = stack_top;
    thread->nominal_priority = thread->current_priority = priority;
    thread->last_running_ticks = CoreTiming::GetTicks();
    thread->processor_id = processor_id;
    thread->wait_set_output = false;
    thread->wait_all = false;
    thread->wait_objects.clear();
    thread->wait_address = 0;
    thread->name = std::move(name);
    thread->callback_handle = wakeup_callback_handle_table.Create(thread).MoveFrom();

    VAddr tls_address = Memory::TLS_AREA_VADDR + (thread->thread_id - 1) * 0x200;

    ASSERT_MSG(tls_address < Memory::TLS_AREA_VADDR_END, "Too many threads");

    thread->tls_address = tls_address;

    // TODO(peachum): move to ScheduleThread() when scheduler is added so selected core is used
    // to initialize the context
    Core::g_app_core->ResetContext(thread->context, stack_top, entry_point, arg);

    ready_queue.push_back(thread->current_priority, thread.get());
    thread->status = THREADSTATUS_READY;

    return MakeResult<SharedPtr<Thread>>(std::move(thread));
}

// TODO(peachum): Remove this. Range checking should be done, and an appropriate error should be returned.
static void ClampPriority(const Thread* thread, s32* priority) {
    if (*priority < THREADPRIO_HIGHEST || *priority > THREADPRIO_LOWEST) {
        DEBUG_ASSERT_MSG(false, "Application passed an out of range priority. An error should be returned.");

        s32 new_priority = MathUtil::Clamp<s32>(*priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        LOG_WARNING(Kernel_SVC, "(name=%s): invalid priority=%d, clamping to %d",
                    thread->name.c_str(), *priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        *priority = new_priority;
    }
}

void Thread::SetPriority(s32 priority) {
    ClampPriority(this, &priority);

    // If thread was ready, adjust queues
    if (status == THREADSTATUS_READY)
        ready_queue.move(this, current_priority, priority);

    nominal_priority = current_priority = priority;
}

void Thread::BoostPriority(s32 priority) {
    ready_queue.move(this, current_priority, priority);
    current_priority = priority;
}

SharedPtr<Thread> SetupIdleThread() {
    // We need to pass a few valid values to get around parameter checking in Thread::Create.
    // TODO(yuriks): Figure out a way to avoid passing the bogus VAddr parameter
    auto thread = Thread::Create("idle", Memory::TLS_AREA_VADDR, THREADPRIO_LOWEST, 0,
            THREADPROCESSORID_0, 0).MoveFrom();

    thread->idle = true;
    return thread;
}

SharedPtr<Thread> SetupMainThread(u32 entry_point, s32 priority) {
    DEBUG_ASSERT(!GetCurrentThread());

    // Initialize new "main" thread
    auto thread_res = Thread::Create("main", entry_point, priority, 0,
            THREADPROCESSORID_0, Memory::HEAP_VADDR_END);

    SharedPtr<Thread> thread = thread_res.MoveFrom();

    // Run new "main" thread
    SwitchContext(thread.get());

    return thread;
}

void Reschedule() {
    Thread* prev = GetCurrentThread();

    PriorityBoostStarvedThreads();

    Thread* next = PopNextReadyThread();
    HLE::g_reschedule = false;

    if (next != nullptr) {
        LOG_TRACE(Kernel, "context switch %u -> %u", prev->GetObjectId(), next->GetObjectId());
        SwitchContext(next);
    } else {
        LOG_TRACE(Kernel, "cannot context switch from %u, no higher priority thread!", prev->GetObjectId());

        for (auto& thread : thread_list) {
            LOG_TRACE(Kernel, "\tid=%u prio=0x%02X, status=0x%08X", thread->GetObjectId(), 
                      thread->current_priority, thread->status);
        }
    }
}

void Thread::SetWaitSynchronizationResult(ResultCode result) {
    context.cpu_registers[0] = result.raw;
}

void Thread::SetWaitSynchronizationOutput(s32 output) {
    context.cpu_registers[1] = output;
}

VAddr Thread::GetTLSAddress() const {
    return tls_address;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
    ThreadWakeupEventType = CoreTiming::RegisterEvent("ThreadWakeupCallback", ThreadWakeupCallback);

    current_thread = nullptr;
    next_thread_id = 1;

    thread_list.clear();
    ready_queue.clear();

    // Setup the idle thread
    SetupIdleThread();
}

void ThreadingShutdown() {
}

} // namespace
