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
#include "core/arm/skyeye_common/armstate.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

/// Event type for the thread wake up event
static CoreTiming::EventType* ThreadWakeupEventType = nullptr;

bool Thread::ShouldWait(Thread* thread) const {
    return status != THREADSTATUS_DEAD;
}

void Thread::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
}

// TODO(yuriks): This can be removed if Thread objects are explicitly pooled in the future, allowing
//               us to simply use a pool index or similar.
static Kernel::HandleTable wakeup_callback_handle_table;

// Lists all thread ids that aren't deleted/etc.
static std::vector<SharedPtr<Thread>> thread_list;

// Lists only ready thread ids.
static Common::ThreadQueueList<Thread*, THREADPRIO_LOWEST + 1> ready_queue;

static SharedPtr<Thread> current_thread;

// The first available thread id at startup
static u32 next_thread_id;

/**
 * Creates a new thread ID
 * @return The new thread ID
 */
inline static u32 const NewThreadId() {
    return next_thread_id++;
}

Thread::Thread() : context(Core::CPU().NewContext()) {}
Thread::~Thread() {}

Thread* GetCurrentThread() {
    return current_thread.get();
}

void Thread::Stop() {
    // Cancel any outstanding wakeup events for this thread
    CoreTiming::UnscheduleEvent(ThreadWakeupEventType, callback_handle);
    wakeup_callback_handle_table.Close(callback_handle);
    callback_handle = 0;

    // Clean up thread from ready queue
    // This is only needed when the thread is termintated forcefully (SVC TerminateProcess)
    if (status == THREADSTATUS_READY) {
        ready_queue.remove(current_priority, this);
    }

    status = THREADSTATUS_DEAD;

    WakeupAllWaitingThreads();

    // Clean up any dangling references in objects that this thread was waiting for
    for (auto& wait_object : wait_objects) {
        wait_object->RemoveWaitingThread(this);
    }
    wait_objects.clear();

    // Release all the mutexes that this thread holds
    ReleaseThreadMutexes(this);

    // Mark the TLS slot in the thread's page as free.
    u32 tls_page = (tls_address - Memory::TLS_AREA_VADDR) / Memory::PAGE_SIZE;
    u32 tls_slot =
        ((tls_address - Memory::TLS_AREA_VADDR) % Memory::PAGE_SIZE) / Memory::TLS_ENTRY_SIZE;
    Kernel::g_current_process->tls_slots[tls_page].reset(tls_slot);
}

/**
 * Switches the CPU's active thread context to that of the specified thread
 * @param new_thread The thread to switch to
 */
static void SwitchContext(Thread* new_thread) {
    Thread* previous_thread = GetCurrentThread();

    // Save context for previous thread
    if (previous_thread) {
        previous_thread->last_running_ticks = CoreTiming::GetTicks();
        Core::CPU().SaveContext(previous_thread->context);

        if (previous_thread->status == THREADSTATUS_RUNNING) {
            // This is only the case when a reschedule is triggered without the current thread
            // yielding execution (i.e. an event triggered, system core time-sliced, etc)
            ready_queue.push_front(previous_thread->current_priority, previous_thread);
            previous_thread->status = THREADSTATUS_READY;
        }
    }

    // Load context of new thread
    if (new_thread) {
        ASSERT_MSG(new_thread->status == THREADSTATUS_READY,
                   "Thread must be ready to become running.");

        // Cancel any outstanding wakeup events for this thread
        CoreTiming::UnscheduleEvent(ThreadWakeupEventType, new_thread->callback_handle);

        auto previous_process = Kernel::g_current_process;

        current_thread = new_thread;

        ready_queue.remove(new_thread->current_priority, new_thread);
        new_thread->status = THREADSTATUS_RUNNING;

        if (previous_process != current_thread->owner_process) {
            Kernel::g_current_process = current_thread->owner_process;
            SetCurrentPageTable(&Kernel::g_current_process->vm_manager.page_table);
        }

        Core::CPU().LoadContext(new_thread->context);
        Core::CPU().SetCP15Register(CP15_THREAD_URO, new_thread->GetTLSAddress());
    } else {
        current_thread = nullptr;
        // Note: We do not reset the current process and current page table when idling because
        // technically we haven't changed processes, our threads are just paused.
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
        if (!next) {
            // Otherwise just keep going with the current thread
            next = thread;
        }
    } else {
        next = ready_queue.pop_first();
    }

    return next;
}

