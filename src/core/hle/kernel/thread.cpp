// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <stdio.h>

#include <list>
#include <vector>
#include <map>
#include <string>

#include "common/common.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/hle.h"
#include "core/hle/syscall.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

struct ThreadQueueList {
    // Number of queues (number of priority levels starting at 0.)
    static const int NUM_QUEUES = 128;
    // Initial number of threads a single queue can handle.
    static const int INITIAL_CAPACITY = 32;

    struct Queue {
        // Next ever-been-used queue (worse priority.)
        Queue *next;
        // First valid item in data.
        int first;
        // One after last valid item in data.
        int end;
        // A too-large array with room on the front and end.
        UID *data;
        // Size of data array.
        int capacity;
    };

    ThreadQueueList() {
        memset(queues, 0, sizeof(queues));
        first = invalid();
    }

    ~ThreadQueueList() {
        for (int i = 0; i < NUM_QUEUES; ++i) {
            if (queues[i].data != NULL) {
                free(queues[i].data);
            }
        }
    }

    // Only for debugging, returns priority level.
    int contains(const UID uid) {
        for (int i = 0; i < NUM_QUEUES; ++i) {
            if (queues[i].data == NULL) {
                continue;
            }
            Queue *cur = &queues[i];
            for (int j = cur->first; j < cur->end; ++j) {
                if (cur->data[j] == uid) {
                    return i;
                }
            }
        }
        return -1;
    }

    inline UID pop_first() {
        Queue *cur = first;
        while (cur != invalid()) {
            if (cur->end - cur->first > 0) {
                return cur->data[cur->first++];
            }
            cur = cur->next;
        }

        _dbg_assert_msg_(KERNEL, false, "ThreadQueueList should not be empty.");
        return 0;
    }

    inline UID pop_first_better(u32 priority) {
        Queue *cur = first;
        Queue *stop = &queues[priority];
        while (cur < stop) {
            if (cur->end - cur->first > 0) {
                return cur->data[cur->first++];
            }
            cur = cur->next;
        }
        return 0;
    }

    inline void push_front(u32 priority, const UID thread_id) {
        Queue *cur = &queues[priority];
        cur->data[--cur->first] = thread_id;
        if (cur->first == 0) {
            rebalance(priority);
        }
    }

    inline void push_back(u32 priority, const UID thread_id)
    {
        Queue *cur = &queues[priority];
        cur->data[cur->end++] = thread_id;
        if (cur->end == cur->capacity) {
            rebalance(priority);
        }
    }

    inline void remove(u32 priority, const UID thread_id) {
        Queue *cur = &queues[priority];
        _dbg_assert_msg_(KERNEL, cur->next != NULL, "ThreadQueueList::Queue should already be linked up.");

        for (int i = cur->first; i < cur->end; ++i) {
            if (cur->data[i] == thread_id) {
                int remaining = --cur->end - i;
                if (remaining > 0) {
                    memmove(&cur->data[i], &cur->data[i + 1], remaining * sizeof(UID));
                }
                return;
            }
        }

        // Wasn't there.
    }

    inline void rotate(u32 priority) {
        Queue *cur = &queues[priority];
        _dbg_assert_msg_(KERNEL, cur->next != NULL, "ThreadQueueList::Queue should already be linked up.");

        if (cur->end - cur->first > 1) {
            cur->data[cur->end++] = cur->data[cur->first++];
            if (cur->end == cur->capacity) {
                rebalance(priority);
            }
        }
    }

    inline void clear() {
        for (int i = 0; i < NUM_QUEUES; ++i) {
            if (queues[i].data != NULL) {
                free(queues[i].data);
            }
        }
        memset(queues, 0, sizeof(queues));
        first = invalid();
    }

    inline bool empty(u32 priority) const {
        const Queue *cur = &queues[priority];
        return cur->first == cur->end;
    }

    inline void prepare(u32 priority) {
        Queue *cur = &queues[priority];
        if (cur->next == NULL) {
            link(priority, INITIAL_CAPACITY);
        }
    }

private:
    Queue *invalid() const {
        return (Queue *)-1;
    }

