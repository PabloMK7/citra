// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

CECD_U::CECD_U(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:u", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {0x000100C2, &CECD_U::OpenRawFile, "OpenRawFile"},
        {0x00020042, &CECD_U::ReadRawFile, "ReadRawFile"},
        {0x00030104, &CECD_U::ReadMessage, "ReadMessage"},
        {0x00040106, &CECD_U::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {0x00050042, &CECD_U::WriteRawFile, "WriteRawFile"},
        {0x00060104, &CECD_U::WriteMessage, "WriteMessage"},
        {0x00070106, &CECD_U::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {0x00080102, &CECD_U::Delete, "Delete"},
        {0x000A00C4, &CECD_U::GetSystemInfo, "GetSystemInfo"},
        {0x000B0040, nullptr, "RunCommand"},
        {0x000C0040, nullptr, "RunCommandAlt"},
        {0x000E0000, &CECD_U::GetCecStateAbbreviated, "GetCecStateAbbreviated"},
        {0x000F0000, &CECD_U::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {0x00100000, &CECD_U::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {0x00110104, &CECD_U::OpenAndWrite, "OpenAndWrite"},
        {0x00120104, &CECD_U::OpenAndRead, "OpenAndRead"},
        {0x001E0082, nullptr, "GetEventLog"},
        {0x001F0000, nullptr, "GetEventLogStart"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace CECD
} // namespace Service
