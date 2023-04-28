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
        {IPC::MakeHeader(0x0001, 3, 3), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Finalize"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "GetEventDesc"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "GetChannel"},
        {IPC::MakeHeader(0x0005, 6, 0), nullptr, "StartScan"},
        {IPC::MakeHeader(0x0006, 0, 0), nullptr, "StopScan"},
        {IPC::MakeHeader(0x0007, 2, 0), nullptr, "GetServerInfo"},
        {IPC::MakeHeader(0x0008, 4, 0), nullptr, "GetTitleInfo"},
        {IPC::MakeHeader(0x0009, 1, 0), nullptr, "GetTitleInfoInOrder"},
        {IPC::MakeHeader(0x000A, 2, 0), nullptr, "DeleteScanInfo"},
        {IPC::MakeHeader(0x000B, 4, 0), nullptr, "PrepareForSystemDownload"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "StartSystemDownload"},
        {IPC::MakeHeader(0x000D, 4, 0), nullptr, "StartTitleDownload"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetMyStatus"},
        {IPC::MakeHeader(0x000F, 1, 0), nullptr, "GetConnectingNodes"},
        {IPC::MakeHeader(0x0010, 1, 0), nullptr, "GetNodeInfo"},
        {IPC::MakeHeader(0x0011, 0, 0), nullptr, "GetWirelessRebootPassphrase"},
        {IPC::MakeHeader(0x0012, 0, 0), nullptr, "StopSession"},
        {IPC::MakeHeader(0x0013, 4, 0), nullptr, "GetCupVersion"},
        {IPC::MakeHeader(0x0014, 4, 0), nullptr, "GetDupAvailability"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
