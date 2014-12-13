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

    u32 max_count;                              ///< Maximum number of simultaneous holders the semaphore can have
    u32 available_count;                        ///< Number of free slots left in the semaphore
    std::queue<Handle> waiting_threads;         ///< Threads that are waiting for the semaphore
    std::string name;                           ///< Name of semaphore (optional)

    /**
     * Tests whether a semaphore still has free slots
     * @return Whether the semaphore is available
     */
    bool IsAvailable() const {
        return available_count > 0;
    }

    ResultVal<bool> WaitSynchronization() override {
        bool wait = !IsAvailable();

        if (wait) {
            Kernel::WaitCurrentThread(WAITTYPE_SEMA, GetHandle());
            waiting_threads.push(GetCurrentThreadHandle());
        } else {
            --available_count;
        }

        return MakeResult<bool>(wait);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

ResultCode CreateSemaphore(Handle* handle, u32 initial_count, 
    u32 max_count, const std::string& name) {

    if (initial_count > max_count)
        return ResultCode(ErrorDescription::InvalidCombination, ErrorModule::Kernel,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);

    Semaphore* semaphore = new Semaphore;
    *handle = g_object_pool.Create(semaphore);

    // When the semaphore is created, some slots are reserved for other threads,
    // and the rest is reserved for the caller thread
    semaphore->max_count = max_count;
    semaphore->available_count = initial_count;
    semaphore->name = name;

    return RESULT_SUCCESS;
}

ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {
    Semaphore* semaphore = g_object_pool.Get<Semaphore>(handle);
    if (semaphore == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    if (semaphore->max_count - semaphore->available_count < release_count)
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::Kernel, 
                          ErrorSummary::InvalidArgument, ErrorLevel::Permanent);

    *count = semaphore->available_count;
    semaphore->available_count += release_count;

    // Notify some of the threads that the semaphore has been released
    // stop once the semaphore is full again or there are no more waiting threads
    while (!semaphore->waiting_threads.empty() && semaphore->IsAvailable()) {
        Kernel::ResumeThreadFromWait(semaphore->waiting_threads.front());
        semaphore->waiting_threads.pop();
        --semaphore->available_count;
    }

    return RESULT_SUCCESS;
}

} // namespace
