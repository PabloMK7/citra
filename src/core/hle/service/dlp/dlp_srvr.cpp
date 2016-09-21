// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/result.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service {
namespace DLP {

static void unk_0x000E0040(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010183, nullptr, "Initialize"},
    {0x00020000, nullptr, "Finalize"},
    {0x000800C0, nullptr, "SendWirelessRebootPassphrase"},
    {0x000E0040, unk_0x000E0040, "unk_0x000E0040"},
};

DLP_SRVR_Interface::DLP_SRVR_Interface() {
    Register(FunctionTable);
}

} // namespace DLP
} // namespace Service
