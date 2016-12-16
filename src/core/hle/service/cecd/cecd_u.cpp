// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

static const Interface::FunctionInfo FunctionTable[] = {
    // cecd:u shared commands
    {0x000100C2, nullptr, "OpenRawFile"},
    {0x00020042, nullptr, "ReadRawFile"},
    {0x00030104, nullptr, "ReadMessage"},
    {0x00040106, nullptr, "ReadMessageWithHMAC"},
    {0x00050042, nullptr, "WriteRawFile"},
    {0x00060104, nullptr, "WriteMessage"},
    {0x00070106, nullptr, "WriteMessageWithHMAC"},
    {0x00080102, nullptr, "Delete"},
    {0x000A00C4, nullptr, "GetSystemInfo"},
    {0x000B0040, nullptr, "RunCommand"},
    {0x000C0040, nullptr, "RunCommandAlt"},
    {0x000E0000, GetCecStateAbbreviated, "GetCecStateAbbreviated"},
    {0x000F0000, GetCecInfoEventHandle, "GetCecInfoEventHandle"},
    {0x00100000, GetChangeStateEventHandle, "GetChangeStateEventHandle"},
    {0x00110104, nullptr, "OpenAndWrite"},
    {0x00120104, nullptr, "OpenAndRead"},
};

CECD_U::CECD_U() {
    Register(FunctionTable);
}

} // namespace CECD
} // namespace Service
