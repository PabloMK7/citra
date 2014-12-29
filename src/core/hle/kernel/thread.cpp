// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <vector>

#include "common/common.h"
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

ResultVal<bool> Thread::WaitSynchronization() {
    const bool wait = status != THREADSTATUS_DORMANT;
    if (wait) {
        Thread* thread = GetCurrentThread();
        if (std::find(waiting_threads.begin(), waiting_threads.end(), thread) == waiting_threads.end()) {
            waiting_threads.push_back(thread);
        }
        WaitCurrentThread(WAITTYPE_THREADEND, this);
    }

    return MakeResult<bool>(wait);
}

// Lists all thread ids that aren't deleted/etc.
static std::vector<Thread*> thread_list; // TODO(yuriks): Owned

// Lists only ready thread ids.
static Common::ThreadQueueList<Thread*, THREADPRIO_LOWEST+1> thread_ready_queue;

static Thread* current_thread;

static const u32 INITIAL_THREAD_ID = 1; ///< The first available thread id at startup
static u32 next_thread_id; ///< The next available thread id

Thread* GetCurrentThread() {
    return current_thread;
}

/// Resets a thread
static void ResetThread(Thread* t, u32 arg, s32 lowest_priority) {
    memset(&t->context, 0, sizeof(Core::ThreadContext));

    t->context.cpu_registers[0] = arg;
    t->context.pc = t->context.reg_15 = t->entry_point;
    t->context.sp = t->stack_top;
    t->context.cpsr = 0x1F; // Usermode

    // TODO(bunnei): This instructs the CPU core to start the execution as if it is "resuming" a
    // thread. This is somewhat Sky-Eye specific, and should be re-architected in the future to be
    // agnostic of the CPU core.
    t->context.mode = 8;

    if (t->current_priority < lowest_priority) {
        t->current_priority = t->initial_priority;
    }
    t->wait_type = WAITTYPE_NONE;
    t->wait_object = nullptr;
    t->wait_address = 0;
}

/// Change a thread to "ready" state
static void ChangeReadyState(Thread* t, bool ready) {
    if (t->IsReady()) {
        if (!ready) {
            thread_ready_queue.remove(t->current_priority, t);
        }
    }  else if (ready) {
        if (t->IsRunning()) {
            thread_ready_queue.push_front(t->current_priority, t);
        } else {
            thread_ready_queue.push_back(t->current_priority, t);
        }
        t->status = THREADSTATUS_READY;
    }
}

/// Check if a thread is blocking on a specified wait type
static bool CheckWaitType(const Thread* thread, WaitType type) {
    return (type == thread->wait_type) && (thread->IsWaiting());
}

/// Check if a thread is blocking on a specified wait type with a specified handle
static bool CheckWaitType(const Thread* thread, WaitType type, Object* wait_object) {
    return CheckWaitType(thread, type) && wait_object == thread->wait_object;
}

/// Check if a thread is blocking on a specified wait type with a specified handle and address
static bool CheckWaitType(const Thread* thread, WaitType type, Object* wait_object, VAddr wait_address) {
    return CheckWaitType(thread, type, wait_object) && (wait_address == thread->wait_address);
}

/// Stops the current thread
void Thread::Stop(const char* reason) {
    // Release all the mutexes that this thread holds
    ReleaseThreadMutexes(GetHandle());

    ChangeReadyState(this, false);
    status = THREADSTATUS_DORMANT;
    for (Thread* waiting_thread : waiting_threads) {
        if (CheckWaitType(waiting_thread, WAITTYPE_THREADEND, this))
            waiting_thread->ResumeFromWait();
    }
    waiting_threads.clear();

    // Stopped threads are never waiting.
    wait_type = WAITTYPE_NONE;
    wait_object = nullptr;
    wait_address = 0;
}

/// Changes a threads state
static void ChangeThreadState(Thread* t, ThreadStatus new_status) {
    if (!t || t->status == new_status) {
        return;
    }
    ChangeReadyState(t, (new_status & THREADSTATUS_READY) != 0);
    t->status = new_status;

    if (new_status == THREADSTATUS_WAIT) {
        if (t->wait_type == WAITTYPE_NONE) {
            LOG_ERROR(Kernel, "Waittype none not allowed");
        }
    }
}

/// Arbitrate the highest priority thread that is waiting
Thread* ArbitrateHighestPriorityThread(Object* arbiter, u32 address) {
    Thread* highest_priority_thread = nullptr;
    s32 priority = THREADPRIO_LOWEST;

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated...
    for (Thread* thread : thread_list) {
        if (!CheckWaitType(thread, WAITTYPE_ARB, arbiter, address))
            continue;

        if (thread == nullptr)
            continue; // TODO(yuriks): Thread handle will hang around forever. Should clean up.

        if(thread->current_priority <= priority) {
            highest_priority_thread = thread;
            priority = thread->current_priority;
        }
    }

    // If a thread was arbitrated, resume it
    if (nullptr != highest_priority_thread) {
        highest_priority_thread->ResumeFromWait();
    }

    return highest_priority_thread;
}

