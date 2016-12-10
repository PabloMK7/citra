// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/act_u.h"

namespace Service {
namespace ACT {

const Interface::FunctionInfo FunctionTable[] = {
    // clang-format off
    {0x00010084, nullptr, "Initialize"},
    {0x00020040, nullptr, "GetErrorCode"},
    {0x000600C2, nullptr, "GetAccountDataBlock"},
    {0x000B0042, nullptr, "AcquireEulaList"},
    {0x000D0040, nullptr, "GenerateUuid"},
    // clang-format on
};

ACT_U::ACT_U() {
    Register(FunctionTable);
}

} // namespace ACT
} // namespace Service
