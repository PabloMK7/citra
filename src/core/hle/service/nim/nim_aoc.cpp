// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nim/nim_aoc.h"

SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_AOC)

namespace Service::NIM {

NIM_AOC::NIM_AOC() : ServiceFramework("nim:aoc", 2) {
    const FunctionInfo functions[] = {
        {0x00030042, nullptr, "SetApplicationId"},
        {0x00040042, nullptr, "SetTin"},
        {0x000902D0, nullptr, "ListContentSetsEx"},
        {0x00180000, nullptr, "GetBalance"},
        {0x001D0000, nullptr, "GetCustomerSupportCode"},
        {0x00210000, nullptr, "Initialize"},
        {0x00240282, nullptr, "CalculateContentsRequiredSize"},
        {0x00250000, nullptr, "RefreshServerTime"},
    };
    RegisterHandlers(functions);
}

NIM_AOC::~NIM_AOC() = default;

} // namespace Service::NIM
