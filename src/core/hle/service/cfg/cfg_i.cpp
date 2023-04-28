// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_i.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_I)

namespace Service::CFG {

CFG_I::CFG_I(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:i", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), &CFG_I::GetConfigInfoBlk2, "GetConfigInfoBlk2"},
        {IPC::MakeHeader(0x0002, 0, 0), &CFG_I::D<&CFG_I::SecureInfoGetRegion, 0x0002>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0003, 1, 0), &CFG_I::GenHashConsoleUnique, "GenHashConsoleUnique"},
        {IPC::MakeHeader(0x0004, 0, 0), &CFG_I::GetRegionCanadaUSA, "GetRegionCanadaUSA"},
        {IPC::MakeHeader(0x0005, 0, 0), &CFG_I::GetSystemModel, "GetSystemModel"},
        {IPC::MakeHeader(0x0006, 0, 0), &CFG_I::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {IPC::MakeHeader(0x0007, 1, 0), nullptr, "WriteToFirstByteCfgSavegame"},
        {IPC::MakeHeader(0x0008, 2, 0), nullptr, "GoThroughTable"},
        {IPC::MakeHeader(0x0009, 1, 0), &CFG_I::GetCountryCodeString, "GetCountryCodeString"},
        {IPC::MakeHeader(0x000A, 1, 0), &CFG_I::GetCountryCodeID, "GetCountryCodeID"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "IsFangateSupported"},
        // cfg:i
        {IPC::MakeHeader(0x0401, 2, 2), &CFG_I::D<&CFG_I::GetConfigInfoBlk8, 0x0401>, "GetConfigInfoBlk8"},
        {IPC::MakeHeader(0x0402, 2, 2), &CFG_I::D<&CFG_I::SetConfigInfoBlk4, 0x0402>, "SetConfigInfoBlk4"},
        {IPC::MakeHeader(0x0403, 0, 0), &CFG_I::D<&CFG_I::UpdateConfigNANDSavegame, 0x0403>, "UpdateConfigNANDSavegame"},
        {IPC::MakeHeader(0x0404, 1, 2), nullptr, "GetLocalFriendCodeSeedData"},
        {IPC::MakeHeader(0x0405, 0, 0), nullptr, "GetLocalFriendCodeSeed"},
        {IPC::MakeHeader(0x0406, 0, 0), &CFG_I::D<&CFG_I::SecureInfoGetRegion, 0x0406>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0407, 0, 0), nullptr, "SecureInfoGetByte101"},
        {IPC::MakeHeader(0x0408, 1, 2), nullptr, "SecureInfoGetSerialNo"},
        {IPC::MakeHeader(0x0409, 0, 0), nullptr, "UpdateConfigBlk00040003"},
        {IPC::MakeHeader(0x0801, 2, 2), &CFG_I::D<&CFG_I::GetConfigInfoBlk8, 0x0801>, "GetConfigInfoBlk8"},
        {IPC::MakeHeader(0x0802, 2, 2), &CFG_I::D<&CFG_I::SetConfigInfoBlk4, 0x0802>, "SetConfigInfoBlk4"},
        {IPC::MakeHeader(0x0803, 0, 0), &CFG_I::D<&CFG_I::UpdateConfigNANDSavegame, 0x0803>, "UpdateConfigNANDSavegame"},
        {IPC::MakeHeader(0x0804, 3, 2), nullptr, "CreateConfigInfoBlk"},
        {IPC::MakeHeader(0x0805, 0, 0), nullptr, "DeleteConfigNANDSavefile"},
        {IPC::MakeHeader(0x0806, 0, 0), &CFG_I::FormatConfig, "FormatConfig"},
        {IPC::MakeHeader(0x0808, 0, 0), nullptr, "UpdateConfigBlk1"},
        {IPC::MakeHeader(0x0809, 0, 0), nullptr, "UpdateConfigBlk2"},
        {IPC::MakeHeader(0x080A, 0, 0), nullptr, "UpdateConfigBlk3"},
        {IPC::MakeHeader(0x080B, 2, 2), nullptr, "SetGetLocalFriendCodeSeedData"},
        {IPC::MakeHeader(0x080C, 1, 2), nullptr, "SetLocalFriendCodeSeedSignature"},
        {IPC::MakeHeader(0x080D, 0, 0), nullptr, "DeleteCreateNANDLocalFriendCodeSeed"},
        {IPC::MakeHeader(0x080E, 0, 0), nullptr, "VerifySigLocalFriendCodeSeed"},
        {IPC::MakeHeader(0x080F, 1, 2), nullptr, "GetLocalFriendCodeSeedData"},
        {IPC::MakeHeader(0x0810, 0, 0), nullptr, "GetLocalFriendCodeSeed"},
        {IPC::MakeHeader(0x0811, 2, 4), nullptr, "SetSecureInfo"},
        {IPC::MakeHeader(0x0812, 0, 0), nullptr, "DeleteCreateNANDSecureInfo"},
        {IPC::MakeHeader(0x0813, 0, 0), nullptr, "VerifySigSecureInfo"},
        {IPC::MakeHeader(0x0814, 1, 2), nullptr, "SecureInfoGetData"},
        {IPC::MakeHeader(0x0815, 1, 2), nullptr, "SecureInfoGetSignature"},
        {IPC::MakeHeader(0x0816, 0, 0), &CFG_I::D<&CFG_I::SecureInfoGetRegion, 0x0816>, "SecureInfoGetRegion"},
        {IPC::MakeHeader(0x0817, 0, 0), nullptr, "SecureInfoGetByte101"},
        {IPC::MakeHeader(0x0818, 1, 2), nullptr, "SecureInfoGetSerialNo"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
