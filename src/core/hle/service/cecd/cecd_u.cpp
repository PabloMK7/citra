// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

static const Interface::FunctionInfo FunctionTable[] = {
    { 0x00120104, nullptr, "ReadSavedData" },
};

CECD_U_Interface::CECD_U_Interface() {
    Register(FunctionTable);
}

} // namespace CECD
} // namespace Service
