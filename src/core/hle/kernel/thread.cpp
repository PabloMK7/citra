// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <stdio.h>

#include <list>
#include <algorithm>
#include <vector>
#include <map>
#include <string>

#include "common/common.h"
#include "common/thread_queue_list.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/svc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Thread : public Kernel::Object {
public:

    const char* GetName() { return name; }
    const char* GetTypeName() { return "Thread"; }

    static Kernel::HandleType GetStaticHandleType() {  return Kernel::HandleType::Thread; }
    Kernel::HandleType GetHandleType() const { return Kernel::HandleType::Thread; }

    inline bool IsRunning() const { return (status & THREADSTATUS_RUNNING) != 0; }
    inline bool IsStopped() const { return (status & THREADSTATUS_DORMANT) != 0; }
    inline bool IsReady() const { return (status & THREADSTATUS_READY) != 0; }
    inline bool IsWaiting() const { return (status & THREADSTATUS_WAIT) != 0; }
    inline bool IsSuspended() const { return (status & THREADSTATUS_SUSPEND) != 0; }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        if (status != THREADSTATUS_DORMANT) {
            Handle thread = GetCurrentThreadHandle();
            if (std::find(waiting_threads.begin(), waiting_threads.end(), thread) == waiting_threads.end()) {
                waiting_threads.push_back(thread);
            }
            WaitCurrentThread(WAITTYPE_THREADEND, this->GetHandle());
            *wait = true;
        }
        return 0;
    }

    ThreadContext context;

    u32 status;
    u32 entry_point;
    u32 stack_top;
    u32 stack_size;

    s32 initial_priority;
    s32 current_priority;

    s32 processor_id;

    WaitType wait_type;
    Handle wait_handle;

    std::vector<Handle> waiting_threads;

    char name[Kernel::MAX_NAME_LENGTH + 1];
};

// Lists all thread ids that aren't deleted/etc.
std::vector<Handle> g_thread_queue;

// Lists only ready thread ids.
Common::ThreadQueueList<Handle> g_thread_ready_queue;

Handle g_current_thread_handle;
Thread* g_current_thread;

/// Gets the current thread
inline Thread* GetCurrentThread() {
    return g_current_thread;
}

/// Gets the current thread handle
Handle GetCurrentThreadHandle() {
    return GetCurrentThread()->GetHandle();
}

/// Sets the current thread
inline void SetCurrentThread(Thread* t) {
    g_current_thread = t;
    g_current_thread_handle = t->GetHandle();
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
    t->context.pc = t->context.cpu_registers[15] = t->entry_point;
    t->context.sp = t->stack_top;
    t->context.cpsr = 0x1F; // Usermode
    
    if (t->current_priority < lowest_priority) {
        t->current_priority = t->initial_priority;
    }
    t->wait_type = WAITTYPE_NONE;
    t->wait_handle = 0;
}

/// Change a thread to "ready" state
void ChangeReadyState(Thread* t, bool ready) {
    Handle handle = t->GetHandle();
    if (t->IsReady()) {
        if (!ready) {
            g_thread_ready_queue.remove(t->current_priority, handle);
        }
    }  else if (ready) {
        if (t->IsRunning()) {
            g_thread_ready_queue.push_front(t->current_priority, handle);
        } else {
            g_thread_ready_queue.push_back(t->current_priority, handle);
        }
        t->status = THREADSTATUS_READY;
    }
}

/// Verify that a thread has not been released from waiting
inline bool VerifyWait(const Handle& thread, WaitType type, Handle handle) {
    Handle wait_id = 0;
    Thread *t = g_object_pool.GetFast<Thread>(thread);
    if (t) {
        if (type == t->wait_type && handle == t->wait_handle) {
            return true;
        }
    } else {
        ERROR_LOG(KERNEL, "thread 0x%08X does not exist", thread);
    }
    return false;
}

