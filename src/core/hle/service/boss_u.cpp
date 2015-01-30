// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/boss_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace BOSS_U

namespace BOSS_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00020100, nullptr,               "GetStorageInfo"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
