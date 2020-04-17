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
        {0x00010082, nullptr, "Initialize"},
        {0x00020000, nullptr, "Shutdown"},
        {0x00030300, nullptr, "CalculateWorkBufSize"},
        {0x000400C0, nullptr, "CalculateImageSize"},
        {0x00080142, nullptr, "ProcessNALUnit"},
        {0x00090042, nullptr, "ControlFrameRendering"},
        {0x000A0000, nullptr, "GetStatus"},
        {0x000B0000, nullptr, "GetStatusOther"},
        {0x001D0042, nullptr, "GetConfig"},
        {0x001E0044, nullptr, "SetConfig"},
        {0x001F0902, nullptr, "SetOutputBuffer"},
        {0x00210100, nullptr, "OverrideOutputBuffers"}
        // clang-format on
    };

    RegisterHandlers(functions);
};

} // namespace Service::MVD
