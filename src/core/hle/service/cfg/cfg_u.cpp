// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cfg/cfg_u.h"

namespace Service::CFG {

CFG_U::CFG_U(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:u", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        {0x00010082, &CFG_U::GetConfigInfoBlk2, "GetConfigInfoBlk2"},
        {0x00020000, &CFG_U::D<&CFG_U::SecureInfoGetRegion, 0x0002>, "SecureInfoGetRegion"},
        {0x00030040, &CFG_U::GenHashConsoleUnique, "GenHashConsoleUnique"},
        {0x00040000, &CFG_U::GetRegionCanadaUSA, "GetRegionCanadaUSA"},
        {0x00050000, &CFG_U::GetSystemModel, "GetSystemModel"},
        {0x00060000, &CFG_U::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {0x00070040, nullptr, "WriteToFirstByteCfgSavegame"},
        {0x00080080, nullptr, "GoThroughTable"},
        {0x00090040, &CFG_U::GetCountryCodeString, "GetCountryCodeString"},
        {0x000A0040, &CFG_U::GetCountryCodeID, "GetCountryCodeID"},
        {0x000B0000, nullptr, "IsFangateSupported"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
