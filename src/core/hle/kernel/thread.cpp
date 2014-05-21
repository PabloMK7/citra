// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <stdio.h>

#include <list>
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

    ThreadContext context;

    u32 status;
    u32 entry_point;
    u32 stack_top;
    u32 stack_size;

    s32 initial_priority;
    s32 current_priority;

    s32 processor_id;

    WaitType wait_type;

    char name[Kernel::MAX_NAME_LENGTH + 1];
};

// Lists all thread ids that aren't deleted/etc.
std::vector<Handle> g_thread_queue;

// Lists only ready thread ids.
Common::ThreadQueueList<Handle> g_thread_ready_queue;

Handle g_current_thread_handle;
Thread* g_current_thread;


/// Gets the current thread
inline Thread* __GetCurrentThread() {
    return g_current_thread;
}

/// Sets the current thread
inline void __SetCurrentThread(Thread* t) {
    g_current_thread = t;
    g_current_thread_handle = t->GetHandle();
}

/// Saves the current CPU context
void __SaveContext(ThreadContext& ctx) {
    Core::g_app_core->SaveContext(ctx);
}

/// Loads a CPU context
void __LoadContext(const ThreadContext& ctx) {
    Core::g_app_core->LoadContext(ctx);
}

/// Resets a thread
void __ResetThread(Thread* t, s32 lowest_priority) {
    memset(&t->context, 0, sizeof(ThreadContext));

    t->context.pc = t->entry_point;
    t->context.sp = t->stack_top;
    
    if (t->current_priority < lowest_priority) {
        t->current_priority = t->initial_priority;
    }
        
    t->wait_type = WAITTYPE_NONE;
}

/// Change a thread to "ready" state
void __ChangeReadyState(Thread* t, bool ready) {
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

/// Changes a threads state
void __ChangeThreadState(Thread* t, ThreadStatus new_status) {
    if (!t || t->status == new_status) {
        return;
    }
    __ChangeReadyState(t, (new_status & THREADSTATUS_READY) != 0);
    t->status = new_status;
    
    if (new_status == THREADSTATUS_WAIT) {
        if (t->wait_type == WAITTYPE_NONE) {
            printf("ERROR: Waittype none not allowed here\n");
        }
    }
}

/// Calls a thread by marking it as "ready" (note: will not actually execute until current thread yields)
void __CallThread(Thread* t) {
    // Stop waiting
    if (t->wait_type != WAITTYPE_NONE) {
        t->wait_type = WAITTYPE_NONE;
    }
    __ChangeThreadState(t, THREADSTATUS_READY);
}

/// Switches CPU context to that of the specified thread
void __SwitchContext(Thread* t, const char* reason) {
    Thread* cur = __GetCurrentThread();
    
    // Save context for current thread
    if (cur) {
        __SaveContext(cur->context);
        
        if (cur->IsRunning()) {
            __ChangeReadyState(cur, true);
        }
    }
    // Load context of new thread
    if (t) {
        __SetCurrentThread(t);
        __ChangeReadyState(t, false);
        t->status = (t->status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;
        t->wait_type = WAITTYPE_NONE;
        __LoadContext(t->context);
    } else {
        __SetCurrentThread(NULL);
    }
}

/// Gets the next thread that is ready to be run by priority
Thread* __NextThread() {
    Handle next;
    Thread* cur = __GetCurrentThread();
    
    if (cur && cur->IsRunning()) {
        next = g_thread_ready_queue.pop_first_better(cur->current_priority);
    } else  {
        next = g_thread_ready_queue.pop_first();
    }
    if (next < 0) {
        return NULL;
    }
    return Kernel::g_object_pool.GetFast<Thread>(next);
}

/// Puts a thread in the wait state for the given type/reason
void __WaitCurThread(WaitType wait_type, const char* reason) {
    Thread* t = __GetCurrentThread();
    t->wait_type = wait_type;
    __ChangeThreadState(t, ThreadStatus(THREADSTATUS_WAIT | (t->status & THREADSTATUS_SUSPEND)));
}

/// Resumes a thread from waiting by marking it as "ready"
void ResumeThreadFromWait(Handle handle) {
    u32 error;
    Thread* t = Kernel::g_object_pool.Get<Thread>(handle, error);
    if (t) {
        t->status &= ~THREADSTATUS_WAIT;
        if (!(t->status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD))) {
            __ChangeReadyState(t, true);
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
    
    strncpy(t->name, name, Kernel::MAX_NAME_LENGTH);
    t->name[Kernel::MAX_NAME_LENGTH] = '\0';
    
    return t;
}

/// Creates a new thread - wrapper for external user
Handle CreateThread(const char* name, u32 entry_point, s32 priority, s32 processor_id,
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

    HLE::EatCycles(32000);

    // This won't schedule to the new thread, but it may to one woken from eating cycles.
    // Technically, this should not eat all at once, and reschedule in the middle, but that's hard.
    HLE::ReSchedule("thread created");

    __CallThread(t);
    
    return handle;
}

/// Gets the current thread
Handle GetCurrentThread() {
    return __GetCurrentThread()->GetHandle();
}

/// Sets up the primary application thread
Handle SetupMainThread(s32 priority, int stack_size) {
    Handle handle;
    
    // Initialize new "main" thread
    Thread* t = CreateThread(handle, "main", Core::g_app_core->GetPC(), priority, 
        THREADPROCESSORID_0, Memory::SCRATCHPAD_VADDR_END, stack_size);
    
    __ResetThread(t, 0);
    
    // If running another thread already, set it to "ready" state
    Thread* cur = __GetCurrentThread();
    if (cur && cur->IsRunning()) {
        __ChangeReadyState(cur, true);
    }
    
    // Run new "main" thread
    __SetCurrentThread(t);
    t->status = THREADSTATUS_RUNNING;
    __LoadContext(t->context);

    return handle;
}

/// Reschedules to the next available thread (call after current thread is suspended)
void Reschedule(const char* reason) {
    Thread* next = __NextThread();
    if (next > 0) {
        __SwitchContext(next, reason);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Put current thread in a wait state - on WaitSynchronization
void WaitThread_Synchronization() {
    // TODO(bunnei): Just a placeholder function for now... FixMe
    __WaitCurThread(WAITTYPE_SYNCH, "waitSynchronization called");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
}

void ThreadingShutdown() {
}

} // namespace
