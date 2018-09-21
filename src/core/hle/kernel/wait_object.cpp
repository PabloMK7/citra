// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/config_mem.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/shared_page.h"

namespace Kernel {

void WaitObject::AddWaitingThread(SharedPtr<Thread> thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr == waiting_threads.end())
        waiting_threads.push_back(std::move(thread));
}

void WaitObject::RemoveWaitingThread(Thread* thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    // If a thread passed multiple handles to the same object,
    // the kernel might attempt to remove the thread from the object's
    // waiting threads list multiple times.
    if (itr != waiting_threads.end())
        waiting_threads.erase(itr);
}

SharedPtr<Thread> WaitObject::GetHighestPriorityReadyThread() {
    Thread* candidate = nullptr;
    u32 candidate_priority = ThreadPrioLowest + 1;

    for (const auto& thread : waiting_threads) {
        // The list of waiting threads must not contain threads that are not waiting to be awakened.
        ASSERT_MSG(thread->status == ThreadStatus::WaitSynchAny ||
                       thread->status == ThreadStatus::WaitSynchAll ||
                       thread->status == ThreadStatus::WaitHleEvent,
                   "Inconsistent thread statuses in waiting_threads");

        if (thread->current_priority >= candidate_priority)
            continue;

        if (ShouldWait(thread.get()))
            continue;

        // A thread is ready to run if it's either in ThreadStatus::WaitSynchAny or
        // in ThreadStatus::WaitSynchAll and the rest of the objects it is waiting on are ready.
        bool ready_to_run = true;
        if (thread->status == ThreadStatus::WaitSynchAll) {
            ready_to_run = std::none_of(thread->wait_objects.begin(), thread->wait_objects.end(),
                                        [&thread](const SharedPtr<WaitObject>& object) {
                                            return object->ShouldWait(thread.get());
                                        });
        }

        if (ready_to_run) {
            candidate = thread.get();
            candidate_priority = thread->current_priority;
        }
    }

    return candidate;
}

void WaitObject::WakeupAllWaitingThreads() {
    while (auto thread = GetHighestPriorityReadyThread()) {
        if (!thread->IsSleepingOnWaitAll()) {
            Acquire(thread.get());
        } else {
            for (auto& object : thread->wait_objects) {
                object->Acquire(thread.get());
            }
        }

        // Invoke the wakeup callback before clearing the wait objects
        if (thread->wakeup_callback)
            thread->wakeup_callback(ThreadWakeupReason::Signal, thread, this);

        for (auto& object : thread->wait_objects)
            object->RemoveWaitingThread(thread.get());
        thread->wait_objects.clear();

        thread->ResumeFromWait();
    }
}

const std::vector<SharedPtr<Thread>>& WaitObject::GetWaitingThreads() const {
    return waiting_threads;
}

} // namespace Kernel
