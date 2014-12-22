// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
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

    static const HandleType HANDLE_TYPE = HandleType::Semaphore;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    s32 max_count;                              ///< Maximum number of simultaneous holders the semaphore can have
    s32 available_count;                        ///< Number of free slots left in the semaphore
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
            Kernel::WaitCurrentThread(WAITTYPE_SEMA, this);
            waiting_threads.push(GetCurrentThread()->GetHandle());
        } else {
            --available_count;
        }

        return MakeResult<bool>(wait);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

ResultCode CreateSemaphore(Handle* handle, s32 initial_count, 
    s32 max_count, const std::string& name) {

    if (initial_count > max_count)
        return ResultCode(ErrorDescription::InvalidCombination, ErrorModule::Kernel,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);

    Semaphore* semaphore = new Semaphore;
    // TOOD(yuriks): Fix error reporting
    *handle = g_handle_table.Create(semaphore).ValueOr(INVALID_HANDLE);

    // When the semaphore is created, some slots are reserved for other threads,
    // and the rest is reserved for the caller thread
    semaphore->max_count = max_count;
    semaphore->available_count = initial_count;
    semaphore->name = name;

    return RESULT_SUCCESS;
}

ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {
    Semaphore* semaphore = g_handle_table.Get<Semaphore>(handle);
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
        Thread* thread = Kernel::g_handle_table.Get<Thread>(semaphore->waiting_threads.front());
        if (thread != nullptr)
            thread->ResumeFromWait();
        semaphore->waiting_threads.pop();
        --semaphore->available_count;
    }

    return RESULT_SUCCESS;
}

} // namespace
