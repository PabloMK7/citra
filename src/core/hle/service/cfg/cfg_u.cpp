// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_u.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_U)

namespace Service::CFG {

CFG_U::CFG_U(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:u", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), &CFG_U::GetConfigInfoBlk2, "GetConfigInfoBlk2"},
        {IPC::MakeHeader(0x0002, 0, 0), &CFG_U::D<&CFG_U::SecureInfoGetRegion, 0x0002>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0003, 1, 0), &CFG_U::GenHashConsoleUnique, "GenHashConsoleUnique"},
        {IPC::MakeHeader(0x0004, 0, 0), &CFG_U::GetRegionCanadaUSA, "GetRegionCanadaUSA"},
        {IPC::MakeHeader(0x0005, 0, 0), &CFG_U::GetSystemModel, "GetSystemModel"},
        {IPC::MakeHeader(0x0006, 0, 0), &CFG_U::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {IPC::MakeHeader(0x0007, 1, 0), nullptr, "WriteToFirstByteCfgSavegame"},
        {IPC::MakeHeader(0x0008, 2, 0), nullptr, "GoThroughTable"},
        {IPC::MakeHeader(0x0009, 1, 0), &CFG_U::GetCountryCodeString, "GetCountryCodeString"},
        {IPC::MakeHeader(0x000A, 1, 0), &CFG_U::GetCountryCodeID, "GetCountryCodeID"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "IsFangateSupported"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
