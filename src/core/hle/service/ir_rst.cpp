// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/ir_rst.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace IR_RST

namespace IR_RST {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr,                 "GetHandles"},
    {0x00020080, nullptr,                 "Initialize"},
    {0x00030000, nullptr,                 "Shutdown"},
    {0x00040000, nullptr,                 "Unknown"},
    {0x00050000, nullptr,                 "Unknown"},
    {0x00060000, nullptr,                 "Unknown"},
    {0x00070080, nullptr,                 "Unknown"},
    {0x00080000, nullptr,                 "Unknown"},
    {0x00090000, nullptr,                 "Unknown"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
