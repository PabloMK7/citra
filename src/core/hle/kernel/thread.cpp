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
#include "core/hle/syscall.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

// Enums

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

class Thread : public KernelObject {
public:

    const char *GetName() { return name; }
    const char *GetTypeName() { return "Thread"; }

    static KernelIDType GetStaticIDType() { return KERNEL_ID_TYPE_THREAD; }
    KernelIDType GetIDType() const { return KERNEL_ID_TYPE_THREAD; }

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

    char name[KERNEL_MAX_NAME_LENGTH+1];
};

// Lists all thread ids that aren't deleted/etc.
std::vector<Handle> g_thread_queue;

// Lists only ready thread ids.
Common::ThreadQueueList<Handle> g_thread_ready_queue;

Handle g_current_thread_handle;

Thread* g_current_thread;


inline Thread *__GetCurrentThread() {
    return g_current_thread;
}

inline void __SetCurrentThread(Thread *t) {
    g_current_thread = t;
    g_current_thread_handle = t->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Saves the current CPU context
void __KernelSaveContext(ThreadContext &ctx) {
    ctx.cpu_registers[0] = Core::g_app_core->GetReg(0);
    ctx.cpu_registers[1] = Core::g_app_core->GetReg(1);
    ctx.cpu_registers[2] = Core::g_app_core->GetReg(2);
    ctx.cpu_registers[3] = Core::g_app_core->GetReg(3);
    ctx.cpu_registers[4] = Core::g_app_core->GetReg(4);
    ctx.cpu_registers[5] = Core::g_app_core->GetReg(5);
    ctx.cpu_registers[6] = Core::g_app_core->GetReg(6);
    ctx.cpu_registers[7] = Core::g_app_core->GetReg(7);
    ctx.cpu_registers[8] = Core::g_app_core->GetReg(8);
    ctx.cpu_registers[9] = Core::g_app_core->GetReg(9);
    ctx.cpu_registers[10] = Core::g_app_core->GetReg(10);
    ctx.cpu_registers[11] = Core::g_app_core->GetReg(11);
    ctx.cpu_registers[12] = Core::g_app_core->GetReg(12);
    ctx.sp = Core::g_app_core->GetReg(13);
    ctx.lr = Core::g_app_core->GetReg(14);
    ctx.pc = Core::g_app_core->GetPC();
    ctx.cpsr = Core::g_app_core->GetCPSR();
}

/// Loads a CPU context
void __KernelLoadContext(const ThreadContext &ctx) {
    Core::g_app_core->SetReg(0, ctx.cpu_registers[0]);
    Core::g_app_core->SetReg(1, ctx.cpu_registers[1]);
    Core::g_app_core->SetReg(2, ctx.cpu_registers[2]);
    Core::g_app_core->SetReg(3, ctx.cpu_registers[3]);
    Core::g_app_core->SetReg(4, ctx.cpu_registers[4]);
    Core::g_app_core->SetReg(5, ctx.cpu_registers[5]);
    Core::g_app_core->SetReg(6, ctx.cpu_registers[6]);
    Core::g_app_core->SetReg(7, ctx.cpu_registers[7]);
    Core::g_app_core->SetReg(8, ctx.cpu_registers[8]);
    Core::g_app_core->SetReg(9, ctx.cpu_registers[9]);
    Core::g_app_core->SetReg(10, ctx.cpu_registers[10]);
    Core::g_app_core->SetReg(11, ctx.cpu_registers[11]);
    Core::g_app_core->SetReg(12, ctx.cpu_registers[12]);
    Core::g_app_core->SetReg(13, ctx.sp);
    Core::g_app_core->SetReg(14, ctx.lr);
    //Core::g_app_core->SetReg(15, ctx.pc);

    Core::g_app_core->SetPC(ctx.pc);
    Core::g_app_core->SetCPSR(ctx.cpsr);
}

/// Resets a thread
void __KernelResetThread(Thread *t, s32 lowest_priority) {
    memset(&t->context, 0, sizeof(ThreadContext));

    t->context.pc = t->entry_point;
    t->context.sp = t->stack_top;
    
    if (t->current_priority < lowest_priority) {
        t->current_priority = t->initial_priority;
    }
        
    t->wait_type = WAITTYPE_NONE;
}

/// Change a thread to "ready" state
void __KernelChangeReadyState(Thread *t, bool ready) {
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
void __KernelChangeThreadState(Thread *t, ThreadStatus new_status) {
    if (!t || t->status == new_status) {
        return;
    }
    __KernelChangeReadyState(t, (new_status & THREADSTATUS_READY) != 0);
    t->status = new_status;
    
    if (new_status == THREADSTATUS_WAIT) {
        if (t->wait_type == WAITTYPE_NONE) {
            printf("ERROR: Waittype none not allowed here\n");
        }
    }
}

/// Calls a thread by marking it as "ready" (note: will not actually execute until current thread yields)
void __KernelCallThread(Thread *t) {
    // Stop waiting
    if (t->wait_type != WAITTYPE_NONE) {
        t->wait_type = WAITTYPE_NONE;
    }
    __KernelChangeThreadState(t, THREADSTATUS_READY);
}

/// Creates a new thread
Thread *__KernelCreateThread(Handle &handle, const char *name, u32 entry_point, s32 priority,
    s32 processor_id, u32 stack_top, int stack_size) {

    Thread *t = new Thread;
    
    handle = g_kernel_objects.Create(t);
    
    g_thread_queue.push_back(handle);
    g_thread_ready_queue.prepare(priority);
    
    t->status = THREADSTATUS_DORMANT;
    t->entry_point = entry_point;
    t->stack_top = stack_top;
    t->stack_size = stack_size;
    t->initial_priority = t->current_priority = priority;
    t->processor_id = processor_id;
    t->wait_type = WAITTYPE_NONE;
    
    strncpy(t->name, name, KERNEL_MAX_NAME_LENGTH);
    t->name[KERNEL_MAX_NAME_LENGTH] = '\0';
    
    return t;
}

/// Creates a new thread - wrapper for external user
Handle __KernelCreateThread(const char *name, u32 entry_point, s32 priority, s32 processor_id,
    u32 stack_top, int stack_size) {
    if (name == NULL) {
        ERROR_LOG(KERNEL, "__KernelCreateThread(): NULL name");
        return -1;
    }
    if ((u32)stack_size < 0x200) {
        ERROR_LOG(KERNEL, "__KernelCreateThread(name=%s): invalid stack_size=0x%08X", name, 
            stack_size);
        return -1;
    }
    if (priority < THREADPRIO_HIGHEST || priority > THREADPRIO_LOWEST) {
        s32 new_priority = CLAMP(priority, THREADPRIO_HIGHEST, THREADPRIO_LOWEST);
        WARN_LOG(KERNEL, "__KernelCreateThread(name=%s): invalid priority=0x%08X, clamping to %08X",
            name, priority, new_priority);
        // TODO(bunnei): Clamping to a valid priority is not necessarily correct behavior... Confirm
        // validity of this
        priority = new_priority;
    }
    if (!Memory::GetPointer(entry_point)) {
        ERROR_LOG(KERNEL, "__KernelCreateThread(name=%s): invalid entry %08x", name, entry_point);
        return -1;
    }
    Handle handle;
    Thread *t = __KernelCreateThread(handle, name, entry_point, priority, processor_id, stack_top, 
        stack_size);

    HLE::EatCycles(32000);

    // This won't schedule to the new thread, but it may to one woken from eating cycles.
    // Technically, this should not eat all at once, and reschedule in the middle, but that's hard.
    HLE::ReSchedule("thread created");

    __KernelCallThread(t);
    
    return handle;
}

/// Switches CPU context to that of the specified thread
void __KernelSwitchContext(Thread* t, const char *reason) {
    Thread *cur = __GetCurrentThread();
    
    // Save context for current thread
    if (cur) {
        __KernelSaveContext(cur->context);
        
        if (cur->IsRunning()) {
            __KernelChangeReadyState(cur, true);
        }
    }
    // Load context of new thread
    if (t) {
        __SetCurrentThread(t);
        __KernelChangeReadyState(t, false);
        t->status = (t->status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;
        t->wait_type = WAITTYPE_NONE;
        __KernelLoadContext(t->context);
    } else {
        __SetCurrentThread(NULL);
    }
}

/// Gets the next thread that is ready to be run by priority
Thread *__KernelNextThread() {
    Handle next;
    Thread *cur = __GetCurrentThread();
    
    if (cur && cur->IsRunning()) {
        next = g_thread_ready_queue.pop_first_better(cur->current_priority);
    } else  {
        next = g_thread_ready_queue.pop_first();
    }
    if (next < 0) {
        return NULL;
    }
    return g_kernel_objects.GetFast<Thread>(next);
}

/// Sets up the primary application thread
Handle __KernelSetupMainThread(s32 priority, int stack_size) {
    Handle handle;
    
    // Initialize new "main" thread
    Thread *t = __KernelCreateThread(handle, "main", Core::g_app_core->GetPC(), priority, 
        THREADPROCESSORID_0, Memory::SCRATCHPAD_VADDR_END, stack_size);
    
    __KernelResetThread(t, 0);
    
    // If running another thread already, set it to "ready" state
    Thread *cur = __GetCurrentThread();
    if (cur && cur->IsRunning()) {
        __KernelChangeReadyState(cur, true);
    }
    
    // Run new "main" thread
    __SetCurrentThread(t);
    t->status = THREADSTATUS_RUNNING;
    __KernelLoadContext(t->context);

    return handle;
}

/// Resumes a thread from waiting by marking it as "ready"
void __KernelResumeThreadFromWait(Handle handle) {
    u32 error;
    Thread *t = g_kernel_objects.Get<Thread>(handle, error);
    if (t) {
        t->status &= ~THREADSTATUS_WAIT;
        if (!(t->status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD))) {
            __KernelChangeReadyState(t, true);
        }
    }
}

/// Puts a thread in the wait state for the given type/reason
void __KernelWaitCurThread(WaitType wait_type, const char *reason) {
    Thread *t = __GetCurrentThread();
    t->wait_type = wait_type;
    __KernelChangeThreadState(t, ThreadStatus(THREADSTATUS_WAIT | (t->status & THREADSTATUS_SUSPEND)));
}

/// Reschedules to the next available thread (call after current thread is suspended)
void __KernelReschedule(const char *reason) {
    Thread *next = __KernelNextThread();
    if (next > 0) {
        __KernelSwitchContext(next, reason);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Put current thread in a wait state - on WaitSynchronization
void __KernelWaitThread_Synchronization() {
    // TODO(bunnei): Just a placeholder function for now... FixMe
    __KernelWaitCurThread(WAITTYPE_SYNCH, "waitSynchronization called");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void __KernelThreadingInit() {
}

void __KernelThreadingShutdown() {
}