/// Arbitrate all threads currently waiting
void ArbitrateAllThreads(Object* arbiter, u32 address) {

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated...
    for (Thread* thread : thread_list) {
        if (CheckWaitType(thread, WAITTYPE_ARB, arbiter, address))
            thread->ResumeFromWait();
    }
}

/// Calls a thread by marking it as "ready" (note: will not actually execute until current thread yields)
static void CallThread(Thread* t) {
    // Stop waiting
    if (t->wait_type != WAITTYPE_NONE) {
        t->wait_type = WAITTYPE_NONE;
    }
    ChangeThreadState(t, THREADSTATUS_READY);
}

/// Switches CPU context to that of the specified thread
static void SwitchContext(Thread* t) {
    Thread* cur = GetCurrentThread();

    // Save context for current thread
    if (cur) {
        Core::g_app_core->SaveContext(cur->context);

        if (cur->IsRunning()) {
            ChangeReadyState(cur, true);
        }
    }
    // Load context of new thread
    if (t) {
        current_thread = t;
        ChangeReadyState(t, false);
        t->status = (t->status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;
        t->wait_type = WAITTYPE_NONE;
        Core::g_app_core->LoadContext(t->context);
    } else {
        current_thread = nullptr;
    }
}

/// Gets the next thread that is ready to be run by priority
static Thread* NextThread() {
    Thread* next;
    Thread* cur = GetCurrentThread();

    if (cur && cur->IsRunning()) {
        next = thread_ready_queue.pop_first_better(cur->current_priority);
    } else  {
        next = thread_ready_queue.pop_first();
    }
    if (next == 0) {
        return nullptr;
    }
    return next;
}

void WaitCurrentThread(WaitType wait_type, Object* wait_object) {
    Thread* thread = GetCurrentThread();
    thread->wait_type = wait_type;
    thread->wait_object = wait_object;
    ChangeThreadState(thread, ThreadStatus(THREADSTATUS_WAIT | (thread->status & THREADSTATUS_SUSPEND)));
}

void WaitCurrentThread(WaitType wait_type, Object* wait_object, VAddr wait_address) {
    WaitCurrentThread(wait_type, wait_object);
    GetCurrentThread()->wait_address = wait_address;
}

/// Event type for the thread wake up event
static int ThreadWakeupEventType = -1;

/// Callback that will wake up the thread it was scheduled for
static void ThreadWakeupCallback(u64 parameter, int cycles_late) {
    Handle handle = static_cast<Handle>(parameter);
    Thread* thread = Kernel::g_handle_table.Get<Thread>(handle);
    if (thread == nullptr) {
        LOG_ERROR(Kernel, "Thread doesn't exist %u", handle);
        return;
    }

    thread->ResumeFromWait();
}


void WakeThreadAfterDelay(Thread* thread, s64 nanoseconds) {
    // Don't schedule a wakeup if the thread wants to wait forever
    if (nanoseconds == -1)
        return;
    _dbg_assert_(Kernel, thread != nullptr);

    u64 microseconds = nanoseconds / 1000;
    CoreTiming::ScheduleEvent(usToCycles(microseconds), ThreadWakeupEventType, thread->GetHandle());
}

/// Resumes a thread from waiting by marking it as "ready"
void Thread::ResumeFromWait() {
    status &= ~THREADSTATUS_WAIT;
    wait_object = nullptr;
    wait_type = WAITTYPE_NONE;
    if (!(status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD))) {
        ChangeReadyState(this, true);
    }
}

/// Prints the thread queue for debugging purposes
static void DebugThreadQueue() {
    Thread* thread = GetCurrentThread();
    if (!thread) {
        return;
    }
    LOG_DEBUG(Kernel, "0x%02X 0x%08X (current)", thread->current_priority, GetCurrentThread()->GetHandle());
    for (Thread* t : thread_list) {
        s32 priority = thread_ready_queue.contains(t);
        if (priority != -1) {
            LOG_DEBUG(Kernel, "0x%02X 0x%08X", priority, t->GetHandle());
        }
    }
}

