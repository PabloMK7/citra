// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <vector>

#include "common/common.h"
#include "common/thread_queue_list.h"

#include "core/core.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/result.h"
#include "core/mem_map.h"

namespace Kernel {

class Thread : public Kernel::Object {
public:

    std::string GetName() const override { return name; }
    std::string GetTypeName() const override { return "Thread"; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Thread; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Thread; }

    inline bool IsRunning() const { return (status & THREADSTATUS_RUNNING) != 0; }
    inline bool IsStopped() const { return (status & THREADSTATUS_DORMANT) != 0; }
    inline bool IsReady() const { return (status & THREADSTATUS_READY) != 0; }
    inline bool IsWaiting() const { return (status & THREADSTATUS_WAIT) != 0; }
    inline bool IsSuspended() const { return (status & THREADSTATUS_SUSPEND) != 0; }

    ResultVal<bool> WaitSynchronization() override {
        const bool wait = status != THREADSTATUS_DORMANT;
        if (wait) {
            Handle thread = GetCurrentThreadHandle();
            if (std::find(waiting_threads.begin(), waiting_threads.end(), thread) == waiting_threads.end()) {
                waiting_threads.push_back(thread);
            }
            WaitCurrentThread(WAITTYPE_THREADEND, this->GetHandle());
        }

        return MakeResult<bool>(wait);
    }

    ThreadContext context;

    u32 thread_id;

    u32 status;
    u32 entry_point;
    u32 stack_top;
    u32 stack_size;

    s32 initial_priority;
    s32 current_priority;

    s32 processor_id;

    WaitType wait_type;
    Handle wait_handle;
    VAddr wait_address;

    std::vector<Handle> waiting_threads;

