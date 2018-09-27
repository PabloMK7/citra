// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd_s.h"

namespace Service::CECD {

CECD_S::CECD_S(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:s", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {0x000100C2, &CECD_S::Open, "Open"},
        {0x00020042, &CECD_S::Read, "Read"},
        {0x00030104, &CECD_S::ReadMessage, "ReadMessage"},
        {0x00040106, &CECD_S::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {0x00050042, &CECD_S::Write, "Write"},
        {0x00060104, &CECD_S::WriteMessage, "WriteMessage"},
        {0x00070106, &CECD_S::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {0x00080102, &CECD_S::Delete, "Delete"},
        {0x000900C2, &CECD_S::SetData, "SetData"},
        {0x000A00C4, &CECD_S::ReadData, "ReadData"},
        {0x000B0040, &CECD_S::Start, "Start"},
        {0x000C0040, &CECD_S::Stop, "Stop"},
        {0x000D0082, &CECD_S::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {0x000E0000, &CECD_S::GetCecdState, "GetCecdState"},
        {0x000F0000, &CECD_S::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {0x00100000, &CECD_S::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {0x00110104, &CECD_S::OpenAndWrite, "OpenAndWrite"},
        {0x00120104, &CECD_S::OpenAndRead, "OpenAndRead"},
        {0x001E0082, nullptr, "GetEventLog"},
        {0x001F0000, nullptr, "GetEventLogStart"},
        // cecd:s commands
        {0x04020002, nullptr, "GetCecInfoEventHandleSys"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
