// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cfg/cfg_nor.h"

namespace Service::CFG {

CFG_NOR::CFG_NOR() : ServiceFramework("cfg:nor", 23) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Shutdown"},
        {0x0005, nullptr, "ReadData"},
        {0x0006, nullptr, "WriteData"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
