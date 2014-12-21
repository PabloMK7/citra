// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/mem_map.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/thread.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

class AddressArbiter : public Object {
public:
    std::string GetTypeName() const override { return "Arbiter"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::AddressArbiter;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    std::string name;   ///< Name of address arbiter object (optional)
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Arbitrate an address
ResultCode ArbitrateAddress(Handle handle, ArbitrationType type, u32 address, s32 value) {
    switch (type) {

    // Signal thread(s) waiting for arbitrate address...
    case ArbitrationType::Signal:
        // Negative value means resume all threads
        if (value < 0) {
            ArbitrateAllThreads(handle, address);
        } else {
            // Resume first N threads
            for(int i = 0; i < value; i++)
                ArbitrateHighestPriorityThread(handle, address);
        }
        break;

    // Wait current thread (acquire the arbiter)...
    case ArbitrationType::WaitIfLessThan:
        if ((s32)Memory::Read32(address) <= value) {
            Kernel::WaitCurrentThread(WAITTYPE_ARB, handle, address);
            HLE::Reschedule(__func__);
        }
        break;

    default:
        LOG_ERROR(Kernel, "unknown type=%d", type);
        return ResultCode(ErrorDescription::InvalidEnumValue, ErrorModule::Kernel, ErrorSummary::WrongArgument, ErrorLevel::Usage);
    }
    return RESULT_SUCCESS;
}

/// Create an address arbiter
AddressArbiter* CreateAddressArbiter(Handle& handle, const std::string& name) {
    AddressArbiter* address_arbiter = new AddressArbiter;
    // TOOD(yuriks): Fix error reporting
    handle = Kernel::g_handle_table.Create(address_arbiter).ValueOr(INVALID_HANDLE);
    address_arbiter->name = name;
    return address_arbiter;
}

/// Create an address arbiter
Handle CreateAddressArbiter(const std::string& name) {
    Handle handle;
    CreateAddressArbiter(handle, name);
    return handle;
}

} // namespace Kernel
