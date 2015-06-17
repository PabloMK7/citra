// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>

#include <boost/range/algorithm_ext/erase.hpp>

#include "common/assert.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

/**
 * Resumes a thread waiting for the specified mutex
 * @param mutex The mutex that some thread is waiting on
 */
static void ResumeWaitingThread(Mutex* mutex) {
    // Reset mutex lock thread handle, nothing is waiting
    mutex->lock_count = 0;
    mutex->holding_thread = nullptr;
    mutex->WakeupAllWaitingThreads();
}

void ReleaseThreadMutexes(Thread* thread) {
    for (auto& mtx : thread->held_mutexes) {
        ResumeWaitingThread(mtx.get());
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

    // Acquire mutex with current thread if initialized as locked...
    if (initial_locked)
        mutex->Acquire();

    return mutex;
}

bool Mutex::ShouldWait() {
    auto thread = GetCurrentThread();
    bool wait = lock_count > 0 && holding_thread != thread;

    // If the holding thread of the mutex is lower priority than this thread, that thread should
    // temporarily inherit this thread's priority
    if (wait && thread->current_priority < holding_thread->current_priority)
        holding_thread->BoostPriority(thread->current_priority);

    return wait;
}

void Mutex::Acquire() {
    Acquire(GetCurrentThread());
}

void Mutex::Acquire(SharedPtr<Thread> thread) {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");

    // Actually "acquire" the mutex only if we don't already have it...
    if (lock_count == 0) {
        thread->held_mutexes.insert(this);
        holding_thread = std::move(thread);
    }

    lock_count++;
}

void Mutex::Release() {
    // Only release if the mutex is held...
    if (lock_count > 0) {
        lock_count--;

        // Yield to the next thread only if we've fully released the mutex...
        if (lock_count == 0) {
            holding_thread->held_mutexes.erase(this);
            ResumeWaitingThread(this);
        }
    }
}

} // namespace
