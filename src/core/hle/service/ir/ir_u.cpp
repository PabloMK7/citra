// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ir/ir_u.h"

SERIALIZE_EXPORT_IMPL(Service::IR::IR_U)

namespace Service::IR {

IR_U::IR_U() : ServiceFramework("ir:u", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 0), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Shutdown"},
        {IPC::MakeHeader(0x0003, 1, 2), nullptr, "StartSendTransfer"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "WaitSendTransfer"},
        {IPC::MakeHeader(0x0005, 3, 2), nullptr, "StartRecvTransfer"},
        {IPC::MakeHeader(0x0006, 0, 0), nullptr, "WaitRecvTransfer"},
        {IPC::MakeHeader(0x0007, 2, 0), nullptr, "GetRecvTransferCount"},
        {IPC::MakeHeader(0x0008, 0, 0), nullptr, "GetSendState"},
        {IPC::MakeHeader(0x0009, 1, 0), nullptr, "SetBitRate"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetBitRate"},
        {IPC::MakeHeader(0x000B, 1, 0), nullptr, "SetIRLEDState"},
        {IPC::MakeHeader(0x000C, 0, 0), nullptr, "GetIRLEDRecvState"},
        {IPC::MakeHeader(0x000D, 0, 0), nullptr, "GetSendFinishedEvent"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetRecvFinishedEvent"},
        {IPC::MakeHeader(0x000F, 0, 0), nullptr, "GetTransferState"},
        {IPC::MakeHeader(0x0010, 0, 0), nullptr, "GetErrorStatus"},
        {IPC::MakeHeader(0x0011, 1, 0), nullptr, "SetSleepModeActive"},
        {IPC::MakeHeader(0x0012, 1, 0), nullptr, "SetSleepModeState"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::IR
