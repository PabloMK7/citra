// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/dlp/dlp_fkcl.h"

namespace Service::DLP {

DLP_FKCL::DLP_FKCL() : ServiceFramework("dlp:FKCL", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x00010083, nullptr, "Initialize"},
        {0x00020000, nullptr, "Finalize"},
        {0x00030000, nullptr, "GetEventDesc"},
        {0x00040000, nullptr, "GetChannels"},
        {0x00050180, nullptr, "StartScan"},
        {0x00060000, nullptr, "StopScan"},
        {0x00070080, nullptr, "GetServerInfo"},
        {0x00080100, nullptr, "GetTitleInfo"},
        {0x00090040, nullptr, "GetTitleInfoInOrder"},
        {0x000A0080, nullptr, "DeleteScanInfo"},
        {0x000B0100, nullptr, "StartFakeSession"},
        {0x000C0000, nullptr, "GetMyStatus"},
        {0x000D0040, nullptr, "GetConnectingNodes"},
        {0x000E0040, nullptr, "GetNodeInfo"},
        {0x000F0000, nullptr, "GetWirelessRebootPassphrase"},
        {0x00100000, nullptr, "StopSession"},
        {0x00110203, nullptr, "Initialize2"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
