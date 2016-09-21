// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

static const Interface::FunctionInfo FunctionTable[] = {
    {0x000E0000, GetCecStateAbbreviated, "GetCecStateAbbreviated"},
    {0x000F0000, GetCecInfoEventHandle, "GetCecInfoEventHandle"},
    {0x00100000, GetChangeStateEventHandle, "GetChangeStateEventHandle"},
    {0x00120104, nullptr, "ReadSavedData"},
};

CECD_U_Interface::CECD_U_Interface() {
    Register(FunctionTable);
}

} // namespace CECD
} // namespace Service