void WaitCurrentThread_Sleep() {
    Thread* thread = GetCurrentThread();
    thread->status = THREADSTATUS_WAIT_SLEEP;
}

void ExitCurrentThread() {
    Thread* thread = GetCurrentThread();
    thread->Stop();
    thread_list.erase(std::remove(thread_list.begin(), thread_list.end(), thread),
                      thread_list.end());
}

/**
 * Callback that will wake up the thread it was scheduled for
 * @param thread_handle The handle of the thread that's been awoken
 * @param cycles_late The number of CPU cycles that have passed since the desired wakeup time
 */
static void ThreadWakeupCallback(u64 thread_handle, s64 cycles_late) {
    SharedPtr<Thread> thread = wakeup_callback_handle_table.Get<Thread>((Handle)thread_handle);
    if (thread == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid thread {:08X}", (Handle)thread_handle);
        return;
    }

    if (thread->status == THREADSTATUS_WAIT_SYNCH_ANY ||
        thread->status == THREADSTATUS_WAIT_SYNCH_ALL || thread->status == THREADSTATUS_WAIT_ARB ||
        thread->status == THREADSTATUS_WAIT_HLE_EVENT) {

        // Invoke the wakeup callback before clearing the wait objects
        if (thread->wakeup_callback)
            thread->wakeup_callback(ThreadWakeupReason::Timeout, thread, nullptr);

        // Remove the thread from each of its waiting objects' waitlists
        for (auto& object : thread->wait_objects)
            object->RemoveWaitingThread(thread.get());
        thread->wait_objects.clear();
    }

    thread->ResumeFromWait();
}

void Thread::WakeAfterDelay(s64 nanoseconds) {
    // Don't schedule a wakeup if the thread wants to wait forever
    if (nanoseconds == -1)
        return;

    CoreTiming::ScheduleEvent(nsToCycles(nanoseconds), ThreadWakeupEventType, callback_handle);
}

void Thread::ResumeFromWait() {
    ASSERT_MSG(wait_objects.empty(), "Thread is waking up while waiting for objects");

    switch (status) {
    case THREADSTATUS_WAIT_SYNCH_ALL:
    case THREADSTATUS_WAIT_SYNCH_ANY:
    case THREADSTATUS_WAIT_HLE_EVENT:
    case THREADSTATUS_WAIT_ARB:
    case THREADSTATUS_WAIT_SLEEP:
    case THREADSTATUS_WAIT_IPC:
        break;

    case THREADSTATUS_READY:
        // The thread's wakeup callback must have already been cleared when the thread was first
        // awoken.
        ASSERT(wakeup_callback == nullptr);
        // If the thread is waiting on multiple wait objects, it might be awoken more than once
        // before actually resuming. We can ignore subsequent wakeups if the thread status has
        // already been set to THREADSTATUS_READY.
        return;

    case THREADSTATUS_RUNNING:
        DEBUG_ASSERT_MSG(false, "Thread with object id {} has already resumed.", GetObjectId());
        return;
    case THREADSTATUS_DEAD:
        // This should never happen, as threads must complete before being stopped.
        DEBUG_ASSERT_MSG(false, "Thread with object id {} cannot be resumed because it's DEAD.",
                         GetObjectId());
        return;
    }

    wakeup_callback = nullptr;

    ready_queue.push_back(current_priority, this);
    status = THREADSTATUS_READY;
    Core::System::GetInstance().PrepareReschedule();
}

/**
 * Prints the thread queue for debugging purposes
 */
static void DebugThreadQueue() {
    Thread* thread = GetCurrentThread();
    if (!thread) {
        LOG_DEBUG(Kernel, "Current: NO CURRENT THREAD");
    } else {
        LOG_DEBUG(Kernel, "0x{:02X} {} (current)", thread->current_priority,
                  GetCurrentThread()->GetObjectId());
    }

    for (auto& t : thread_list) {
        u32 priority = ready_queue.contains(t.get());
        if (priority != -1) {
            LOG_DEBUG(Kernel, "0x{:02X} {}", priority, t->GetObjectId());
        }
    }
}

/**
 * Finds a free location for the TLS section of a thread.
 * @param tls_slots The TLS page array of the thread's owner process.
 * Returns a tuple of (page, slot, alloc_needed) where:
 * page: The index of the first allocated TLS page that has free slots.
 * slot: The index of the first free slot in the indicated page.
 * alloc_needed: Whether there's a need to allocate a new TLS page (All pages are full).
 */
