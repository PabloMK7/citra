// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/ir_rst.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace IR_RST

namespace IR_RST {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr,                 "GetHandles"},
    {0x00020080, nullptr,                 "Initialize"},
    {0x00030000, nullptr,                 "Shutdown"},
    {0x00090000, nullptr,                 "WriteToTwoFields"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
