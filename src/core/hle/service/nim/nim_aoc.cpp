// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nim/nim_aoc.h"

SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_AOC)

namespace Service::NIM {

NIM_AOC::NIM_AOC() : ServiceFramework("nim:aoc", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0003, 1, 2), nullptr, "SetApplicationId"},
        {IPC::MakeHeader(0x0004, 1, 2), nullptr, "SetTin"},
        {IPC::MakeHeader(0x0009, 11, 16), nullptr, "ListContentSetsEx"},
        {IPC::MakeHeader(0x0018, 0, 0), nullptr, "GetBalance"},
        {IPC::MakeHeader(0x001D, 0, 0), nullptr, "GetCustomerSupportCode"},
        {IPC::MakeHeader(0x0021, 0, 0), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0024, 10, 2), nullptr, "CalculateContentsRequiredSize"},
        {IPC::MakeHeader(0x0025, 0, 0), nullptr, "RefreshServerTime"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

NIM_AOC::~NIM_AOC() = default;

} // namespace Service::NIM
