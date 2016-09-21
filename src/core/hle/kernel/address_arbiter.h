// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

// Address arbiters are an underlying kernel synchronization object that can be created/used via
// supervisor calls (SVCs). They function as sort of a global lock. Typically, games/other CTR
// applications use them as an underlying mechanism to implement thread-safe barriers, events, and
// semphores.

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

enum class ArbitrationType : u32 {
    Signal,
    WaitIfLessThan,
    DecrementAndWaitIfLessThan,
    WaitIfLessThanWithTimeout,
    DecrementAndWaitIfLessThanWithTimeout,
};

class AddressArbiter final : public Object {
public:
    /**
     * Creates an address arbiter.
     *
     * @param name Optional name used for debugging.
     * @returns The created AddressArbiter.
     */
    static SharedPtr<AddressArbiter> Create(std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "Arbiter";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::AddressArbiter;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::string name; ///< Name of address arbiter object (optional)

    ResultCode ArbitrateAddress(ArbitrationType type, VAddr address, s32 value, u64 nanoseconds);

private:
    AddressArbiter();
    ~AddressArbiter() override;
};

} // namespace FileSys
