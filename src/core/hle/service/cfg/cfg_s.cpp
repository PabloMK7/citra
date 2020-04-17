// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_s.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_S)

namespace Service::CFG {

CFG_S::CFG_S(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:s", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        {0x00010082, &CFG_S::GetConfigInfoBlk2, "GetConfigInfoBlk2"},
        {0x00020000, &CFG_S::D<&CFG_S::SecureInfoGetRegion, 0x0002>, "SecureInfoGetRegion"},
        {0x00030040, &CFG_S::GenHashConsoleUnique, "GenHashConsoleUnique"},
        {0x00040000, &CFG_S::GetRegionCanadaUSA, "GetRegionCanadaUSA"},
        {0x00050000, &CFG_S::GetSystemModel, "GetSystemModel"},
        {0x00060000, &CFG_S::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {0x00070040, nullptr, "WriteToFirstByteCfgSavegame"},
        {0x00080080, nullptr, "GoThroughTable"},
        {0x00090040, &CFG_S::GetCountryCodeString, "GetCountryCodeString"},
        {0x000A0040, &CFG_S::GetCountryCodeID, "GetCountryCodeID"},
        {0x000B0000, nullptr, "IsFangateSupported"},
        // cfg:s
        {0x04010082, &CFG_S::D<&CFG_S::GetConfigInfoBlk8, 0x0401>, "GetConfigInfoBlk8"},
        {0x04020082, &CFG_S::D<&CFG_S::SetConfigInfoBlk4, 0x0402>, "SetConfigInfoBlk4"},
        {0x04030000, &CFG_S::D<&CFG_S::UpdateConfigNANDSavegame, 0x0403>,
         "UpdateConfigNANDSavegame"},
        {0x04040042, nullptr, "GetLocalFriendCodeSeedData"},
        {0x04050000, nullptr, "GetLocalFriendCodeSeed"},
        {0x04060000, &CFG_S::D<&CFG_S::SecureInfoGetRegion, 0x0406>, "SecureInfoGetRegion"},
        {0x04070000, nullptr, "SecureInfoGetByte101"},
        {0x04080042, nullptr, "SecureInfoGetSerialNo"},
        {0x04090000, nullptr, "UpdateConfigBlk00040003"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