    std::string name;
};

// Lists all thread ids that aren't deleted/etc.
static std::vector<Handle> thread_queue;

// Lists only ready thread ids.
static Common::ThreadQueueList<Handle> thread_ready_queue;

static Handle current_thread_handle;
static Thread* current_thread;

static const u32 INITIAL_THREAD_ID = 1; ///< The first available thread id at startup
static u32 next_thread_id; ///< The next available thread id

Thread* GetCurrentThread() {
    return current_thread;
}

/// Gets the current thread handle
Handle GetCurrentThreadHandle() {
    return GetCurrentThread()->GetHandle();
}

/// Sets the current thread
inline void SetCurrentThread(Thread* t) {
    current_thread = t;
    current_thread_handle = t->GetHandle();
}

/// Saves the current CPU context
void SaveContext(ThreadContext& ctx) {
    Core::g_app_core->SaveContext(ctx);
}

/// Loads a CPU context
void LoadContext(ThreadContext& ctx) {
    Core::g_app_core->LoadContext(ctx);
}

/// Resets a thread
void ResetThread(Thread* t, u32 arg, s32 lowest_priority) {
    memset(&t->context, 0, sizeof(ThreadContext));

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
    t->wait_handle = 0;
    t->wait_address = 0;
}

/// Change a thread to "ready" state
void ChangeReadyState(Thread* t, bool ready) {
    Handle handle = t->GetHandle();
    if (t->IsReady()) {
        if (!ready) {
            thread_ready_queue.remove(t->current_priority, handle);
        }
    }  else if (ready) {
        if (t->IsRunning()) {
            thread_ready_queue.push_front(t->current_priority, handle);
        } else {
            thread_ready_queue.push_back(t->current_priority, handle);
        }
        t->status = THREADSTATUS_READY;
    }
}

/// Check if a thread is blocking on a specified wait type
static bool CheckWaitType(const Thread* thread, WaitType type) {
    return (type == thread->wait_type) && (thread->IsWaiting());
}

/// Check if a thread is blocking on a specified wait type with a specified handle
static bool CheckWaitType(const Thread* thread, WaitType type, Handle wait_handle) {
    return CheckWaitType(thread, type) && (wait_handle == thread->wait_handle);
}

/// Check if a thread is blocking on a specified wait type with a specified handle and address
static bool CheckWaitType(const Thread* thread, WaitType type, Handle wait_handle, VAddr wait_address) {
    return CheckWaitType(thread, type, wait_handle) && (wait_address == thread->wait_address);
}

/// Stops the current thread
ResultCode StopThread(Handle handle, const char* reason) {
    Thread* thread = g_object_pool.Get<Thread>(handle);
    if (thread == nullptr) return InvalidHandle(ErrorModule::Kernel);

    // Release all the mutexes that this thread holds
    ReleaseThreadMutexes(handle);

    ChangeReadyState(thread, false);
    thread->status = THREADSTATUS_DORMANT;
    for (Handle waiting_handle : thread->waiting_threads) {
        Thread* waiting_thread = g_object_pool.Get<Thread>(waiting_handle);

        if (CheckWaitType(waiting_thread, WAITTYPE_THREADEND, handle))
            ResumeThreadFromWait(waiting_handle);
    }
    thread->waiting_threads.clear();

    // Stopped threads are never waiting.
    thread->wait_type = WAITTYPE_NONE;
    thread->wait_handle = 0;
    thread->wait_address = 0;

    return RESULT_SUCCESS;
}

/// Changes a threads state
void ChangeThreadState(Thread* t, ThreadStatus new_status) {
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
Handle ArbitrateHighestPriorityThread(u32 arbiter, u32 address) {
    Handle highest_priority_thread = 0;
    s32 priority = THREADPRIO_LOWEST;

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated...
    for (Handle handle : thread_queue) {
        Thread* thread = g_object_pool.Get<Thread>(handle);

        if (!CheckWaitType(thread, WAITTYPE_ARB, arbiter, address))
            continue;

        if (thread == nullptr)
            continue; // TODO(yuriks): Thread handle will hang around forever. Should clean up.

        if(thread->current_priority <= priority) {
            highest_priority_thread = handle;
            priority = thread->current_priority;
        }
    }
    // If a thread was arbitrated, resume it
    if (0 != highest_priority_thread)
        ResumeThreadFromWait(highest_priority_thread);

    return highest_priority_thread;
}

/// Arbitrate all threads currently waiting
void ArbitrateAllThreads(u32 arbiter, u32 address) {

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated...
    for (Handle handle : thread_queue) {
        Thread* thread = g_object_pool.Get<Thread>(handle);

        if (CheckWaitType(thread, WAITTYPE_ARB, arbiter, address))
            ResumeThreadFromWait(handle);
    }
}

/// Calls a thread by marking it as "ready" (note: will not actually execute until current thread yields)
void CallThread(Thread* t) {
    // Stop waiting
    if (t->wait_type != WAITTYPE_NONE) {
        t->wait_type = WAITTYPE_NONE;
    }
    ChangeThreadState(t, THREADSTATUS_READY);
}

/// Switches CPU context to that of the specified thread
void SwitchContext(Thread* t) {
    Thread* cur = GetCurrentThread();

    // Save context for current thread
    if (cur) {
        SaveContext(cur->context);

        if (cur->IsRunning()) {
            ChangeReadyState(cur, true);
        }
    }
    // Load context of new thread
    if (t) {
        SetCurrentThread(t);
        ChangeReadyState(t, false);
        t->status = (t->status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;
        t->wait_type = WAITTYPE_NONE;
        LoadContext(t->context);
    } else {
        SetCurrentThread(nullptr);
    }
}

/// Gets the next thread that is ready to be run by priority
Thread* NextThread() {
    Handle next;
    Thread* cur = GetCurrentThread();

    if (cur && cur->IsRunning()) {
        next = thread_ready_queue.pop_first_better(cur->current_priority);
    } else  {
        next = thread_ready_queue.pop_first();
    }
    if (next == 0) {
        return nullptr;
    }
    return Kernel::g_object_pool.Get<Thread>(next);
}

void WaitCurrentThread(WaitType wait_type, Handle wait_handle) {
    Thread* thread = GetCurrentThread();
    thread->wait_type = wait_type;
    thread->wait_handle = wait_handle;
    ChangeThreadState(thread, ThreadStatus(THREADSTATUS_WAIT | (thread->status & THREADSTATUS_SUSPEND)));
}

void WaitCurrentThread(WaitType wait_type, Handle wait_handle, VAddr wait_address) {
    WaitCurrentThread(wait_type, wait_handle);
    GetCurrentThread()->wait_address = wait_address;
}

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle) {
    Thread* thread = Kernel::g_object_pool.Get<Thread>(handle);
    if (thread) {
        thread->status &= ~THREADSTATUS_WAIT;
        thread->wait_handle = 0;
        thread->wait_type = WAITTYPE_NONE;
        if (!(thread->status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD))) {
            ChangeReadyState(thread, true);
        }
    }
}

/// Prints the thread queue for debugging purposes
void DebugThreadQueue() {
    Thread* thread = GetCurrentThread();
    if (!thread) {
        return;
    }
    LOG_DEBUG(Kernel, "0x%02X 0x%08X (current)", thread->current_priority, GetCurrentThreadHandle());
    for (u32 i = 0; i < thread_queue.size(); i++) {
        Handle handle = thread_queue[i];
        s32 priority = thread_ready_queue.contains(handle);
        if (priority != -1) {
            LOG_DEBUG(Kernel, "0x%02X 0x%08X", priority, handle);
        }
    }
}

/// Creates a new thread
Thread* CreateThread(Handle& handle, const char* name, u32 entry_point, s32 priority,
    s32 processor_id, u32 stack_top, int stack_size) {

    _assert_msg_(KERNEL, (priority >= THREADPRIO_HIGHEST && priority <= THREADPRIO_LOWEST),
        "priority=%d, outside of allowable range!", priority)

    Thread* thread = new Thread;

    handle = Kernel::g_object_pool.Create(thread);

    thread_queue.push_back(handle);
    thread_ready_queue.prepare(priority);

    thread->thread_id = next_thread_id++;
    thread->status = THREADSTATUS_DORMANT;
    thread->entry_point = entry_point;
    thread->stack_top = stack_top;
    thread->stack_size = stack_size;
    thread->initial_priority = thread->current_priority = priority;
    thread->processor_id = processor_id;
    thread->wait_type = WAITTYPE_NONE;
    thread->wait_handle = 0;
    thread->wait_address = 0;
    thread->name = name;

    return thread;
}

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, u32 arg, s32 processor_id,
    u32 stack_top, int stack_size) {

    if (name == nullptr) {
        LOG_ERROR(Kernel_SVC, "nullptr name");
        return -1;
    }
    if ((u32)stack_size < 0x200) {
        LOG_ERROR(Kernel_SVC, "(name=%s): invalid stack_size=0x%08X", name,
            stack_size);
        return -1;
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
        return -1;
    }
    Handle handle;
    Thread* thread = CreateThread(handle, name, entry_point, priority, processor_id, stack_top,
        stack_size);

    ResetThread(thread, arg, 0);
    CallThread(thread);

    return handle;
}

