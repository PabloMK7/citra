// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_u.h"

namespace Service {
namespace CECD {

// Empty arrays are illegal -- commented out until an entry is added.
//const Interface::FunctionInfo FunctionTable[] = { };

CECD_U_Interface::CECD_U_Interface() {
    //Register(FunctionTable);
}

} // namespace CECD
} // namespace Service
