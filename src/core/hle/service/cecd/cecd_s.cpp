// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cecd/cecd_s.h"

SERIALIZE_EXPORT_IMPL(Service::CECD::CECD_S)

namespace Service::CECD {

CECD_S::CECD_S(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:s", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 3, 2), &CECD_S::Open, "Open"},
        {IPC::MakeHeader(0x0002, 1, 2), &CECD_S::Read, "Read"},
        {IPC::MakeHeader(0x0003, 4, 4), &CECD_S::ReadMessage, "ReadMessage"},
        {IPC::MakeHeader(0x0004, 4, 6), &CECD_S::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {IPC::MakeHeader(0x0005, 1, 2), &CECD_S::Write, "Write"},
        {IPC::MakeHeader(0x0006, 4, 4), &CECD_S::WriteMessage, "WriteMessage"},
        {IPC::MakeHeader(0x0007, 4, 6), &CECD_S::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {IPC::MakeHeader(0x0008, 4, 2), &CECD_S::Delete, "Delete"},
        {IPC::MakeHeader(0x0009, 3, 2), &CECD_S::SetData, "SetData"},
        {IPC::MakeHeader(0x000A, 3, 4), &CECD_S::ReadData, "ReadData"},
        {IPC::MakeHeader(0x000B, 1, 0), &CECD_S::Start, "Start"},
        {IPC::MakeHeader(0x000C, 1, 0), &CECD_S::Stop, "Stop"},
        {IPC::MakeHeader(0x000D, 2, 2), &CECD_S::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {IPC::MakeHeader(0x000E, 0, 0), &CECD_S::GetCecdState, "GetCecdState"},
        {IPC::MakeHeader(0x000F, 0, 0), &CECD_S::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {IPC::MakeHeader(0x0010, 0, 0), &CECD_S::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {IPC::MakeHeader(0x0011, 4, 4), &CECD_S::OpenAndWrite, "OpenAndWrite"},
        {IPC::MakeHeader(0x0012, 4, 4), &CECD_S::OpenAndRead, "OpenAndRead"},
        {IPC::MakeHeader(0x001E, 2, 2), nullptr, "GetEventLog"},
        {IPC::MakeHeader(0x001F, 0, 0), nullptr, "GetEventLogStart"},
        // cecd:s commands
        {IPC::MakeHeader(0x0402, 0, 2), nullptr, "GetCecInfoEventHandleSys"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
