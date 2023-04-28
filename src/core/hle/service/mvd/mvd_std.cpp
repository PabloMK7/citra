// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/mvd/mvd_std.h"

SERIALIZE_EXPORT_IMPL(Service::MVD::MVD_STD)

namespace Service::MVD {

MVD_STD::MVD_STD() : ServiceFramework("mvd:std", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), nullptr, "Initialize"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "Shutdown"},
        {IPC::MakeHeader(0x0003, 12, 0), nullptr, "CalculateWorkBufSize"},
        {IPC::MakeHeader(0x0004, 3, 0), nullptr, "CalculateImageSize"},
        {IPC::MakeHeader(0x0008, 5, 2), nullptr, "ProcessNALUnit"},
        {IPC::MakeHeader(0x0009, 1, 2), nullptr, "ControlFrameRendering"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetStatus"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "GetStatusOther"},
        {IPC::MakeHeader(0x001D, 1, 2), nullptr, "GetConfig"},
        {IPC::MakeHeader(0x001E, 1, 4), nullptr, "SetConfig"},
        {IPC::MakeHeader(0x001F, 36, 2), nullptr, "SetOutputBuffer"},
        {IPC::MakeHeader(0x0021, 4, 0), nullptr, "OverrideOutputBuffers"}
        // clang-format on
    };

    RegisterHandlers(functions);
};

} // namespace Service::MVD
