// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_s.h"

namespace Service {
namespace CFG {

const Interface::FunctionInfo FunctionTable[] = {
    // cfg common
    {0x00010082, GetConfigInfoBlk2, "GetConfigInfoBlk2"},
    {0x00020000, SecureInfoGetRegion, "SecureInfoGetRegion"},
    {0x00030040, GenHashConsoleUnique, "GenHashConsoleUnique"},
    {0x00040000, GetRegionCanadaUSA, "GetRegionCanadaUSA"},
    {0x00050000, GetSystemModel, "GetSystemModel"},
    {0x00060000, GetModelNintendo2DS, "GetModelNintendo2DS"},
    {0x00070040, nullptr, "WriteToFirstByteCfgSavegame"},
    {0x00080080, nullptr, "GoThroughTable"},
    {0x00090040, GetCountryCodeString, "GetCountryCodeString"},
    {0x000A0040, GetCountryCodeID, "GetCountryCodeID"},
    // cfg:s
    {0x04010082, GetConfigInfoBlk8, "GetConfigInfoBlk8"},
    {0x04020082, SetConfigInfoBlk4, "SetConfigInfoBlk4"},
    {0x04030000, UpdateConfigNANDSavegame, "UpdateConfigNANDSavegame"},
    {0x04040042, nullptr, "GetLocalFriendCodeSeedData"},
    {0x04050000, nullptr, "GetLocalFriendCodeSeed"},
    {0x04060000, nullptr, "SecureInfoGetRegion"},
    {0x04070000, nullptr, "SecureInfoGetByte101"},
    {0x04080042, nullptr, "SecureInfoGetSerialNo"},
    {0x04090000, nullptr, "UpdateConfigBlk00040003"},
};

CFG_S_Interface::CFG_S_Interface() {
    Register(FunctionTable);
}

} // namespace CFG
} // namespace Service
