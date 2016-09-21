// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_u.h"

namespace Service {
namespace NIM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr, "StartSysUpdate"},
    {0x00020000, nullptr, "GetUpdateDownloadProgress"},
    {0x00040000, nullptr, "FinishTitlesInstall"},
    {0x00050000, nullptr, "CheckForSysUpdateEvent"},
    {0x00090000, CheckSysUpdateAvailable, "CheckSysUpdateAvailable"},
    {0x000A0000, nullptr, "GetState"},
};

NIM_U_Interface::NIM_U_Interface() {
    Register(FunctionTable);
}

} // namespace NIM
} // namespace Service
