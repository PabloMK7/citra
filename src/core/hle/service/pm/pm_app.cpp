// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm_app.h"

SERIALIZE_EXPORT_IMPL(Service::PM::PM_APP)

namespace Service::PM {

PM_APP::PM_APP() : ServiceFramework("pm:app", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 5, 0), nullptr, "LaunchTitle"},
        {IPC::MakeHeader(0x0002, 2, 2), nullptr, "LaunchFIRM"},
        {IPC::MakeHeader(0x0003, 2, 0), nullptr, "TerminateApplication"},
        {IPC::MakeHeader(0x0004, 4, 0), nullptr, "TerminateTitle"},
        {IPC::MakeHeader(0x0005, 3, 0), nullptr, "TerminateProcess"},
        {IPC::MakeHeader(0x0006, 2, 2), nullptr, "PrepareForReboot"},
        {IPC::MakeHeader(0x0007, 1, 2), nullptr, "GetFIRMLaunchParams"},
        {IPC::MakeHeader(0x0008, 4, 0), nullptr, "GetTitleExheaderFlags"},
        {IPC::MakeHeader(0x0009, 1, 2), nullptr, "SetFIRMLaunchParams"},
        {IPC::MakeHeader(0x000A, 5, 0), nullptr, "SetAppResourceLimit"},
        {IPC::MakeHeader(0x000B, 5, 0), nullptr, "GetAppResourceLimit"},
        {IPC::MakeHeader(0x000C, 2, 0), nullptr, "UnregisterProcess"},
        {IPC::MakeHeader(0x000D, 9, 0), nullptr, "LaunchTitleUpdate"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::PM
