// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <map>
#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Mutex : public Object {
public:
    const char* GetTypeName() { return "Mutex"; }

    static Kernel::HandleType GetStaticHandleType() {  return Kernel::HandleType::Mutex; }
    Kernel::HandleType GetHandleType() const { return Kernel::HandleType::Mutex; }

    bool initial_locked;                        ///< Initial lock state when mutex was created
    bool locked;                                ///< Current locked state
    Handle lock_thread;                         ///< Handle to thread that currently has mutex
    std::vector<Handle> waiting_threads;        ///< Threads that are waiting for the mutex
};

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::multimap<Handle, Handle> MutexMap;
static MutexMap g_mutex_held_locks;

void __MutexAcquireLock(Mutex* mutex, Handle thread) {
    g_mutex_held_locks.insert(std::make_pair(thread, mutex->GetHandle()));
    mutex->lock_thread = thread;
}

void __MutexAcquireLock(Mutex* mutex) {
    Handle thread = GetCurrentThread();
    __MutexAcquireLock(mutex, thread);
}

void __MutexEraseLock(Mutex* mutex) {
    Handle handle = mutex->GetHandle();
    auto locked = g_mutex_held_locks.equal_range(mutex->lock_thread);
    for (MutexMap::iterator iter = locked.first; iter != locked.second; ++iter) {
        if ((*iter).second == handle) {
            g_mutex_held_locks.erase(iter);
            break;
        }
    }
    mutex->lock_thread = -1;
}

bool __LockMutex(Mutex* mutex) {
    // Mutex alread locked?
    if (mutex->locked) {
        return false;
    }
    __MutexAcquireLock(mutex);
    return true;
}

bool __ReleaseMutexForThread(Mutex* mutex, Handle thread) {
    __MutexAcquireLock(mutex, thread);
    Kernel::ResumeThreadFromWait(thread);
    return true;
}

bool __ReleaseMutex(Mutex* mutex) {
    __MutexEraseLock(mutex);
    bool woke_threads = false;
    auto iter = mutex->waiting_threads.begin();

    // Find the next waiting thread for the mutex...
    while (!woke_threads && !mutex->waiting_threads.empty()) {
        woke_threads |= __ReleaseMutexForThread(mutex, *iter);
        mutex->waiting_threads.erase(iter);
    }
    // Reset mutex lock thread handle, nothing is waiting
    if (!woke_threads) {
        mutex->locked = false;
        mutex->lock_thread = -1;
    }
    return woke_threads;
}

/**
 * Releases a mutex
 * @param handle Handle to mutex to release
 */
Result ReleaseMutex(Handle handle) {
    Mutex* mutex = Kernel::g_object_pool.GetFast<Mutex>(handle);
    if (!__ReleaseMutex(mutex)) {
        return -1;
    }
    return 0;
}

/**
 * Creates a mutex
 * @param handle Reference to handle for the newly created mutex
 * @param initial_locked Specifies if the mutex should be locked initially
 */
Result CreateMutex(Handle& handle, bool initial_locked) {
    Mutex* mutex = new Mutex;
    handle = Kernel::g_object_pool.Create(mutex);

    mutex->locked = mutex->initial_locked = initial_locked;

    // Acquire mutex with current thread if initialized as locked...
    if (mutex->locked) {
        __MutexAcquireLock(mutex);

    // Otherwise, reset lock thread handle
    } else {
        mutex->lock_thread = -1;
    }
    return 0;
}

} // namespace
