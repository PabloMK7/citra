// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ps/ps_ps.h"

namespace Service::PS {

PS_PS::PS_PS() : ServiceFramework("ps:ps", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x00010244, nullptr, "SignRsaSha256"},
        {0x00020244, nullptr, "VerifyRsaSha256"},
        {0x00040204, nullptr, "EncryptDecryptAes"},
        {0x00050284, nullptr, "EncryptSignDecryptVerifyAesCcm"},
        {0x00060040, nullptr, "GetRomId"},
        {0x00070040, nullptr, "GetRomId2"},
        {0x00080040, nullptr, "GetRomMakerCode"},
        {0x00090000, nullptr, "GetCTRCardAutoStartupBit"},
        {0x000A0000, nullptr, "GetLocalFriendCodeSeed"},
        {0x000B0000, nullptr, "GetDeviceId"},
        {0x000C0000, nullptr, "SeedRNG"},
        {0x000D0042, nullptr, "GenerateRandomBytes"},
        {0x000E0082, nullptr, "InterfaceForPXI_0x04010084"},
        {0x000F0082, nullptr, "InterfaceForPXI_0x04020082"},
        {0x00100042, nullptr, "InterfaceForPXI_0x04030044"},
        {0x00110042, nullptr, "InterfaceForPXI_0x04040044"},
        // clang-format on
    };

    RegisterHandlers(functions);
};

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<PS_PS>()->InstallAsService(service_manager);
}

} // namespace Service::PS
