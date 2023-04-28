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
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), &CFG_S::GetConfigInfoBlk2, "GetConfigInfoBlk2"},
        {IPC::MakeHeader(0x0002, 0, 0), &CFG_S::D<&CFG_S::SecureInfoGetRegion, 0x0002>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0003, 1, 0), &CFG_S::GenHashConsoleUnique, "GenHashConsoleUnique"},
        {IPC::MakeHeader(0x0004, 0, 0), &CFG_S::GetRegionCanadaUSA, "GetRegionCanadaUSA"},
        {IPC::MakeHeader(0x0005, 0, 0), &CFG_S::GetSystemModel, "GetSystemModel"},
        {IPC::MakeHeader(0x0006, 0, 0), &CFG_S::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {IPC::MakeHeader(0x0007, 1, 0), nullptr, "WriteToFirstByteCfgSavegame"},
        {IPC::MakeHeader(0x0008, 2, 0), nullptr, "GoThroughTable"},
        {IPC::MakeHeader(0x0009, 1, 0), &CFG_S::GetCountryCodeString, "GetCountryCodeString"},
        {IPC::MakeHeader(0x000A, 1, 0), &CFG_S::GetCountryCodeID, "GetCountryCodeID"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "IsFangateSupported"},
        // cfg:s
        {IPC::MakeHeader(0x0401, 2, 2), &CFG_S::D<&CFG_S::GetConfigInfoBlk8, 0x0401>, "GetConfigInfoBlk8"},
        {IPC::MakeHeader(0x0402, 2, 2), &CFG_S::D<&CFG_S::SetConfigInfoBlk4, 0x0402>, "SetConfigInfoBlk4"},
        {IPC::MakeHeader(0x0403, 0, 0), &CFG_S::D<&CFG_S::UpdateConfigNANDSavegame, 0x0403>, "UpdateConfigNANDSavegame"},
        {IPC::MakeHeader(0x0404, 1, 2), nullptr, "GetLocalFriendCodeSeedData"},
        {IPC::MakeHeader(0x0405, 0, 0), nullptr, "GetLocalFriendCodeSeed"},
        {IPC::MakeHeader(0x0406, 0, 0), &CFG_S::D<&CFG_S::SecureInfoGetRegion, 0x0406>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0407, 0, 0), nullptr, "SecureInfoGetByte101"},
        {IPC::MakeHeader(0x0408, 1, 2), nullptr, "SecureInfoGetSerialNo"},
        {IPC::MakeHeader(0x0409, 0, 0), nullptr, "UpdateConfigBlk00040003"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
