// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/boss_p.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace BOSS_P

namespace BOSS_P {

// Empty arrays are illegal -- commented out until an entry is added.
// const Interface::FunctionInfo FunctionTable[] = { };

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    //Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
