// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/nim_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NIM_U

namespace NIM_U {

/**
 * NIM_U::CheckSysUpdateAvailable service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : flag, 0 = no system update available, 1 = system update available.
 */
static void CheckSysUpdateAvailable(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // No update available

    LOG_WARNING(Service_NWM, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr,                  "StartSysUpdate"},
    {0x00020000, nullptr,                  "GetUpdateDownloadProgress"},
    {0x00040000, nullptr,                  "FinishTitlesInstall"},
    {0x00050000, nullptr,                  "CheckForSysUpdateEvent"},
    {0x00090000, CheckSysUpdateAvailable,  "CheckSysUpdateAvailable"},
    {0x000A0000, nullptr,                  "GetState"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
