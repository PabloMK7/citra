// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/dlp/dlp_fkcl.h"

SERIALIZE_EXPORT_IMPL(Service::DLP::DLP_FKCL)

namespace Service::DLP {

DLP_FKCL::DLP_FKCL() : ServiceFramework("dlp:FKCL", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 3), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Finalize"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "GetEventDesc"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "GetChannels"},
        {IPC::MakeHeader(0x0005, 6, 0), nullptr, "StartScan"},
        {IPC::MakeHeader(0x0006, 0, 0), nullptr, "StopScan"},
        {IPC::MakeHeader(0x0007, 2, 0), nullptr, "GetServerInfo"},
        {IPC::MakeHeader(0x0008, 4, 0), nullptr, "GetTitleInfo"},
        {IPC::MakeHeader(0x0009, 1, 0), nullptr, "GetTitleInfoInOrder"},
        {IPC::MakeHeader(0x000A, 2, 0), nullptr, "DeleteScanInfo"},
        {IPC::MakeHeader(0x000B, 4, 0), nullptr, "StartFakeSession"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "GetMyStatus"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "GetConnectingNodes"},
        {IPC::MakeHeader(0x000E, 1, 0), nullptr, "GetNodeInfo"},
        {IPC::MakeHeader(0x000F, 0, 0), nullptr, "GetWirelessRebootPassphrase"},
        {IPC::MakeHeader(0x0010, 0, 0), nullptr, "StopSession"},
        {IPC::MakeHeader(0x0011, 8, 3), nullptr, "Initialize2"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
