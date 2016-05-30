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
#include "core/hle/hle.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

/// Event type for the thread wake up event
static int ThreadWakeupEventType;

bool Thread::ShouldWait() {
    return status != THREADSTATUS_DEAD;
}

void Thread::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");
}

// TODO(yuriks): This can be removed if Thread objects are explicitly pooled in the future, allowing
//               us to simply use a pool index or similar.
static Kernel::HandleTable wakeup_callback_handle_table;

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
    wakeup_callback_handle_table.Close(callback_handle);
    callback_handle = 0;

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
    wait_objects.clear();

    // Mark the TLS slot in the thread's page as free.
    u32 tls_page = (tls_address - Memory::TLS_AREA_VADDR) / Memory::PAGE_SIZE;
    u32 tls_slot = ((tls_address - Memory::TLS_AREA_VADDR) % Memory::PAGE_SIZE) / Memory::TLS_ENTRY_SIZE;
    Kernel::g_current_process->tls_slots[tls_page].reset(tls_slot);

    HLE::Reschedule(__func__);
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

        if (thread->status == THREADSTATUS_READY && delta > boost_timeout) {
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
        DEBUG_ASSERT_MSG(new_thread->status == THREADSTATUS_READY, "Thread must be ready to become running.");

        // Cancel any outstanding wakeup events for this thread
        CoreTiming::UnscheduleEvent(ThreadWakeupEventType, new_thread->callback_handle);

        current_thread = new_thread;

        // If the thread was waited by a svcWaitSynch call, step back PC by one instruction to rerun
        // the SVC when the thread wakes up. This is necessary to ensure that the thread can acquire
        // the requested wait object(s) before continuing.
        if (new_thread->waitsynch_waited) {
            // CPSR flag indicates CPU mode
            bool thumb_mode = (new_thread->context.cpsr & TBIT) != 0;

            // SVC instruction is 2 bytes for THUMB, 4 bytes for ARM
            new_thread->context.pc -= thumb_mode ? 2 : 4;
        }

        // Clean up the thread's wait_objects, they'll be restored if needed during
        // the svcWaitSynchronization call
        for (size_t i = 0; i < new_thread->wait_objects.size(); ++i) {
            SharedPtr<WaitObject> object = new_thread->wait_objects[i];
            object->RemoveWaitingThread(new_thread);
        }
        new_thread->wait_objects.clear();

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
        if (!next) {
            // Otherwise just keep going with the current thread
            next = thread;
        }
    } else  {
        next = ready_queue.pop_first();
    }

    return next;
}

void WaitCurrentThread_Sleep() {
    Thread* thread = GetCurrentThread();
    thread->status = THREADSTATUS_WAIT_SLEEP;

    HLE::Reschedule(__func__);
}

void WaitCurrentThread_WaitSynchronization(std::vector<SharedPtr<WaitObject>> wait_objects, bool wait_set_output, bool wait_all) {
    Thread* thread = GetCurrentThread();
    thread->wait_set_output = wait_set_output;
    thread->wait_all = wait_all;
    thread->wait_objects = std::move(wait_objects);
    thread->waitsynch_waited = true;
    thread->status = THREADSTATUS_WAIT_SYNCH;
}

