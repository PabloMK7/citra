// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/boss/boss_u.h"

namespace Service {
namespace BOSS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00020100, nullptr, "GetStorageInfo"},
    {0x000C0082, nullptr, "UnregisterTask"},
    {0x001E0042, nullptr, "CancelTask"},
    {0x00330042, nullptr, "StartBgImmediate"},
};

BOSS_U_Interface::BOSS_U_Interface() {
    Register(FunctionTable);
}

} // namespace BOSS
} // namespace Service
