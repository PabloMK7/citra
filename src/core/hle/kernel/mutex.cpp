// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>
#include <boost/range/algorithm_ext/erase.hpp>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

/**
 * Boost's a thread's priority to the best priority among the thread's held mutexes.
 * This prevents priority inversion via priority inheritance.
 */
static void UpdateThreadPriority(Thread* thread) {
    s32 best_priority = THREADPRIO_LOWEST;
    for (auto& mutex : thread->held_mutexes) {
        if (mutex->priority < best_priority)
            best_priority = mutex->priority;
    }

    best_priority = std::min(best_priority, thread->nominal_priority);
    thread->SetPriority(best_priority);
}

/**
 * Elevate the mutex priority to the best priority
 * among the priorities of all its waiting threads.
 */
static void UpdateMutexPriority(Mutex* mutex) {
    s32 best_priority = THREADPRIO_LOWEST;
    for (auto& waiter : mutex->GetWaitingThreads()) {
        if (waiter->current_priority < best_priority)
            best_priority = waiter->current_priority;
    }

    if (best_priority != mutex->priority) {
        mutex->priority = best_priority;
        UpdateThreadPriority(mutex->holding_thread.get());
    }
}

void ReleaseThreadMutexes(Thread* thread) {
    for (auto& mtx : thread->held_mutexes) {
        mtx->lock_count = 0;
        mtx->holding_thread = nullptr;
        mtx->WakeupAllWaitingThreads();
    }
    thread->held_mutexes.clear();
}

Mutex::Mutex() {}
Mutex::~Mutex() {}

SharedPtr<Mutex> Mutex::Create(bool initial_locked, std::string name) {
    SharedPtr<Mutex> mutex(new Mutex);

    mutex->lock_count = 0;
    mutex->name = std::move(name);
    mutex->holding_thread = nullptr;

    // Acquire mutex with current thread if initialized as locked
    if (initial_locked)
        mutex->Acquire(GetCurrentThread());

    return mutex;
}

bool Mutex::ShouldWait(Thread* thread) const {
    return lock_count > 0 && thread != holding_thread;
}

void Mutex::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    // Actually "acquire" the mutex only if we don't already have it
    if (lock_count == 0) {
        priority = thread->current_priority;
        thread->held_mutexes.insert(this);
        holding_thread = thread;

        UpdateThreadPriority(thread);

        Core::System::GetInstance().PrepareReschedule();
    }

    lock_count++;
}

void Mutex::Release() {
    // Only release if the mutex is held
    if (lock_count > 0) {
        lock_count--;

        // Yield to the next thread only if we've fully released the mutex
        if (lock_count == 0) {
            holding_thread->held_mutexes.erase(this);
            UpdateThreadPriority(holding_thread.get());
            holding_thread = nullptr;
            WakeupAllWaitingThreads();
            Core::System::GetInstance().PrepareReschedule();
        }
    }
}

void Mutex::AddWaitingThread(SharedPtr<Thread> thread) {
    WaitObject::AddWaitingThread(thread);
    UpdateMutexPriority(this);
}

void Mutex::RemoveWaitingThread(Thread* thread) {
    WaitObject::RemoveWaitingThread(thread);
    UpdateMutexPriority(this);
}

} // namespace