void WaitCurrentThread_ArbitrateAddress(VAddr wait_address) {
    Thread* thread = GetCurrentThread();
    thread->wait_address = wait_address;
    thread->status = THREADSTATUS_WAIT_ARB;
}

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

    thread->waitsynch_waited = false;

    if (thread->status == THREADSTATUS_WAIT_SYNCH || thread->status == THREADSTATUS_WAIT_ARB) {
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

void Thread::ResumeFromWait() {
    switch (status) {
        case THREADSTATUS_WAIT_SYNCH:
        case THREADSTATUS_WAIT_ARB:
        case THREADSTATUS_WAIT_SLEEP:
            break;

        case THREADSTATUS_READY:
            // If the thread is waiting on multiple wait objects, it might be awoken more than once
            // before actually resuming. We can ignore subsequent wakeups if the thread status has
            // already been set to THREADSTATUS_READY.
            return;

        case THREADSTATUS_RUNNING:
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

/**
 * Finds a free location for the TLS section of a thread.
 * @param tls_slots The TLS page array of the thread's owner process.
 * Returns a tuple of (page, slot, alloc_needed) where:
 * page: The index of the first allocated TLS page that has free slots.
 * slot: The index of the first free slot in the indicated page.
 * alloc_needed: Whether there's a need to allocate a new TLS page (All pages are full).
 */
std::tuple<u32, u32, bool> GetFreeThreadLocalSlot(std::vector<std::bitset<8>>& tls_slots) {
    // Iterate over all the allocated pages, and try to find one where not all slots are used.
    for (unsigned page = 0; page < tls_slots.size(); ++page) {
        const auto& page_tls_slots = tls_slots[page];
        if (!page_tls_slots.all()) {
            // We found a page with at least one free slot, find which slot it is
            for (unsigned slot = 0; slot < page_tls_slots.size(); ++slot) {
                if (!page_tls_slots.test(slot)) {
                    return std::make_tuple(page, slot, false);
                }
            }
        }
    }

    return std::make_tuple(0, 0, true);
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

    if (!Memory::IsValidVirtualAddress(entry_point)) {
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
    thread->owner_process = g_current_process;
    thread->waitsynch_waited = false;

    // Find the next available TLS index, and mark it as used
    auto& tls_slots = Kernel::g_current_process->tls_slots;
    bool needs_allocation = true;
    u32 available_page; // Which allocated page has free space
    u32 available_slot; // Which slot within the page is free

    std::tie(available_page, available_slot, needs_allocation) = GetFreeThreadLocalSlot(tls_slots);

    if (needs_allocation) {
        // There are no already-allocated pages with free slots, lets allocate a new one.
        // TLS pages are allocated from the BASE region in the linear heap.
        MemoryRegionInfo* memory_region = GetMemoryRegion(MemoryRegion::BASE);
        auto& linheap_memory = memory_region->linear_heap_memory;

        if (linheap_memory->size() + Memory::PAGE_SIZE > memory_region->size) {
            LOG_ERROR(Kernel_SVC, "Not enough space in region to allocate a new TLS page for thread");
            return ResultCode(ErrorDescription::OutOfMemory, ErrorModule::Kernel, ErrorSummary::OutOfResource, ErrorLevel::Permanent);
        }

        u32 offset = linheap_memory->size();

        // Allocate some memory from the end of the linear heap for this region.
        linheap_memory->insert(linheap_memory->end(), Memory::PAGE_SIZE, 0);
        memory_region->used += Memory::PAGE_SIZE;
        Kernel::g_current_process->linear_heap_used += Memory::PAGE_SIZE;

        tls_slots.emplace_back(0); // The page is completely available at the start
        available_page = tls_slots.size() - 1;
        available_slot = 0; // Use the first slot in the new page

        auto& vm_manager = Kernel::g_current_process->vm_manager;
        vm_manager.RefreshMemoryBlockMappings(linheap_memory.get());

        // Map the page to the current process' address space.
        // TODO(Subv): Find the correct MemoryState for this region.
        vm_manager.MapMemoryBlock(Memory::TLS_AREA_VADDR + available_page * Memory::PAGE_SIZE,
                                  linheap_memory, offset, Memory::PAGE_SIZE, MemoryState::Private);
    }

    // Mark the slot as used
    tls_slots[available_page].set(available_slot);
    thread->tls_address = Memory::TLS_AREA_VADDR + available_page * Memory::PAGE_SIZE + available_slot * Memory::TLS_ENTRY_SIZE;

    // TODO(peachum): move to ScheduleThread() when scheduler is added so selected core is used
    // to initialize the context
    Core::g_app_core->ResetContext(thread->context, stack_top, entry_point, arg);

    ready_queue.push_back(thread->current_priority, thread.get());
    thread->status = THREADSTATUS_READY;

    HLE::Reschedule(__func__);

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
    else
        ready_queue.prepare(priority);

    nominal_priority = current_priority = priority;
}

void Thread::BoostPriority(s32 priority) {
    ready_queue.move(this, current_priority, priority);
    current_priority = priority;
}

SharedPtr<Thread> SetupMainThread(u32 entry_point, s32 priority) {
    DEBUG_ASSERT(!GetCurrentThread());

    // Initialize new "main" thread
    auto thread_res = Thread::Create("main", entry_point, priority, 0,
            THREADPROCESSORID_0, Memory::HEAP_VADDR_END);

    SharedPtr<Thread> thread = thread_res.MoveFrom();

    thread->context.fpscr = FPSCR_DEFAULT_NAN | FPSCR_FLUSH_TO_ZERO | FPSCR_ROUND_TOZERO | FPSCR_IXC; // 0x03C00010

    // Run new "main" thread
    SwitchContext(thread.get());

    return thread;
}

void Reschedule() {
    PriorityBoostStarvedThreads();

    Thread* cur = GetCurrentThread();
    Thread* next = PopNextReadyThread();

    HLE::DoneRescheduling();

    // Don't bother switching to the same thread
    if (next == cur)
        return;

    if (cur && next) {
        LOG_TRACE(Kernel, "context switch %u -> %u", cur->GetObjectId(), next->GetObjectId());
    } else if (cur) {
        LOG_TRACE(Kernel, "context switch %u -> idle", cur->GetObjectId());
    } else if (next) {
        LOG_TRACE(Kernel, "context switch idle -> %u", next->GetObjectId());
    }

    SwitchContext(next);
}

void Thread::SetWaitSynchronizationResult(ResultCode result) {
    context.cpu_registers[0] = result.raw;
}

void Thread::SetWaitSynchronizationOutput(s32 output) {
    context.cpu_registers[1] = output;
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
}

} // namespace
