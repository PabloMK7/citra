// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_nor.h"

namespace Service {
namespace CFG {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, nullptr, "Initialize"},
    {0x00020000, nullptr, "Shutdown"},
    {0x00050082, nullptr, "ReadData"},
    {0x00060082, nullptr, "WriteData"},
};

CFG_NOR::CFG_NOR() {
    Register(FunctionTable);
}

} // namespace CFG
} // namespace Service
