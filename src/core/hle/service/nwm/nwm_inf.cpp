// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_inf.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_INF)

namespace Service::NWM {

NWM_INF::NWM_INF() : ServiceFramework("nwm::INF") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0006, 15, 4), nullptr, "RecvBeaconBroadcastData"},
        {IPC::MakeHeader(0x0007, 29, 2), nullptr, "ConnectToEncryptedAP"},
        {IPC::MakeHeader(0x0008, 12, 2), nullptr, "ConnectToAP"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