/// Stops the current thread
void StopThread(Handle thread, const char* reason) {
    u32 error;
    Thread *t = g_object_pool.Get<Thread>(thread, error);
    if (t) {
        ChangeReadyState(t, false);
        t->status = THREADSTATUS_DORMANT;
        for (size_t i = 0; i < t->waiting_threads.size(); ++i) {
            const Handle waiting_thread = t->waiting_threads[i];
            if (VerifyWait(waiting_thread, WAITTYPE_THREADEND, thread)) {
                ResumeThreadFromWait(waiting_thread);
            }
        }
        t->waiting_threads.clear();

        // Stopped threads are never waiting.
        t->wait_type = WAITTYPE_NONE;
        t->wait_handle = 0;
    } else {
        ERROR_LOG(KERNEL, "thread 0x%08X does not exist", thread);
    }
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
            ERROR_LOG(KERNEL, "Waittype none not allowed");
        }
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
        SetCurrentThread(NULL);
    }
}

/// Gets the next thread that is ready to be run by priority
Thread* NextThread() {
    Handle next;
    Thread* cur = GetCurrentThread();
    
    if (cur && cur->IsRunning()) {
        next = g_thread_ready_queue.pop_first_better(cur->current_priority);
    } else  {
        next = g_thread_ready_queue.pop_first();
    }
    if (next == 0) {
        return NULL;
    }
    return Kernel::g_object_pool.GetFast<Thread>(next);
}

/// Puts the current thread in the wait state for the given type
void WaitCurrentThread(WaitType wait_type, Handle wait_handle) {
    Thread* t = GetCurrentThread();
    t->wait_type = wait_type;
    t->wait_handle = wait_handle;
    ChangeThreadState(t, ThreadStatus(THREADSTATUS_WAIT | (t->status & THREADSTATUS_SUSPEND)));
}

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle) {
    u32 error;
    Thread* t = Kernel::g_object_pool.Get<Thread>(handle, error);
    if (t) {
        t->status &= ~THREADSTATUS_WAIT;
        if (!(t->status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD))) {
            ChangeReadyState(t, true);
        }
    }
}

/// Prints the thread queue for debugging purposes
void DebugThreadQueue() {
    Thread* thread = GetCurrentThread();
    if (!thread) {
        return;
    }
    INFO_LOG(KERNEL, "0x%02X 0x%08X (current)", thread->current_priority, GetCurrentThreadHandle());
    for (u32 i = 0; i < g_thread_queue.size(); i++) {
        Handle handle = g_thread_queue[i];
        s32 priority = g_thread_ready_queue.contains(handle);
        if (priority != -1) {
            INFO_LOG(KERNEL, "0x%02X 0x%08X", priority, handle);
        }
    }
}

/// Creates a new thread
Thread* CreateThread(Handle& handle, const char* name, u32 entry_point, s32 priority,
    s32 processor_id, u32 stack_top, int stack_size) {

    _assert_msg_(KERNEL, (priority >= THREADPRIO_HIGHEST && priority <= THREADPRIO_LOWEST), 
        "CreateThread priority=%d, outside of allowable range!", priority)

    Thread* t = new Thread;

    handle = Kernel::g_object_pool.Create(t);

    g_thread_queue.push_back(handle);
    g_thread_ready_queue.prepare(priority);

    t->status = THREADSTATUS_DORMANT;
    t->entry_point = entry_point;
    t->stack_top = stack_top;
    t->stack_size = stack_size;
    t->initial_priority = t->current_priority = priority;
    t->processor_id = processor_id;
    t->wait_type = WAITTYPE_NONE;
    t->wait_handle = 0;

    strncpy(t->name, name, Kernel::MAX_NAME_LENGTH);
    t->name[Kernel::MAX_NAME_LENGTH] = '\0';

    return t;
}

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, u32 arg, s32 processor_id,
    u32 stack_top, int stack_size) {

    if (name == NULL) {
        ERROR_LOG(KERNEL, "CreateThread(): NULL name");
        return -1;
    }
    if ((u32)stack_size < 0x200) {
        ERROR_LOG(KERNEL, "CreateThread(name=%s): invalid stack_size=0x%08X", name, 
            stack_size);
        return -1;
    }
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        WARN_LOG(KERNEL, "CreateThread(name=%s): invalid priority=0x%08X, clamping to %08X",
            name, priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }
    if (!Memory::GetPointer(entry_point)) {
        ERROR_LOG(KERNEL, "CreateThread(name=%s): invalid entry %08x", name, entry_point);
        return -1;
    }
    Handle handle;
    Thread* t = CreateThread(handle, name, entry_point, priority, processor_id, stack_top, 
        stack_size);

    ResetThread(t, arg, 0);

    HLE::EatCycles(32000);

    CallThread(t);

    // This won't schedule to the new thread, but it may to one woken from eating cycles.
    // Technically, this should not eat all at once, and reschedule in the middle, but that's hard.
    //HLE::Reschedule(__func__);
    
    return handle;
}

