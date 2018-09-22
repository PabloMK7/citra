// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/dlp/dlp_srvr.h"

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
        {0x00010183, nullptr, "Initialize"},
        {0x00020000, nullptr, "Finalize"},
        {0x00030000, nullptr, "GetServerState"},
        {0x00040000, nullptr, "GetEventDescription"},
        {0x00050080, nullptr, "StartAccepting"},
        {0x00060000, nullptr, "EndAccepting"},
        {0x00070000, nullptr, "StartDistribution"},
        {0x000800C0, nullptr, "SendWirelessRebootPassphrase"},
        {0x00090040, nullptr, "AcceptClient"},
        {0x000A0040, nullptr, "DisconnectClient"},
        {0x000B0042, nullptr, "GetConnectingClients"},
        {0x000C0040, nullptr, "GetClientInfo"},
        {0x000D0040, nullptr, "GetClientState"},
        {0x000E0040, &DLP_SRVR::IsChild, "IsChild"},
        {0x000F0303, nullptr, "InitializeWithName"},
        {0x00100000, nullptr, "GetDupNoticeNeed"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
