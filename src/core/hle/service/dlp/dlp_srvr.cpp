// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/dlp/dlp_srvr.h"

SERIALIZE_EXPORT_IMPL(Service::DLP::DLP_SRVR)

namespace Service::DLP {

void DLP_SRVR::IsChild(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 1, 0);
    rp.Skip(1, false);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

DLP_SRVR::DLP_SRVR() : ServiceFramework("dlp:SRVR", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 6, 3), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Finalize"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "GetServerState"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "GetEventDescription"},
        {IPC::MakeHeader(0x0005, 2, 0), nullptr, "StartAccepting"},
        {IPC::MakeHeader(0x0006, 0, 0), nullptr, "EndAccepting"},
        {IPC::MakeHeader(0x0007, 0, 0), nullptr, "StartDistribution"},
        {IPC::MakeHeader(0x0008, 3, 0), nullptr, "SendWirelessRebootPassphrase"},
        {IPC::MakeHeader(0x0009, 1, 0), nullptr, "AcceptClient"},
        {IPC::MakeHeader(0x000A, 1, 0), nullptr, "DisconnectClient"},
        {IPC::MakeHeader(0x000B, 1, 2), nullptr, "GetConnectingClients"},
        {IPC::MakeHeader(0x000C, 1, 0), nullptr, "GetClientInfo"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "GetClientState"},
        {IPC::MakeHeader(0x000E, 1, 0), &DLP_SRVR::IsChild, "IsChild"},
        {IPC::MakeHeader(0x000F, 12, 3), nullptr, "InitializeWithName"},
        {IPC::MakeHeader(0x0010, 0, 0), nullptr, "GetDupNoticeNeed"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
