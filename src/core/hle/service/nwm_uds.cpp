// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/nwm_uds.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NWM_UDS

namespace NWM_UDS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00030000, nullptr,               "Shutdown"},
    {0x000F0404, nullptr,               "RecvBeaconBroadcastData"},
    {0x00100042, nullptr,               "SetBeaconAdditionalData"},
    {0x001400C0, nullptr,               "RecvBroadcastDataFrame"},
    {0x001B0302, nullptr,               "Initialize"},
    {0x001D0044, nullptr,               "BeginHostingNetwork"},
    {0x001E0084, nullptr,               "ConnectToNetwork"},
    {0x001F0006, nullptr,               "DecryptBeaconData"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
