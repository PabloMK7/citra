// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_u.h"

namespace Service {
namespace BOSS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00020100, nullptr,               "GetStorageInfo"},
};

BOSS_U_Interface::BOSS_U_Interface() {
    Register(FunctionTable);
}

} // namespace BOSS
} // namespace Service