static std::tuple<std::size_t, std::size_t, bool> GetFreeThreadLocalSlot(
    const std::vector<std::bitset<8>>& tls_slots) {
    // Iterate over all the allocated pages, and try to find one where not all slots are used.
    for (std::size_t page = 0; page < tls_slots.size(); ++page) {
        const auto& page_tls_slots = tls_slots[page];
        if (!page_tls_slots.all()) {
            // We found a page with at least one free slot, find which slot it is
            for (std::size_t slot = 0; slot < page_tls_slots.size(); ++slot) {
                if (!page_tls_slots.test(slot)) {
                    return std::make_tuple(page, slot, false);
                }
            }
        }
    }

    return std::make_tuple(0, 0, true);
}

/**
 * Resets a thread context, making it ready to be scheduled and run by the CPU
 * @param context Thread context to reset
 * @param stack_top Address of the top of the stack
 * @param entry_point Address of entry point for execution
 * @param arg User argument for thread
 */
static void ResetThreadContext(const std::unique_ptr<ARM_Interface::ThreadContext>& context,
                               u32 stack_top, u32 entry_point, u32 arg) {
    context->Reset();
    context->SetCpuRegister(0, arg);
    context->SetProgramCounter(entry_point);
    context->SetStackPointer(stack_top);
    context->SetCpsr(USER32MODE | ((entry_point & 1) << 5)); // Usermode and THUMB mode
}

