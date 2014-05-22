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
    t->context.pc = t->entry_point;
    t->context.sp = t->stack_top;
    t->context.cpsr = 0x1F; // Usermode
    
    if (t->current_priority < lowest_priority) {
        t->current_priority = t->initial_priority;
    }
        
    t->wait_type = WAITTYPE_NONE;
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

/// Changes a threads state
void ChangeThreadState(Thread* t, ThreadStatus new_status) {
    if (!t || t->status == new_status) {
        return;
    }
    ChangeReadyState(t, (new_status & THREADSTATUS_READY) != 0);
    t->status = new_status;
    
    if (new_status == THREADSTATUS_WAIT) {
        if (t->wait_type == WAITTYPE_NONE) {
            printf("ERROR: Waittype none not allowed here\n");
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

/// Puts a thread in the wait state for the given type/reason
void WaitCurThread(WaitType wait_type, const char* reason) {
    Thread* t = GetCurrentThread();
    t->wait_type = wait_type;
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

    // This won't schedule to the new thread, but it may to one woken from eating cycles.
    // Technically, this should not eat all at once, and reschedule in the middle, but that's hard.
    HLE::ReSchedule("thread created");

    CallThread(t);
    
    return handle;
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
    if (next > 0) {
        SwitchContext(next);

        // Hack - automatically change previous thread (which would have been in "wait" state) to
        // "ready" state, so that we can immediately resume to it when new thread yields. FixMe to
        // actually wait for whatever event it is supposed to be waiting on.
        ChangeReadyState(prev, true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
}

void ThreadingShutdown() {
}

} // namespace
