// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/nim/nim_aoc.h"

namespace Service {
namespace NIM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00030042, nullptr,               "SetApplicationId"},
    {0x00040042, nullptr,               "SetTin"},
    {0x000902D0, nullptr,               "ListContentSetsEx"},
    {0x00180000, nullptr,               "GetBalance"},
    {0x001D0000, nullptr,               "GetCustomerSupportCode"},
    {0x00210000, nullptr,               "Initialize"},
    {0x00240282, nullptr,               "CalculateContentsRequiredSize"},
    {0x00250000, nullptr,               "RefreshServerTime"},
};

NIM_AOC_Interface::NIM_AOC_Interface() {
    Register(FunctionTable);
}

} // namespace NIM
} // namespace Service
