// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <vector>

#include <boost/range/algorithm_ext/erase.hpp>

#include "common/common.h"

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
    mutex->locked = false;
    mutex->holding_thread = nullptr;

    // Find the next waiting thread for the mutex...
    auto next_thread = mutex->WakeupNextThread();
    if (next_thread != nullptr) {
        mutex->Acquire(next_thread);
    }
}

void ReleaseThreadMutexes(Thread* thread) {
    for (auto& mtx : thread->held_mutexes) {
        ResumeWaitingThread(mtx.get());
    }
    thread->held_mutexes.clear();
}

Mutex::Mutex() {}
Mutex::~Mutex() {}

ResultVal<SharedPtr<Mutex>> Mutex::Create(bool initial_locked, std::string name) {
    SharedPtr<Mutex> mutex(new Mutex);
    // TOOD(yuriks): Don't create Handle (see Thread::Create())
    CASCADE_RESULT(auto unused, Kernel::g_handle_table.Create(mutex));

    mutex->initial_locked = initial_locked;
    mutex->locked = false;
    mutex->name = std::move(name);
    mutex->holding_thread = nullptr;

    // Acquire mutex with current thread if initialized as locked...
    if (initial_locked)
        mutex->Acquire();

    return MakeResult<SharedPtr<Mutex>>(mutex);
}

bool Mutex::ShouldWait() {
    return locked && holding_thread != GetCurrentThread();
}

void Mutex::Acquire() {
    Acquire(GetCurrentThread());
}

void Mutex::Acquire(Thread* thread) {
    _assert_msg_(Kernel, !ShouldWait(), "object unavailable!");
    if (locked)
        return;

    locked = true;

    thread->held_mutexes.insert(this);
    holding_thread = thread;
}

void Mutex::Release() {
    if (!locked)
        return;

    holding_thread->held_mutexes.erase(this);
    ResumeWaitingThread(this);
}

} // namespace
