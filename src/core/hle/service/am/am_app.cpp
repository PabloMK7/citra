// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"

namespace Service {
namespace AM {

// Empty arrays are illegal -- commented out until an entry is added.
//const Interface::FunctionInfo FunctionTable[] = { };

AM_APP_Interface::AM_APP_Interface() {
    //Register(FunctionTable);
}

} // namespace AM
} // namespace Service
