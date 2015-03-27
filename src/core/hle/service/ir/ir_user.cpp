// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_user.h"

namespace Service {
namespace IR {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010182, nullptr,                 "InitializeIrNop"},
    {0x00020000, nullptr,                 "FinalizeIrNop"},
    {0x00030000, nullptr,                 "ClearReceiveBuffer"},
    {0x00040000, nullptr,                 "ClearSendBuffer"},
    {0x00060040, nullptr,                 "RequireConnection"},
    {0x00090000, nullptr,                 "Disconnect"},
    {0x000A0000, nullptr,                 "GetReceiveEvent"},
    {0x000B0000, nullptr,                 "GetSendEvent"},
    {0x000C0000, nullptr,                 "GetConnectionStatusEvent"},
    {0x000D0042, nullptr,                 "SendIrNop"},
    {0x000E0042, nullptr,                 "SendIrNopLarge"},
    {0x00180182, nullptr,                 "InitializeIrNopShared"},
    {0x00190040, nullptr,                 "ReleaseReceivedData"},
    {0x001A0040, nullptr,                 "SetOwnMachineId"},
};

IR_User_Interface::IR_User_Interface() {
    Register(FunctionTable);
}

} // namespace IR
} // namespace Service