    void link(u32 priority, int size) {
        _dbg_assert_msg_(KERNEL, queues[priority].data == NULL, "ThreadQueueList::Queue should only be initialized once.");

        if (size <= INITIAL_CAPACITY) {
            size = INITIAL_CAPACITY;
        } else {
            int goal = size;
            size = INITIAL_CAPACITY;
            while (size < goal)
                size *= 2;
        }
        Queue *cur = &queues[priority];
        cur->data = (UID*)malloc(sizeof(UID)* size);
        cur->capacity = size;
        cur->first = size / 2;
        cur->end = size / 2;

        for (int i = (int)priority - 1; i >= 0; --i) {
            if (queues[i].next != NULL) {
                cur->next = queues[i].next;
                queues[i].next = cur;
                return;
            }
        }

        cur->next = first;
        first = cur;
    }

    void rebalance(u32 priority) {
        Queue *cur = &queues[priority];
        int size = cur->end - cur->first;
        if (size >= cur->capacity - 2) {
            UID* new_data = (UID*)realloc(cur->data, cur->capacity * 2 * sizeof(UID));
            if (new_data != NULL) {
                cur->capacity *= 2;
                cur->data = new_data;
            }
        }

        int newFirst = (cur->capacity - size) / 2;
        if (newFirst != cur->first) {
            memmove(&cur->data[newFirst], &cur->data[cur->first], size * sizeof(UID));
            cur->first = newFirst;
            cur->end = newFirst + size;
        }
    }

    // The first queue that's ever been used.
    Queue* first;
    // The priority level queues of thread ids.
    Queue queues[NUM_QUEUES];
};

// Supposed to represent a real CTR struct... but not sure of the correct fields yet.
struct NativeThread {
    //u32         Pointer to vtable
    //u32         Reference count
    //KProcess*   Process the thread belongs to (virtual address)
    //u32         Thread id
    //u32*        ptr = *(KThread+0x8C) - 0xB0
    //u32*        End-address of the page for this thread allocated in the 0xFF4XX000 region. Thus, 
    //      if the beginning of this mapped page is 0xFF401000, this ptr would be 0xFF402000.
    //KThread*    Previous ? (virtual address)
    //KThread*    Next ? (virtual address)

    u32_le native_size;
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];

    // Threading stuff
    u32_le status;
    u32_le entry_point;
    u32_le initial_stack;
    u32_le stack_top;
    u32_le stack_size;
    
    u32_le arg;
    u32_le processor_id;

    s32_le initial_priority;
    s32_le current_priority;
};

struct ThreadWaitInfo {
    u32 wait_value;
    u32 timeout_ptr;
};

class Thread : public KernelObject {
public:
    /*const char *GetName() { return nt.name; }*/
    const char *GetTypeName() { return "Thread"; }
    //void GetQuickInfo(char *ptr, int size)
    //{
    //    sprintf(ptr, "pc= %08x sp= %08x %s %s %s %s %s %s (wt=%i wid=%i wv= %08x )",
    //        context.pc, context.r[13], // 13 is stack pointer
    //        (nt.status & THREADSTATUS_RUNNING) ? "RUN" : "",
    //        (nt.status & THREADSTATUS_READY) ? "READY" : "",
    //        (nt.status & THREADSTATUS_WAIT) ? "WAIT" : "",
    //        (nt.status & THREADSTATUS_SUSPEND) ? "SUSPEND" : "",
    //        (nt.status & THREADSTATUS_DORMANT) ? "DORMANT" : "",
    //        (nt.status & THREADSTATUS_DEAD) ? "DEAD" : "",
    //        nt.waitType,
    //        nt.waitID,
    //        waitInfo.waitValue);
    //}

    //static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_THID; }
    static KernelIDType GetStaticIDType() { return KERNEL_ID_TYPE_THREAD; }
    KernelIDType GetIDType() const { return KERNEL_ID_TYPE_THREAD; }

    bool SetupStack(u32 stack_top, int stack_size) {
        current_stack.start = stack_top; 
        nt.initial_stack = current_stack.start;
        nt.stack_size = stack_size;
        return true;
    }

