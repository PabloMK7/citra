// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

class HTTP_C final : public ServiceFramework<HTTP_C> {
public:
    HTTP_C();

private:
    /**
     * HTTP_C::Initialize service function
     *  Inputs:
     *      1 : POST buffer size
     *      2 : 0x20
     *      3 : 0x0 (Filled with process ID by ARM11 Kernel)
     *      4 : 0x0
     *      5 : POST buffer memory block handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory = nullptr;
};

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace HTTP
} // namespace Service
