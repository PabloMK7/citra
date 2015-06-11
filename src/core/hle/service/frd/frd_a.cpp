// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"

namespace Service {
namespace FRD {

// Empty arrays are illegal -- commented out until an entry is added.
// const Interface::FunctionInfo FunctionTable[] = { };

FRD_A_Interface::FRD_A_Interface() {
    //Register(FunctionTable);
}

} // namespace FRD
} // namespace Service
