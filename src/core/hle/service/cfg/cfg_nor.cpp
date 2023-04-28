// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_nor.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_NOR)

namespace Service::CFG {

CFG_NOR::CFG_NOR() : ServiceFramework("cfg:nor", 23) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 0), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Shutdown"},
        {IPC::MakeHeader(0x0005, 2, 2), nullptr, "ReadData"},
        {IPC::MakeHeader(0x0006, 2, 2), nullptr, "WriteData"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
