// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ptm/ptm_sets.h"

namespace Service::PTM {

PTM_Sets::PTM_Sets(std::shared_ptr<Module> ptm) : Module::Interface(std::move(ptm), "ptm:sets", 1) {
    static const FunctionInfo functions[] = {
        // Note that this service does not have access to ptm:u's common commands
        // clang-format off
        {0x0001, nullptr, "SetSystemTime"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
