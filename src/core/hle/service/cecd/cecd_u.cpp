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
        {IPC::MakeHeader(0x0001, 3, 2), &CECD_U::Open, "Open"},
        {IPC::MakeHeader(0x0002, 1, 2), &CECD_U::Read, "Read"},
        {IPC::MakeHeader(0x0003, 4, 4), &CECD_U::ReadMessage, "ReadMessage"},
        {IPC::MakeHeader(0x0004, 4, 6), &CECD_U::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {IPC::MakeHeader(0x0005, 1, 2), &CECD_U::Write, "Write"},
        {IPC::MakeHeader(0x0006, 4, 4), &CECD_U::WriteMessage, "WriteMessage"},
        {IPC::MakeHeader(0x0007, 4, 6), &CECD_U::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {IPC::MakeHeader(0x0008, 4, 2), &CECD_U::Delete, "Delete"},
        {IPC::MakeHeader(0x0009, 3, 2), &CECD_U::SetData, "SetData"},
        {IPC::MakeHeader(0x000A, 3, 4), &CECD_U::ReadData, "ReadData"},
        {IPC::MakeHeader(0x000B, 1, 0), &CECD_U::Start, "Start"},
        {IPC::MakeHeader(0x000C, 1, 0), &CECD_U::Stop, "Stop"},
        {IPC::MakeHeader(0x000D, 2, 2), &CECD_U::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {IPC::MakeHeader(0x000E, 0, 0), &CECD_U::GetCecdState, "GetCecdState"},
        {IPC::MakeHeader(0x000F, 0, 0), &CECD_U::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {IPC::MakeHeader(0x0010, 0, 0), &CECD_U::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {IPC::MakeHeader(0x0011, 4, 4), &CECD_U::OpenAndWrite, "OpenAndWrite"},
        {IPC::MakeHeader(0x0012, 4, 4), &CECD_U::OpenAndRead, "OpenAndRead"},
        {IPC::MakeHeader(0x001E, 2, 2), nullptr, "GetEventLog"},
        {IPC::MakeHeader(0x001F, 0, 0), nullptr, "GetEventLogStart"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