/// Get the priority of the thread specified by handle
ResultVal<u32> GetThreadPriority(const Handle handle) {
    Thread* thread = g_object_pool.Get<Thread>(handle);
    if (thread == nullptr) return InvalidHandle(ErrorModule::Kernel);

    return MakeResult<u32>(thread->current_priority);
}

/// Set the priority of the thread specified by handle
ResultCode SetThreadPriority(Handle handle, s32 priority) {
    Thread* thread = nullptr;
    if (!handle) {
        thread = GetCurrentThread(); // TODO(bunnei): Is this correct behavior?
    } else {
        thread = g_object_pool.Get<Thread>(handle);
        if (thread == nullptr) {
            return InvalidHandle(ErrorModule::Kernel);
        }
    }
    _assert_msg_(KERNEL, (thread != nullptr), "called, but thread is nullptr!");

    // If priority is invalid, clamp to valid range
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        LOG_WARNING(Kernel_SVC, "invalid priority=%d, clamping to %d", priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }

    // Change thread priority
    s32 old = thread->current_priority;
    thread_ready_queue.remove(old, handle);
    thread->current_priority = priority;
    thread_ready_queue.prepare(thread->current_priority);

    // Change thread status to "ready" and push to ready queue
    if (thread->IsRunning()) {
        thread->status = (thread->status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;
    }
    if (thread->IsReady()) {
        thread_ready_queue.push_back(thread->current_priority, handle);
    }

    return RESULT_SUCCESS;
}

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size) {
    Handle handle;

    // Initialize new "main" thread
    Thread* thread = CreateThread(handle, "main", Core::g_app_core->GetPC(), priority,
        THREADPROCESSORID_0, Memory::SCRATCHPAD_VADDR_END, stack_size);

    ResetThread(thread, 0, 0);

    // If running another thread already, set it to "ready" state
    Thread* cur = GetCurrentThread();
    if (cur && cur->IsRunning()) {
        ChangeReadyState(cur, true);
    }

    // Run new "main" thread
    SetCurrentThread(thread);
    thread->status = THREADSTATUS_RUNNING;
    LoadContext(thread->context);

    return handle;
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

        for (Handle handle : thread_queue) {
            Thread* thread = g_object_pool.Get<Thread>(handle);
            LOG_TRACE(Kernel, "\thandle=0x%08X prio=0x%02X, status=0x%08X wait_type=0x%08X wait_handle=0x%08X",
                thread->GetHandle(), thread->current_priority, thread->status, thread->wait_type, thread->wait_handle);
        }
    }

    // TODO(bunnei): Hack - There is no timing mechanism yet to wake up a thread if it has been put
    // to sleep. So, we'll just immediately set it to "ready" again after an attempted context
    // switch has occurred. This results in the current thread yielding on a sleep once, and then it
    // will immediately be placed back in the queue for execution.

    if (CheckWaitType(prev, WAITTYPE_SLEEP))
        ResumeThreadFromWait(prev->GetHandle());
}

ResultCode GetThreadId(u32* thread_id, Handle handle) {
    Thread* thread = g_object_pool.Get<Thread>(handle);
    if (thread == nullptr)
        return ResultCode(ErrorDescription::InvalidHandle, ErrorModule::OS, 
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);

    *thread_id = thread->thread_id;

    return RESULT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
    next_thread_id = INITIAL_THREAD_ID;
}

void ThreadingShutdown() {
}

} // namespace
