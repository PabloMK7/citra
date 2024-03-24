// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cecd/cecd_ndm.h"

namespace Service::CECD {

CECD_NDM::CECD_NDM(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:ndm", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Deinitialize"},
        {0x0003, nullptr, "ResumeDaemon"},
        {0x0004, nullptr, "SuspendDaemon"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
