// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

Semaphore::Semaphore() {}
Semaphore::~Semaphore() {}

ResultVal<SharedPtr<Semaphore>> Semaphore::Create(s32 initial_count, s32 max_count,
                                                  std::string name) {

    if (initial_count > max_count)
        return ResultCode(ErrorDescription::InvalidCombination, ErrorModule::Kernel,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);

    SharedPtr<Semaphore> semaphore(new Semaphore);

    // When the semaphore is created, some slots are reserved for other threads,
    // and the rest is reserved for the caller thread
    semaphore->max_count = max_count;
    semaphore->available_count = initial_count;
    semaphore->name = std::move(name);

    return MakeResult<SharedPtr<Semaphore>>(std::move(semaphore));
}

bool Semaphore::ShouldWait() {
    return available_count <= 0;
}

void Semaphore::Acquire() {
    ASSERT_MSG(!ShouldWait(), "object unavailable!");
    --available_count;
}

ResultVal<s32> Semaphore::Release(s32 release_count) {
    if (max_count - available_count < release_count)
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::Kernel,
                          ErrorSummary::InvalidArgument, ErrorLevel::Permanent);

    s32 previous_count = available_count;
    available_count += release_count;

    WakeupAllWaitingThreads();

    return MakeResult<s32>(previous_count);
}

} // namespace