    //bool FillStack() {
    //    // Fill the stack.
    //    if ((nt.attr & PSP_THREAD_ATTR_NO_FILLSTACK) == 0) {
    //        Memory::Memset(current_stack.start, 0xFF, nt.stack_size);
    //    }
    //    context.r[MIPS_REG_SP] = current_stack.start + nt.stack_size;
    //    current_stack.end = context.r[MIPS_REG_SP];
    //    // The k0 section is 256 bytes at the top of the stack.
    //    context.r[MIPS_REG_SP] -= 256;
    //    context.r[MIPS_REG_K0] = context.r[MIPS_REG_SP];
    //    u32 k0 = context.r[MIPS_REG_K0];
    //    Memory::Memset(k0, 0, 0x100);
    //    Memory::Write_U32(GetUID(), k0 + 0xc0);
    //    Memory::Write_U32(nt.initialStack, k0 + 0xc8);
    //    Memory::Write_U32(0xffffffff, k0 + 0xf8);
    //    Memory::Write_U32(0xffffffff, k0 + 0xfc);
    //    // After k0 comes the arguments, which is done by sceKernelStartThread().

    //    Memory::Write_U32(GetUID(), nt.initialStack);
    //    return true;
    //}

    //void FreeStack() {
    //    if (current_stack.start != 0) {
    //        DEBUG_LOG(KERNEL, "Freeing thread stack %s", nt.name);

    //        if ((nt.attr & PSP_THREAD_ATTR_CLEAR_STACK) != 0 && nt.initialStack != 0) {
    //            Memory::Memset(nt.initialStack, 0, nt.stack_size);
    //        }

    //        if (nt.attr & PSP_THREAD_ATTR_KERNEL) {
    //            kernelMemory.Free(current_stack.start);
    //        }
    //        else {
    //            userMemory.Free(current_stack.start);
    //        }
    //        current_stack.start = 0;
    //    }
    //}

    //bool PushExtendedStack(u32 size) {
    //    u32 stack = userMemory.Alloc(size, true, (std::string("extended/") + nt.name).c_str());
    //    if (stack == (u32)-1)
    //        return false;

    //    pushed_stacks.push_back(current_stack);
    //    current_stack.start = stack;
    //    current_stack.end = stack + size;
    //    nt.initialStack = current_stack.start;
    //    nt.stack_size = current_stack.end - current_stack.start;

    //    // We still drop the thread_id at the bottom and fill it, but there's no k0.
    //    Memory::Memset(current_stack.start, 0xFF, nt.stack_size);
    //    Memory::Write_U32(GetUID(), nt.initialStack);
    //    return true;
    //}

    //bool PopExtendedStack() {
    //    if (pushed_stacks.size() == 0) {
    //        return false;
    //    }
    //    userMemory.Free(current_stack.start);
    //    current_stack = pushed_stacks.back();
    //    pushed_stacks.pop_back();
    //    nt.initialStack = current_stack.start;
    //    nt.stack_size = current_stack.end - current_stack.start;
    //    return true;
    //}

    Thread() {
        current_stack.start = 0;
    }

    // Can't use a destructor since savestates will call that too.
    //void Cleanup() {
    //    // Callbacks are automatically deleted when their owning thread is deleted.
    //    for (auto it = callbacks.begin(), end = callbacks.end(); it != end; ++it)
    //        g_kernel_objects.Destroy<Callback>(*it);

    //    if (pushed_stacks.size() != 0)
    //    {
    //        WARN_LOG(KERNEL, "Thread ended within an extended stack");
    //        for (size_t i = 0; i < pushed_stacks.size(); ++i)
    //            userMemory.Free(pushed_stacks[i].start);
    //    }
    //    FreeStack();
    //}

    void setReturnValue(u32 retval);
    void setReturnValue(u64 retval);
    void resumeFromWait();
    //bool isWaitingFor(WaitType type, int id);
    //int getWaitID(WaitType type);
    ThreadWaitInfo getWaitInfo();

    // Utils
    inline bool IsRunning() const { return (nt.status & THREADSTATUS_RUNNING) != 0; }
    inline bool IsStopped() const { return (nt.status & THREADSTATUS_DORMANT) != 0; }
    inline bool IsReady() const { return (nt.status & THREADSTATUS_READY) != 0; }
    inline bool IsWaiting() const { return (nt.status & THREADSTATUS_WAIT) != 0; }
    inline bool IsSuspended() const { return (nt.status & THREADSTATUS_SUSPEND) != 0; }