ResultVal<Thread*> Thread::Create(const char* name, u32 entry_point, s32 priority, u32 arg,
        s32 processor_id, u32 stack_top, int stack_size) {
    _dbg_assert_(Kernel, name != nullptr);

    if ((u32)stack_size < 0x200) {
        LOG_ERROR(Kernel, "(name=%s): invalid stack_size=0x%08X", name, stack_size);
        // TODO: Verify error
        return ResultCode(ErrorDescription::InvalidSize, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        LOG_WARNING(Kernel_SVC, "(name=%s): invalid priority=%d, clamping to %d",
            name, priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }

    if (!Memory::GetPointer(entry_point)) {
        LOG_ERROR(Kernel_SVC, "(name=%s): invalid entry %08x", name, entry_point);
        // TODO: Verify error
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    Thread* thread = new Thread;

    // TODO(yuriks): Thread requires a handle to be inserted into the various scheduling queues for
    //               the time being. Create a handle here, it will be copied to the handle field in
    //               the object and use by the rest of the code. This should be removed when other
    //               code doesn't rely on the handle anymore.
    ResultVal<Handle> handle = Kernel::g_handle_table.Create(thread);
    // TODO(yuriks): Plug memory leak
    if (handle.Failed())
        return handle.Code();

    thread_list.push_back(thread);
    thread_ready_queue.prepare(priority);

    thread->thread_id = next_thread_id++;
    thread->status = THREADSTATUS_DORMANT;
    thread->entry_point = entry_point;
    thread->stack_top = stack_top;
    thread->stack_size = stack_size;
    thread->initial_priority = thread->current_priority = priority;
    thread->processor_id = processor_id;
    thread->wait_type = WAITTYPE_NONE;
    thread->wait_object = nullptr;
    thread->wait_address = 0;
    thread->name = name;

    ResetThread(thread, arg, 0);
    CallThread(thread);

    return MakeResult<Thread*>(thread);
}

/// Set the priority of the thread specified by handle
void Thread::SetPriority(s32 priority) {
    // If priority is invalid, clamp to valid range
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        LOG_WARNING(Kernel_SVC, "invalid priority=%d, clamping to %d", priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }

    // Change thread priority
    s32 old = current_priority;
    thread_ready_queue.remove(old, this);
    current_priority = priority;
    thread_ready_queue.prepare(current_priority);

    // Change thread status to "ready" and push to ready queue
    if (IsRunning()) {
        status = (status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;
    }
    if (IsReady()) {
        thread_ready_queue.push_back(current_priority, this);
    }
}

Handle SetupIdleThread() {
    // We need to pass a few valid values to get around parameter checking in Thread::Create.
    auto thread_res = Thread::Create("idle", Memory::KERNEL_MEMORY_VADDR, THREADPRIO_LOWEST, 0,
            THREADPROCESSORID_0, 0, Kernel::DEFAULT_STACK_SIZE);
    _dbg_assert_(Kernel, thread_res.Succeeded());
    Thread* thread = *thread_res;

    thread->idle = true;
    CallThread(thread);
    return thread->GetHandle();
}

Thread* SetupMainThread(s32 priority, int stack_size) {
    // Initialize new "main" thread
    ResultVal<Thread*> thread_res = Thread::Create("main", Core::g_app_core->GetPC(), priority, 0,
        THREADPROCESSORID_0, Memory::SCRATCHPAD_VADDR_END, stack_size);
    // TODO(yuriks): Propagate error
    _dbg_assert_(Kernel, thread_res.Succeeded());
    Thread* thread = *thread_res;

    // If running another thread already, set it to "ready" state
    Thread* cur = GetCurrentThread();
    if (cur && cur->IsRunning()) {
        ChangeReadyState(cur, true);
    }

    // Run new "main" thread
    current_thread = thread;
    thread->status = THREADSTATUS_RUNNING;
    Core::g_app_core->LoadContext(thread->context);

    return thread;
}


/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule() {
    Thread* prev = GetCurrentThread();
    Thread* next = NextThread();
    HLE::g_reschedule = false;

    if (next != nullptr) {
        LOG_TRACE(Kernel, "context switch 0x%08X -> 0x%08X", prev->GetHandle(), next->GetHandle());
        SwitchContext(next);
    } else {
        LOG_TRACE(Kernel, "cannot context switch from 0x%08X, no higher priority thread!", prev->GetHandle());

        for (Thread* thread : thread_list) {
            LOG_TRACE(Kernel, "\thandle=0x%08X prio=0x%02X, status=0x%08X wait_type=0x%08X wait_handle=0x%08X",
                thread->GetHandle(), thread->current_priority, thread->status, thread->wait_type,
                (thread->wait_object ? thread->wait_object->GetHandle() : INVALID_HANDLE));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
    next_thread_id = INITIAL_THREAD_ID;
    ThreadWakeupEventType = CoreTiming::RegisterEvent("ThreadWakeupCallback", ThreadWakeupCallback);
}

void ThreadingShutdown() {
}

} // namespace
