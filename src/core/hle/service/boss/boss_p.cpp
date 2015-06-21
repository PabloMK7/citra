// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"

namespace Service {
namespace BOSS {

// Empty arrays are illegal -- commented out until an entry is added.
// const Interface::FunctionInfo FunctionTable[] = { };

BOSS_P_Interface::BOSS_P_Interface() {
    //Register(FunctionTable);
}

} // namespace BOSS
} // namespace Service
