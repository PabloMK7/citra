// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NIM {

class NIM_U final : public ServiceFramework<NIM_U> {
public:
    explicit NIM_U(Core::System& system);
    ~NIM_U();

private:
    /**
     * NIM::CheckForSysUpdateEvent service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy handle descriptor
     *      3 : System Update event handle
     */
    void CheckForSysUpdateEvent(Kernel::HLERequestContext& ctx);

    /**
     * NIM::CheckSysUpdateAvailable service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8 flag, 0 = no system update available, 1 = system update available.
     */
    void CheckSysUpdateAvailable(Kernel::HLERequestContext& ctx);

    Kernel::SharedPtr<Kernel::Event> nim_system_update_event;
};

} // namespace Service::NIM
