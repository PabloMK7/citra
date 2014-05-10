// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <stdio.h>

#include <list>
#include <vector>
#include <map>
#include <string>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

// Real CTR struct, don't change the fields.
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
    //static int GetStaticIDType() { return SCE_KERNEL_TMID_Thread; }
    //int GetIDType() const { return SCE_KERNEL_TMID_Thread; }

    //bool AllocateStack(u32 &stack_size) {
    //    FreeStack();

    //    bool fromTop = (nt.attr & PSP_THREAD_ATTR_LOW_STACK) == 0;
    //    if (nt.attr & PSP_THREAD_ATTR_KERNEL)
    //    {
    //        // Allocate stacks for kernel threads (idle) in kernel RAM
    //        currentStack.start = kernelMemory.Alloc(stack_size, fromTop, (std::string("stack/") + nt.name).c_str());
    //    }
    //    else
    //    {
    //        currentStack.start = userMemory.Alloc(stack_size, fromTop, (std::string("stack/") + nt.name).c_str());
    //    }
    //    if (currentStack.start == (u32)-1)
    //    {
    //        currentStack.start = 0;
    //        nt.initialStack = 0;
    //        ERROR_LOG(KERNEL, "Failed to allocate stack for thread");
    //        return false;
    //    }

    //    nt.initialStack = currentStack.start;
    //    nt.stack_size = stack_size;
    //    return true;
    //}

    //bool FillStack() {
    //    // Fill the stack.
    //    if ((nt.attr & PSP_THREAD_ATTR_NO_FILLSTACK) == 0) {
    //        Memory::Memset(currentStack.start, 0xFF, nt.stack_size);
    //    }
    //    context.r[MIPS_REG_SP] = currentStack.start + nt.stack_size;
    //    currentStack.end = context.r[MIPS_REG_SP];
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
    //    if (currentStack.start != 0) {
    //        DEBUG_LOG(KERNEL, "Freeing thread stack %s", nt.name);

    //        if ((nt.attr & PSP_THREAD_ATTR_CLEAR_STACK) != 0 && nt.initialStack != 0) {
    //            Memory::Memset(nt.initialStack, 0, nt.stack_size);
    //        }

    //        if (nt.attr & PSP_THREAD_ATTR_KERNEL) {
    //            kernelMemory.Free(currentStack.start);
    //        }
    //        else {
    //            userMemory.Free(currentStack.start);
    //        }
    //        currentStack.start = 0;
    //    }
    //}

    //bool PushExtendedStack(u32 size) {
    //    u32 stack = userMemory.Alloc(size, true, (std::string("extended/") + nt.name).c_str());
    //    if (stack == (u32)-1)
    //        return false;

    //    pushed_stacks.push_back(currentStack);
    //    currentStack.start = stack;
    //    currentStack.end = stack + size;
    //    nt.initialStack = currentStack.start;
    //    nt.stack_size = currentStack.end - currentStack.start;

    //    // We still drop the threadID at the bottom and fill it, but there's no k0.
    //    Memory::Memset(currentStack.start, 0xFF, nt.stack_size);
    //    Memory::Write_U32(GetUID(), nt.initialStack);
    //    return true;
    //}

    //bool PopExtendedStack() {
    //    if (pushed_stacks.size() == 0) {
    //        return false;
    //    }
    //    userMemory.Free(currentStack.start);
    //    currentStack = pushed_stacks.back();
    //    pushed_stacks.pop_back();
    //    nt.initialStack = currentStack.start;
    //    nt.stack_size = currentStack.end - currentStack.start;
    //    return true;
    //}

    Thread() {
        currentStack.start = 0;
    }

    // Can't use a destructor since savestates will call that too.
    //void Cleanup() {
    //    // Callbacks are automatically deleted when their owning thread is deleted.
    //    for (auto it = callbacks.begin(), end = callbacks.end(); it != end; ++it)
    //        kernelObjects.Destroy<Callback>(*it);

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
    //inline bool isRunning() const { return (nt.status & THREADSTATUS_RUNNING) != 0; }
    //inline bool isStopped() const { return (nt.status & THREADSTATUS_DORMANT) != 0; }
    //inline bool isReady() const { return (nt.status & THREADSTATUS_READY) != 0; }
    //inline bool isWaiting() const { return (nt.status & THREADSTATUS_WAIT) != 0; }
    //inline bool isSuspended() const { return (nt.status & THREADSTATUS_SUSPEND) != 0; }

    NativeThread nt;

    ThreadWaitInfo waitInfo;
    UID moduleId;

    bool isProcessingCallbacks;
    u32 currentMipscallId;
    UID currentCallbackId;

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

    StackInfo currentStack;

    // For thread end.
    std::vector<UID> waiting_threads;
    // Key is the callback id it was for, or if no callback, the thread id.
    std::map<UID, u64> paused_waits;
};

void __KernelThreadingInit() {
}

void __KernelThreadingShutdown() {
}

//const char *__KernelGetThreadName(UID threadID);
//
//void __KernelSaveContext(ThreadContext *ctx);
//void __KernelLoadContext(ThreadContext *ctx);

//void __KernelSwitchContext(Thread *target, const char *reason);