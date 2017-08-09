// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc.h"
#include "core/hle/result.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service {
namespace DLP {

static void IsChild(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
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
    {0x000E0040, IsChild, "IsChild"},
    {0x000F0303, nullptr, "InitializeWithName"},
    {0x00100000, nullptr, "GetDupNoticeNeed"},
};

DLP_SRVR_Interface::DLP_SRVR_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