ResultVal<SharedPtr<Thread>> Thread::Create(std::string name, VAddr entry_point, u32 priority,
                                            u32 arg, s32 processor_id, VAddr stack_top,
                                            SharedPtr<Process> owner_process) {
    // Check if priority is in ranged. Lowest priority -> highest priority id.
    if (priority > THREADPRIO_LOWEST) {
        LOG_ERROR(Kernel_SVC, "Invalid thread priority: {}", priority);
        return ERR_OUT_OF_RANGE;
    }

    if (processor_id > THREADPROCESSORID_MAX) {
        LOG_ERROR(Kernel_SVC, "Invalid processor id: {}", processor_id);
        return ERR_OUT_OF_RANGE_KERNEL;
    }

    // TODO(yuriks): Other checks, returning 0xD9001BEA

    if (!Memory::IsValidVirtualAddress(*owner_process, entry_point)) {
        LOG_ERROR(Kernel_SVC, "(name={}): invalid entry {:08x}", name, entry_point);
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
    thread->wait_objects.clear();
    thread->wait_address = 0;
    thread->name = std::move(name);
    thread->callback_handle = wakeup_callback_handle_table.Create(thread).Unwrap();
    thread->owner_process = owner_process;

    // Find the next available TLS index, and mark it as used
    auto& tls_slots = owner_process->tls_slots;

    auto [available_page, available_slot, needs_allocation] = GetFreeThreadLocalSlot(tls_slots);

    if (needs_allocation) {
        // There are no already-allocated pages with free slots, lets allocate a new one.
        // TLS pages are allocated from the BASE region in the linear heap.
        MemoryRegionInfo* memory_region = GetMemoryRegion(MemoryRegion::BASE);
        auto& linheap_memory = memory_region->linear_heap_memory;

        if (linheap_memory->size() + Memory::PAGE_SIZE > memory_region->size) {
            LOG_ERROR(Kernel_SVC,
                      "Not enough space in region to allocate a new TLS page for thread");
            return ERR_OUT_OF_MEMORY;
        }

        std::size_t offset = linheap_memory->size();

        // Allocate some memory from the end of the linear heap for this region.
        linheap_memory->insert(linheap_memory->end(), Memory::PAGE_SIZE, 0);
        memory_region->used += Memory::PAGE_SIZE;
        owner_process->linear_heap_used += Memory::PAGE_SIZE;

        tls_slots.emplace_back(0); // The page is completely available at the start
        available_page = tls_slots.size() - 1;
        available_slot = 0; // Use the first slot in the new page

        auto& vm_manager = owner_process->vm_manager;
        vm_manager.RefreshMemoryBlockMappings(linheap_memory.get());

        // Map the page to the current process' address space.
        // TODO(Subv): Find the correct MemoryState for this region.
        vm_manager.MapMemoryBlock(Memory::TLS_AREA_VADDR + available_page * Memory::PAGE_SIZE,
                                  linheap_memory, offset, Memory::PAGE_SIZE, MemoryState::Private);
    }

    // Mark the slot as used
    tls_slots[available_page].set(available_slot);
    thread->tls_address = Memory::TLS_AREA_VADDR + available_page * Memory::PAGE_SIZE +
                          available_slot * Memory::TLS_ENTRY_SIZE;

    // TODO(peachum): move to ScheduleThread() when scheduler is added so selected core is used
    // to initialize the context
    ResetThreadContext(thread->context, stack_top, entry_point, arg);

    ready_queue.push_back(thread->current_priority, thread.get());
    thread->status = THREADSTATUS_READY;

    return MakeResult<SharedPtr<Thread>>(std::move(thread));
}

void Thread::SetPriority(u32 priority) {
    ASSERT_MSG(priority <= THREADPRIO_LOWEST && priority >= THREADPRIO_HIGHEST,
               "Invalid priority value.");
    // If thread was ready, adjust queues
    if (status == THREADSTATUS_READY)
        ready_queue.move(this, current_priority, priority);
    else
        ready_queue.prepare(priority);

    nominal_priority = current_priority = priority;
}

void Thread::UpdatePriority() {
    u32 best_priority = nominal_priority;
    for (auto& mutex : held_mutexes) {
        if (mutex->priority < best_priority)
            best_priority = mutex->priority;
    }
    BoostPriority(best_priority);
}

void Thread::BoostPriority(u32 priority) {
    // If thread was ready, adjust queues
    if (status == THREADSTATUS_READY)
        ready_queue.move(this, current_priority, priority);
    else
        ready_queue.prepare(priority);
    current_priority = priority;
}

SharedPtr<Thread> SetupMainThread(u32 entry_point, u32 priority, SharedPtr<Process> owner_process) {
    // Initialize new "main" thread
    auto thread_res =
        Thread::Create("main", entry_point, priority, 0, owner_process->ideal_processor,
                       Memory::HEAP_VADDR_END, owner_process);

    SharedPtr<Thread> thread = std::move(thread_res).Unwrap();

    thread->context->SetFpscr(FPSCR_DEFAULT_NAN | FPSCR_FLUSH_TO_ZERO | FPSCR_ROUND_TOZERO |
                              FPSCR_IXC); // 0x03C00010

    // Note: The newly created thread will be run when the scheduler fires.
    return thread;
}

bool HaveReadyThreads() {
    return ready_queue.get_first() != nullptr;
}

void Reschedule() {
    Thread* cur = GetCurrentThread();
    Thread* next = PopNextReadyThread();

    if (cur && next) {
        LOG_TRACE(Kernel, "context switch {} -> {}", cur->GetObjectId(), next->GetObjectId());
    } else if (cur) {
        LOG_TRACE(Kernel, "context switch {} -> idle", cur->GetObjectId());
    } else if (next) {
        LOG_TRACE(Kernel, "context switch idle -> {}", next->GetObjectId());
    }

    SwitchContext(next);
}

void Thread::SetWaitSynchronizationResult(ResultCode result) {
    context->SetCpuRegister(0, result.raw);
}

void Thread::SetWaitSynchronizationOutput(s32 output) {
    context->SetCpuRegister(1, output);
}

s32 Thread::GetWaitObjectIndex(WaitObject* object) const {
    ASSERT_MSG(!wait_objects.empty(), "Thread is not waiting for anything");
    auto match = std::find(wait_objects.rbegin(), wait_objects.rend(), object);
    return static_cast<s32>(std::distance(match, wait_objects.rend()) - 1);
}

VAddr Thread::GetCommandBufferAddress() const {
    // Offset from the start of TLS at which the IPC command buffer begins.
    static constexpr int CommandHeaderOffset = 0x80;
    return GetTLSAddress() + CommandHeaderOffset;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ThreadingInit() {
    ThreadWakeupEventType = CoreTiming::RegisterEvent("ThreadWakeupCallback", ThreadWakeupCallback);

    current_thread = nullptr;
    next_thread_id = 1;
}

void ThreadingShutdown() {
    current_thread = nullptr;

    for (auto& t : thread_list) {
        t->Stop();
    }
    thread_list.clear();
    ready_queue.clear();
    ClearProcessList();
}

const std::vector<SharedPtr<Thread>>& GetThreadList() {
    return thread_list;
}

} // namespace Kernel
