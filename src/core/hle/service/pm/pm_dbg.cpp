// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm_dbg.h"

SERIALIZE_EXPORT_IMPL(Service::PM::PM_DBG)

namespace Service::PM {

PM_DBG::PM_DBG() : ServiceFramework("pm:dbg", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 5, 0), nullptr, "LaunchAppDebug"},
        {IPC::MakeHeader(0x0002, 5, 0), nullptr, "LaunchApp"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "RunQueuedProcess"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::PM
