// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm_dbg.h"

namespace Service::PM {

PM_DBG::PM_DBG() : ServiceFramework("pm:dbg", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x00010140, nullptr, "LaunchAppDebug"},
        {0x00020140, nullptr, "LaunchApp"},
        {0x00030000, nullptr, "RunQueuedProcess"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::PM
