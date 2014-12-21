// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/ldr_ro.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C2, nullptr,               "Initialize"},
    {0x00020082, nullptr,               "CRR_Load"},
    {0x00030042, nullptr,               "CRR_Unload"},
    {0x000402C2, nullptr,               "CRO_LoadAndFix"},
    {0x000500C2, nullptr,               "CRO_ApplyRelocationPatchesAndLink"}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