/// Get the priority of the thread specified by handle
u32 GetThreadPriority(const Handle handle) {
    Thread* thread = g_object_pool.GetFast<Thread>(handle);
    _assert_msg_(KERNEL, thread, "called, but thread is NULL!");
    return thread->current_priority;
}

/// Set the priority of the thread specified by handle
Result SetThreadPriority(Handle handle, s32 priority) {
    Thread* thread = NULL;
    if (!handle) {
        thread = GetCurrentThread(); // TODO(bunnei): Is this correct behavior?
    } else {
        thread = g_object_pool.GetFast<Thread>(handle);
    }
    _assert_msg_(KERNEL, thread, "called, but thread is NULL!");

    // If priority is invalid, clamp to valid range
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        WARN_LOG(KERNEL, "invalid priority=0x%08X, clamping to %08X", priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }

    // Change thread priority
    s32 old = thread->current_priority;
    g_thread_ready_queue.remove(old, handle);
    thread->current_priority = priority;
    g_thread_ready_queue.prepare(thread->current_priority);

    // Change thread status to "ready" and push to ready queue
    if (thread->IsRunning()) {
        thread->status = (thread->status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;
    }
    if (thread->IsReady()) {
        g_thread_ready_queue.push_back(thread->current_priority, handle);
    }

    HLE::EatCycles(450);

    return 0;
}

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size) {
    Handle handle;
    
    // Initialize new "main" thread
    Thread* t = CreateThread(handle, "main", Core::g_app_core->GetPC(), priority, 
        THREADPROCESSORID_0, Memory::SCRATCHPAD_VADDR_END, stack_size);
    
    ResetThread(t, 0, 0);
    
    // If running another thread already, set it to "ready" state
    Thread* cur = GetCurrentThread();
    if (cur && cur->IsRunning()) {
        ChangeReadyState(cur, true);
    }
    
    // Run new "main" thread
    SetCurrentThread(t);
    t->status = THREADSTATUS_RUNNING;
    LoadContext(t->context);

    return handle;
}


/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule() {
    Thread* prev = GetCurrentThread();
    Thread* next = NextThread();
    HLE::g_reschedule = false;
    if (next > 0) {
        INFO_LOG(KERNEL, "context switch 0x%08X -> 0x%08X", prev->GetHandle(), next->GetHandle());
        
        SwitchContext(next);

        // Hack - There is no mechanism yet to waken the primary thread if it has been put to sleep
        // by a simulated VBLANK thread switch. So, we'll just immediately set it to "ready" again.
        // This results in the current thread yielding on a VBLANK once, and then it will be 
        // immediately placed back in the queue for execution.
        if (prev->wait_type == WAITTYPE_VBLANK) {
            ResumeThreadFromWait(prev->GetHandle());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
}

void ThreadingShutdown() {
}

} // namespace
