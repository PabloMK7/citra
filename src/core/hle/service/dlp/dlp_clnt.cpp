// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/dlp/dlp_clnt.h"

SERIALIZE_EXPORT_IMPL(Service::DLP::DLP_CLNT)

namespace Service::DLP {

DLP_CLNT::DLP_CLNT() : ServiceFramework("dlp:CLNT", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x000100C3, nullptr, "Initialize"},
        {0x00020000, nullptr, "Finalize"},
        {0x00030000, nullptr, "GetEventDesc"},
        {0x00040000, nullptr, "GetChannel"},
        {0x00050180, nullptr, "StartScan"},
        {0x00060000, nullptr, "StopScan"},
        {0x00070080, nullptr, "GetServerInfo"},
        {0x00080100, nullptr, "GetTitleInfo"},
        {0x00090040, nullptr, "GetTitleInfoInOrder"},
        {0x000A0080, nullptr, "DeleteScanInfo"},
        {0x000B0100, nullptr, "PrepareForSystemDownload"},
        {0x000C0000, nullptr, "StartSystemDownload"},
        {0x000D0100, nullptr, "StartTitleDownload"},
        {0x000E0000, nullptr, "GetMyStatus"},
        {0x000F0040, nullptr, "GetConnectingNodes"},
        {0x00100040, nullptr, "GetNodeInfo"},
        {0x00110000, nullptr, "GetWirelessRebootPassphrase"},
        {0x00120000, nullptr, "StopSession"},
        {0x00130100, nullptr, "GetCupVersion"},
        {0x00140100, nullptr, "GetDupAvailability"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
