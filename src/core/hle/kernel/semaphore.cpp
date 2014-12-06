// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <queue>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Semaphore : public Object {
public:
    std::string GetTypeName() const override { return "Semaphore"; }
    std::string GetName() const override { return name; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Semaphore; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Semaphore; }

    u32 initial_count;                          ///< Number of entries reserved for other threads
    u32 max_count;                              ///< Maximum number of simultaneous holders the semaphore can have
    u32 current_usage;                          ///< Number of currently used entries in the semaphore
    std::queue<Handle> waiting_threads;         ///< Threads that are waiting for the semaphore
    std::string name;                           ///< Name of semaphore (optional)

    /**
     * Tests whether a semaphore is at its peak capacity
     * @return Whether the semaphore is full
     */
    bool IsFull() const {
        return current_usage == max_count;
    }

    ResultVal<bool> WaitSynchronization() override {
        bool wait = current_usage == max_count;

        if (wait) {
            Kernel::WaitCurrentThread(WAITTYPE_SEMA, GetHandle());
            waiting_threads.push(GetCurrentThreadHandle());
        } else {
            ++current_usage;
        }

        return MakeResult<bool>(wait);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a semaphore
 * @param initial_count number of slots reserved for other threads
 * @param max_count maximum number of holders the semaphore can have
 * @param name Optional name of semaphore
 * @return Handle for the newly created semaphore
 */
Handle CreateSemaphore(u32 initial_count, 
    u32 max_count, const std::string& name) {

    Semaphore* semaphore = new Semaphore;
    Handle handle = g_object_pool.Create(semaphore);

    semaphore->initial_count = initial_count;
    // When the semaphore is created, some slots are reserved for other threads,
    // and the rest is reserved for the caller thread
    semaphore->max_count = semaphore->current_usage = max_count;
    semaphore->current_usage -= initial_count;
    semaphore->name = name;

    return handle;
}

ResultCode CreateSemaphore(Handle* handle, u32 initial_count, 
    u32 max_count, const std::string& name) {

    if (initial_count > max_count)
        return ResultCode(ErrorDescription::InvalidCombination, ErrorModule::Kernel,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    *handle = CreateSemaphore(initial_count, max_count, name);

    return RESULT_SUCCESS;
}

ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {

    Semaphore* semaphore = g_object_pool.Get<Semaphore>(handle);
    if (semaphore == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    if (semaphore->current_usage < release_count)
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::Kernel, 
                          ErrorSummary::InvalidArgument, ErrorLevel::Permanent);

    *count = semaphore->max_count - semaphore->current_usage;
    semaphore->current_usage = semaphore->current_usage - release_count;

    // Notify some of the threads that the semaphore has been released
    // stop once the semaphore is full again or there are no more waiting threads
    while (!semaphore->waiting_threads.empty() && !semaphore->IsFull()) {
        Kernel::ResumeThreadFromWait(semaphore->waiting_threads.front());
        semaphore->waiting_threads.pop();
        semaphore->current_usage++;
    }

    return RESULT_SUCCESS;
}

} // namespace
