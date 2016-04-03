// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/dlp_srvr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace DLP_SRVR

namespace DLP_SRVR {

static void unk_0x000E0040(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_DLP, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010183, nullptr,           "Initialize"},
    {0x00020000, nullptr,           "Finalize"},
    {0x000E0040, unk_0x000E0040,    "unk_0x000E0040"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
