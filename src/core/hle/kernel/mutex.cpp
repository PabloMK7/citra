// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>
#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Mutex;
void MutexAcquireLock(Mutex* mutex, Handle thread = GetCurrentThreadHandle());

class Mutex : public Object {
public:
    std::string GetTypeName() const override { return "Mutex"; }
    std::string GetName() const override { return name; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Mutex; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Mutex; }

    bool initial_locked;                        ///< Initial lock state when mutex was created
    bool locked;                                ///< Current locked state
    Handle lock_thread;                         ///< Handle to thread that currently has mutex
    std::vector<Handle> waiting_threads;        ///< Threads that are waiting for the mutex
    std::string name;                           ///< Name of mutex (optional)

    ResultVal<bool> WaitSynchronization() override {
        bool wait = locked;
        if (locked) {
            Kernel::WaitCurrentThread(WAITTYPE_MUTEX, GetHandle());
        } else {
            // Lock the mutex when the first thread accesses it
            locked = true;
            MutexAcquireLock(this);
        }

        return MakeResult<bool>(wait);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::multimap<Handle, Handle> MutexMap;
static MutexMap g_mutex_held_locks;

/**
 * Acquires the specified mutex for the specified thread
 * @param mutex Mutex that is to be acquired
 * @param thread Thread that will acquired
 */
void MutexAcquireLock(Mutex* mutex, Handle thread) {
    g_mutex_held_locks.insert(std::make_pair(thread, mutex->GetHandle()));
    mutex->lock_thread = thread;
}

bool ReleaseMutexForThread(Mutex* mutex, Handle thread) {
    MutexAcquireLock(mutex, thread);
    Kernel::ResumeThreadFromWait(thread);
    return true;
}

/**
 * Resumes a thread waiting for the specified mutex
 * @param mutex The mutex that some thread is waiting on
 */
void ResumeWaitingThread(Mutex* mutex) {
    // Find the next waiting thread for the mutex...
    if (mutex->waiting_threads.empty()) {
        // Reset mutex lock thread handle, nothing is waiting
        mutex->locked = false;
        mutex->lock_thread = -1;
    }
    else {
        // Resume the next waiting thread and re-lock the mutex
        std::vector<Handle>::iterator iter = mutex->waiting_threads.begin();
        ReleaseMutexForThread(mutex, *iter);
        mutex->waiting_threads.erase(iter);
    }
}

void MutexEraseLock(Mutex* mutex) {
    Handle handle = mutex->GetHandle();
    auto locked = g_mutex_held_locks.equal_range(mutex->lock_thread);
    for (MutexMap::iterator iter = locked.first; iter != locked.second; ++iter) {
        if (iter->second == handle) {
            g_mutex_held_locks.erase(iter);
            break;
        }
    }
    mutex->lock_thread = -1;
}

void ReleaseThreadMutexes(Handle thread) {
    auto locked = g_mutex_held_locks.equal_range(thread);
    
    // Release every mutex that the thread holds, and resume execution on the waiting threads
    for (MutexMap::iterator iter = locked.first; iter != locked.second; ++iter) {
        Mutex* mutex = g_object_pool.GetFast<Mutex>(iter->second);
        ResumeWaitingThread(mutex);
    }

    // Erase all the locks that this thread holds
    g_mutex_held_locks.erase(thread);
}

bool LockMutex(Mutex* mutex) {
    // Mutex alread locked?
    if (mutex->locked) {
        return false;
    }
    MutexAcquireLock(mutex);
    return true;
}

bool ReleaseMutex(Mutex* mutex) {
    MutexEraseLock(mutex);
    ResumeWaitingThread(mutex);
    return true;
}

/**
 * Releases a mutex
 * @param handle Handle to mutex to release
 */
ResultCode ReleaseMutex(Handle handle) {
    Mutex* mutex = Kernel::g_object_pool.Get<Mutex>(handle);
    if (mutex == nullptr) return InvalidHandle(ErrorModule::Kernel);

    if (!ReleaseMutex(mutex)) {
        // TODO(yuriks): Verify error code, this one was pulled out of thin air. I'm not even sure
        // what error condition this is supposed to be signaling.
        return ResultCode(ErrorDescription::AlreadyDone, ErrorModule::Kernel,
                ErrorSummary::NothingHappened, ErrorLevel::Temporary);
    }
    return RESULT_SUCCESS;
}

/**
 * Creates a mutex
 * @param handle Reference to handle for the newly created mutex
 * @param initial_locked Specifies if the mutex should be locked initially
 * @param name Optional name of mutex
 * @return Pointer to new Mutex object
 */
Mutex* CreateMutex(Handle& handle, bool initial_locked, const std::string& name) {
    Mutex* mutex = new Mutex;
    handle = Kernel::g_object_pool.Create(mutex);

    mutex->locked = mutex->initial_locked = initial_locked;
    mutex->name = name;

    // Acquire mutex with current thread if initialized as locked...
    if (mutex->locked) {
        MutexAcquireLock(mutex);

    // Otherwise, reset lock thread handle
    } else {
        mutex->lock_thread = -1;
    }
    return mutex;
}

/**
 * Creates a mutex
 * @param initial_locked Specifies if the mutex should be locked initially
 * @param name Optional name of mutex
 * @return Handle to newly created object
 */
Handle CreateMutex(bool initial_locked, const std::string& name) {
    Handle handle;
    Mutex* mutex = CreateMutex(handle, initial_locked, name);
    return handle;
}

} // namespace
