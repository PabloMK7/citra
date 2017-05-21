// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

AddressArbiter::AddressArbiter() {}
AddressArbiter::~AddressArbiter() {}

SharedPtr<AddressArbiter> AddressArbiter::Create(std::string name) {
    SharedPtr<AddressArbiter> address_arbiter(new AddressArbiter);

    address_arbiter->name = std::move(name);

    return address_arbiter;
}

ResultCode AddressArbiter::ArbitrateAddress(ArbitrationType type, VAddr address, s32 value,
                                            u64 nanoseconds) {
    switch (type) {

    // Signal thread(s) waiting for arbitrate address...
    case ArbitrationType::Signal:
        // Negative value means resume all threads
        if (value < 0) {
            ArbitrateAllThreads(address);
        } else {
            // Resume first N threads
            for (int i = 0; i < value; i++)
                ArbitrateHighestPriorityThread(address);
        }
        break;

    // Wait current thread (acquire the arbiter)...
    case ArbitrationType::WaitIfLessThan:
        if ((s32)Memory::Read32(address) < value) {
            Kernel::WaitCurrentThread_ArbitrateAddress(address);
        }
        break;
    case ArbitrationType::WaitIfLessThanWithTimeout:
        if ((s32)Memory::Read32(address) < value) {
            Kernel::WaitCurrentThread_ArbitrateAddress(address);
            GetCurrentThread()->WakeAfterDelay(nanoseconds);
        }
        break;
    case ArbitrationType::DecrementAndWaitIfLessThan: {
        s32 memory_value = Memory::Read32(address);
        if (memory_value < value) {
            // Only change the memory value if the thread should wait
            Memory::Write32(address, (s32)memory_value - 1);
            Kernel::WaitCurrentThread_ArbitrateAddress(address);
        }
        break;
    }
    case ArbitrationType::DecrementAndWaitIfLessThanWithTimeout: {
        s32 memory_value = Memory::Read32(address);
        if (memory_value < value) {
            // Only change the memory value if the thread should wait
            Memory::Write32(address, (s32)memory_value - 1);
            Kernel::WaitCurrentThread_ArbitrateAddress(address);
            GetCurrentThread()->WakeAfterDelay(nanoseconds);
        }
        break;
    }

    default:
        LOG_ERROR(Kernel, "unknown type=%d", type);
        return ERR_INVALID_ENUM_VALUE_FND;
    }

    // The calls that use a timeout seem to always return a Timeout error even if they did not put
    // the thread to sleep
    if (type == ArbitrationType::WaitIfLessThanWithTimeout ||
        type == ArbitrationType::DecrementAndWaitIfLessThanWithTimeout) {

        return RESULT_TIMEOUT;
    }
    return RESULT_SUCCESS;
}

} // namespace Kernel
