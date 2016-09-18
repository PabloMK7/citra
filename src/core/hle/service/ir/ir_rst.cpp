// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"

namespace Service {
namespace IR {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, GetHandles, "GetHandles"},
    {0x00020080, nullptr, "Initialize"},
    {0x00030000, nullptr, "Shutdown"},
    {0x00090000, nullptr, "WriteToTwoFields"},
};

IR_RST_Interface::IR_RST_Interface() {
    Register(FunctionTable);
}

} // namespace IR
} // namespace Service
