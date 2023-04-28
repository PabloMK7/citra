// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/gsp/gsp_lcd.h"

SERIALIZE_EXPORT_IMPL(Service::GSP::GSP_LCD)

namespace Service::GSP {

GSP_LCD::GSP_LCD() : ServiceFramework("gsp::Lcd") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x000A, 2, 0), nullptr, "SetBrightnessRaw"},
        {IPC::MakeHeader(0x000B, 2, 0), nullptr, "SetBrightness"},
        {IPC::MakeHeader(0x000F, 0, 0), nullptr, "PowerOnAllBacklights"},
        {IPC::MakeHeader(0x0010, 0, 0), nullptr, "PowerOffAllBacklights"},
        {IPC::MakeHeader(0x0011, 1, 0), nullptr, "PowerOnBacklight"},
        {IPC::MakeHeader(0x0012, 1, 0), nullptr, "PowerOffBacklight"},
        {IPC::MakeHeader(0x0013, 1, 0), nullptr, "SetLedForceOff"},
        {IPC::MakeHeader(0x0014, 0, 0), nullptr, "GetVendor"},
        {IPC::MakeHeader(0x0015, 1, 0), nullptr, "GetBrightness"},
        // clang-format on
    };
    RegisterHandlers(functions);
};

} // namespace Service::GSP
