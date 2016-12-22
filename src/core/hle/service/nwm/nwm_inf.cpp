// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nwm/nwm_inf.h"

namespace Service {
namespace NWM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000603C4, nullptr, "RecvBeaconBroadcastData"},
    {0x00070742, nullptr, "ConnectToEncryptedAP"},
    {0x00080302, nullptr, "ConnectToAP"},
};

NWM_INF::NWM_INF() {
    Register(FunctionTable);
}

} // namespace NWM
} // namespace Service
