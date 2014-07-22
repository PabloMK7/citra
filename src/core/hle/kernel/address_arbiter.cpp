// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
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
    const char* GetTypeName() const { return "Arbiter"; }
    const char* GetName() const { return name.c_str(); }

    static Kernel::HandleType GetStaticHandleType() { return HandleType::AddressArbiter; }
    Kernel::HandleType GetHandleType() const { return HandleType::AddressArbiter; }

    std::string name;   ///< Name of address arbiter object (optional)

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED)");
        return 0;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Arbitrate an address
Result ArbitrateAddress(Handle handle, ArbitrationType type, u32 address, s32 value) {
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
        HLE::Reschedule(__func__);
        break;

    // Wait current thread (acquire the arbiter)...
    case ArbitrationType::WaitIfLessThan:
        if ((s32)Memory::Read32(address) <= value) {
            Kernel::WaitCurrentThread(WAITTYPE_ARB, handle);
            HLE::Reschedule(__func__);
        }
        break;

    default:
        ERROR_LOG(KERNEL, "unknown type=%d", type);
        return -1;
    }
    return 0;
}

/// Create an address arbiter
AddressArbiter* CreateAddressArbiter(Handle& handle, const std::string& name) {
    AddressArbiter* address_arbiter = new AddressArbiter;
    handle = Kernel::g_object_pool.Create(address_arbiter);
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
