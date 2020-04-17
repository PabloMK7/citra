// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cecd/cecd_u.h"

SERIALIZE_EXPORT_IMPL(Service::CECD::CECD_U)

namespace Service::CECD {

CECD_U::CECD_U(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:u", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {0x000100C2, &CECD_U::Open, "Open"},
        {0x00020042, &CECD_U::Read, "Read"},
        {0x00030104, &CECD_U::ReadMessage, "ReadMessage"},
        {0x00040106, &CECD_U::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {0x00050042, &CECD_U::Write, "Write"},
        {0x00060104, &CECD_U::WriteMessage, "WriteMessage"},
        {0x00070106, &CECD_U::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {0x00080102, &CECD_U::Delete, "Delete"},
        {0x000900C2, &CECD_U::SetData, "SetData"},
        {0x000A00C4, &CECD_U::ReadData, "ReadData"},
        {0x000B0040, &CECD_U::Start, "Start"},
        {0x000C0040, &CECD_U::Stop, "Stop"},
        {0x000D0082, &CECD_U::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {0x000E0000, &CECD_U::GetCecdState, "GetCecdState"},
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

} // namespace Service::CECD