    NativeThread nt;

    ThreadWaitInfo waitInfo;
    UID moduleId;

    //bool isProcessingCallbacks;
    //u32 currentMipscallId;
    //UID currentCallbackId;

    ThreadContext context;

    std::vector<UID> callbacks;

    std::list<u32> pending_calls;

    struct StackInfo {
        u32 start;
        u32 end;
    };
    // This is a stack of... stacks, since sceKernelExtendThreadStack() can recurse.
    // These are stacks that aren't "active" right now, but will pop off once the func returns.
    std::vector<StackInfo> pushed_stacks;

    StackInfo current_stack;

    // For thread end.
    std::vector<UID> waiting_threads;
    // Key is the callback id it was for, or if no callback, the thread id.
    std::map<UID, u64> paused_waits;
};

void ThreadContext::reset() {
    for (int i = 0; i < 16; i++) {
        reg[i] = 0;
    }
    cpsr = 0;
}

// Lists all thread ids that aren't deleted/etc.
std::vector<UID> g_thread_queue;

// Lists only ready thread ids
ThreadQueueList g_thread_ready_queue;

UID         g_current_thread            = 0;
Thread*     g_current_thread_ptr        = NULL;
const char* g_hle_current_thread_name   = NULL;

/// Creates a new thread
Thread* __KernelCreateThread(UID& id, UID module_id, const char* name, u32 priority, 
    u32 entry_point, u32 arg, u32 stack_top, u32 processor_id, int stack_size) {

    Thread *t = new Thread;
    id = g_kernel_objects.Create(t);

    g_thread_queue.push_back(id);
    g_thread_ready_queue.prepare(priority);

    memset(&t->nt, 0xCD, sizeof(t->nt));

    t->nt.entry_point = entry_point;
    t->nt.native_size = sizeof(t->nt);
    t->nt.initial_priority = t->nt.current_priority = priority;
    t->nt.status = THREADSTATUS_DORMANT;
    t->nt.initial_stack = t->nt.stack_top = stack_top;
    t->nt.stack_size = stack_size;
    t->nt.processor_id = processor_id;

    strncpy(t->nt.name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    t->nt.name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';

    t->nt.stack_size = stack_size;
    t->SetupStack(stack_top, stack_size);

    return t;
}

UID __KernelCreateThread(UID module_id, const char* name, u32 priority, u32 entry_point, u32 arg, 
    u32 stack_top, u32 processor_id, int stack_size) {
    UID id;
    __KernelCreateThread(id, module_id, name, priority, entry_point, arg, stack_top, processor_id, 
        stack_size);
    
    HLE::EatCycles(32000);
    HLE::ReSchedule("thread created");
    
    return id;
}

/// Resets the specified thread back to initial calling state
void __KernelResetThread(Thread *t, int lowest_priority) {
    t->context.reset();
    t->context.pc = t->nt.entry_point;
    t->context.reg[13] = t->nt.initial_stack;

    // If the thread would be better than lowestPriority, reset to its initial.  Yes, kinda odd...
    if (t->nt.current_priority < lowest_priority) {
        t->nt.current_priority = t->nt.initial_priority;
    }

    memset(&t->waitInfo, 0, sizeof(t->waitInfo));
}

/// Returns the current executing thread
inline Thread *__GetCurrentThread() {
    return g_current_thread_ptr;
}

/// Sets the current executing thread
inline void __SetCurrentThread(Thread *thread, UID thread_id, const char *name) {
    g_current_thread = thread_id;
    g_current_thread_ptr = thread;
    g_hle_current_thread_name = name;
}

// TODO: Use __KernelChangeThreadState instead?  It has other affects...
void __KernelChangeReadyState(Thread *thread, UID thread_id, bool ready) {
    // Passing the id as a parameter is just an optimization, if it's wrong it will cause havoc.
    _dbg_assert_msg_(KERNEL, thread->GetUID() == thread_id, "Incorrect thread_id");
    int prio = thread->nt.current_priority;

    if (thread->IsReady()) {
        if (!ready)
            g_thread_ready_queue.remove(prio, thread_id);
    } else if (ready) {
        if (thread->IsRunning()) {
            g_thread_ready_queue.push_front(prio, thread_id);
        } else {
            g_thread_ready_queue.push_back(prio, thread_id);
        }
        thread->nt.status = THREADSTATUS_READY;
    }
}

void __KernelChangeReadyState(UID thread_id, bool ready) {
    u32 error;
    Thread *thread = g_kernel_objects.Get<Thread>(thread_id, error);
    if (thread) {
        __KernelChangeReadyState(thread, thread_id, ready);
    } else {
        WARN_LOG(KERNEL, "Trying to change the ready state of an unknown thread?");
    }
}

/// Returns NULL if the current thread is fine.
Thread* __KernelNextThread() {
    UID best_thread;

    // If the current thread is running, it's a valid candidate.
    Thread *cur = __GetCurrentThread();
    if (cur && cur->IsRunning()) {
        best_thread = g_thread_ready_queue.pop_first_better(cur->nt.current_priority);
        if (best_thread != 0) {
            __KernelChangeReadyState(cur, g_current_thread, true);
        }
    } else {
        best_thread = g_thread_ready_queue.pop_first();
    }
    // Assume g_thread_ready_queue has not become corrupt.
    if (best_thread != 0) {
        return g_kernel_objects.GetFast<Thread>(best_thread);
    } else {
        return NULL;
    }
}

/// Saves the current CPU context
void __KernelSaveContext(ThreadContext *ctx) {
    ctx->reg[0] = Core::g_app_core->GetReg(0);
    ctx->reg[1] = Core::g_app_core->GetReg(1);
    ctx->reg[2] = Core::g_app_core->GetReg(2);
    ctx->reg[3] = Core::g_app_core->GetReg(3);
    ctx->reg[4] = Core::g_app_core->GetReg(4);
    ctx->reg[5] = Core::g_app_core->GetReg(5);
    ctx->reg[6] = Core::g_app_core->GetReg(6);
    ctx->reg[7] = Core::g_app_core->GetReg(7);
    ctx->reg[8] = Core::g_app_core->GetReg(8);
    ctx->reg[9] = Core::g_app_core->GetReg(9);
    ctx->reg[10] = Core::g_app_core->GetReg(10);
    ctx->reg[11] = Core::g_app_core->GetReg(11);
    ctx->reg[12] = Core::g_app_core->GetReg(12);
    ctx->reg[13] = Core::g_app_core->GetReg(13);
    ctx->reg[14] = Core::g_app_core->GetReg(14);
    ctx->reg[15] = Core::g_app_core->GetReg(15);
    ctx->pc = Core::g_app_core->GetPC();
    ctx->cpsr = Core::g_app_core->GetCPSR();
}

/// Loads a CPU context
void __KernelLoadContext(ThreadContext *ctx) {
    Core::g_app_core->SetReg(0, ctx->reg[0]);
    Core::g_app_core->SetReg(1, ctx->reg[1]);
    Core::g_app_core->SetReg(2, ctx->reg[2]);
    Core::g_app_core->SetReg(3, ctx->reg[3]);
    Core::g_app_core->SetReg(4, ctx->reg[4]);
    Core::g_app_core->SetReg(5, ctx->reg[5]);
    Core::g_app_core->SetReg(6, ctx->reg[6]);
    Core::g_app_core->SetReg(7, ctx->reg[7]);
    Core::g_app_core->SetReg(8, ctx->reg[8]);
    Core::g_app_core->SetReg(9, ctx->reg[9]);
    Core::g_app_core->SetReg(10, ctx->reg[10]);
    Core::g_app_core->SetReg(11, ctx->reg[11]);
    Core::g_app_core->SetReg(12, ctx->reg[12]);
    Core::g_app_core->SetReg(13, ctx->reg[13]);
    Core::g_app_core->SetReg(14, ctx->reg[14]);
    Core::g_app_core->SetReg(15, ctx->reg[15]);
    Core::g_app_core->SetPC(ctx->pc);
    Core::g_app_core->SetCPSR(ctx->cpsr);
}

/// Switches thread context
void __KernelSwitchContext(Thread *target, const char *reason) {
    u32 old_pc = 0;
    UID old_uid = 0;
    const char *old_name = g_hle_current_thread_name != NULL ? g_hle_current_thread_name : "(none)";
    Thread *cur = __GetCurrentThread();

    if (cur) { // It might just have been deleted.
        __KernelSaveContext(&cur->context);
        old_pc = Core::g_app_core->GetPC();
        old_uid = cur->GetUID();

        // Normally this is taken care of in __KernelNextThread().
        if (cur->IsRunning())
            __KernelChangeReadyState(cur, old_uid, true);
    }
    if (target) {
        __SetCurrentThread(target, target->GetUID(), target->nt.name);
        __KernelChangeReadyState(target, g_current_thread, false);

        target->nt.status = (target->nt.status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;

        __KernelLoadContext(&target->context);
    } else {
        __SetCurrentThread(NULL, 0, NULL);
    }
}

bool __KernelSwitchToThread(UID thread_id, const char *reason) {
    if (!reason) {
        reason = "switch to thread";
    }
    if (g_current_thread == thread_id) {
        return false;
    }
    u32 error;
    Thread *t = g_kernel_objects.Get<Thread>(thread_id, error);
    if (!t) {
        ERROR_LOG(KERNEL, "__KernelSwitchToThread: %x doesn't exist", thread_id);
        HLE::ReSchedule("switch to deleted thread");
    } else if (t->IsReady() || t->IsRunning()) {
        Thread *current = __GetCurrentThread();
        if (current && current->IsRunning()) {
            __KernelChangeReadyState(current, g_current_thread, true);
        }
        __KernelSwitchContext(t, reason);
        return true;
    } else {
        HLE::ReSchedule("switch to waiting thread");
    }
    return false;
}

/// Sets up the root (primary) thread of execution
UID __KernelSetupRootThread(UID module_id, int arg, int prio, int stack_size) {
    UID id;

    Thread *thread = __KernelCreateThread(id, module_id, "root", prio, Core::g_app_core->GetPC(),
        arg, Memory::SCRATCHPAD_VADDR_END, 0xFFFFFFFE, stack_size=stack_size);

    if (thread->current_stack.start == 0) {
        ERROR_LOG(KERNEL, "Unable to allocate stack for root thread.");
    }
    __KernelResetThread(thread, 0);

    Thread *prev_thread = __GetCurrentThread();
    if (prev_thread && prev_thread->IsRunning())
        __KernelChangeReadyState(g_current_thread, true);
    __SetCurrentThread(thread, id, "root");
    thread->nt.status = THREADSTATUS_RUNNING; // do not schedule

    strcpy(thread->nt.name, "root");

    __KernelLoadContext(&thread->context);
    
    // NOTE(bunnei): Not sure this is really correct, ignore args for now...
    //Core::g_app_core->SetReg(0, args); 
    //Core::g_app_core->SetReg(13, (args + 0xf) & ~0xf); // Setup SP - probably not correct
    //u32 location = Core::g_app_core->GetReg(13); // SP 
    //Core::g_app_core->SetReg(1, location);
    
    //if (argp)
    //    Memory::Memcpy(location, argp, args);
    //// Let's assume same as starting a new thread, 64 bytes for safety/kernel.
    //Core::g_app_core->SetReg(13, Core::g_app_core->GetReg(13) - 64);

    return id;
}

int __KernelRotateThreadReadyQueue(int priority) {
    Thread *cur = __GetCurrentThread();

    // 0 is special, it means "my current priority."
    if (priority == 0) {
        priority = cur->nt.current_priority;
    }
    //if (priority <= 0x07 || priority > 0x77)
    //    return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;

    if (!g_thread_ready_queue.empty(priority)) {
        // In other words, yield to everyone else.
        if (cur->nt.current_priority == priority) {
            g_thread_ready_queue.push_back(priority, g_current_thread);
            cur->nt.status = (cur->nt.status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;

        // Yield the next thread of this priority to all other threads of same priority.
        } else {
            g_thread_ready_queue.rotate(priority);
        }
    }
    HLE::EatCycles(250);
    HLE::ReSchedule("rotatethreadreadyqueue");

    return 0;
}

void __KernelThreadingInit() {
}

void __KernelThreadingShutdown() {
}
