// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_i.h"

namespace Service {
namespace CFG {

const Interface::FunctionInfo FunctionTable[] = {
    // cfg common
    {0x00010082, GetConfigInfoBlk2,                    "GetConfigInfoBlk2"},
    {0x00020000, SecureInfoGetRegion,                  "SecureInfoGetRegion"},
    {0x00030040, GenHashConsoleUnique,                 "GenHashConsoleUnique"},
    {0x00040000, GetRegionCanadaUSA,                   "GetRegionCanadaUSA"},
    {0x00050000, GetSystemModel,                       "GetSystemModel"},
    {0x00060000, GetModelNintendo2DS,                  "GetModelNintendo2DS"},
    {0x00070040, nullptr,                              "WriteToFirstByteCfgSavegame"},
    {0x00080080, nullptr,                              "GoThroughTable"},
    {0x00090040, GetCountryCodeString,                 "GetCountryCodeString"},
    {0x000A0040, GetCountryCodeID,                     "GetCountryCodeID"},
    // cfg:i
    {0x04010082, GetConfigInfoBlk8,                    "GetConfigInfoBlk8"},
    {0x04020082, SetConfigInfoBlk4,                    "SetConfigInfoBlk4"},
    {0x04030000, UpdateConfigNANDSavegame,             "UpdateConfigNANDSavegame"},
    {0x04040042, nullptr,                              "GetLocalFriendCodeSeedData"},
    {0x04050000, nullptr,                              "GetLocalFriendCodeSeed"},
    {0x04060000, SecureInfoGetRegion,                  "SecureInfoGetRegion"},
    {0x04070000, nullptr,                              "SecureInfoGetByte101"},
    {0x04080042, nullptr,                              "SecureInfoGetSerialNo"},
    {0x04090000, nullptr,                              "UpdateConfigBlk00040003"},
    {0x08010082, GetConfigInfoBlk8,                    "GetConfigInfoBlk8"},
    {0x08020082, SetConfigInfoBlk4,                    "SetConfigInfoBlk4"},
    {0x08030000, UpdateConfigNANDSavegame,             "UpdateConfigNANDSavegame"},
    {0x080400C2, nullptr,                              "CreateConfigInfoBlk"},
    {0x08050000, nullptr,                              "DeleteConfigNANDSavefile"},
    {0x08060000, FormatConfig,                         "FormatConfig"},
    {0x08080000, nullptr,                              "UpdateConfigBlk1"},
    {0x08090000, nullptr,                              "UpdateConfigBlk2"},
    {0x080A0000, nullptr,                              "UpdateConfigBlk3"},
    {0x080B0082, nullptr,                              "SetGetLocalFriendCodeSeedData"},
    {0x080C0042, nullptr,                              "SetLocalFriendCodeSeedSignature"},
    {0x080D0000, nullptr,                              "DeleteCreateNANDLocalFriendCodeSeed"},
    {0x080E0000, nullptr,                              "VerifySigLocalFriendCodeSeed"},
    {0x080F0042, nullptr,                              "GetLocalFriendCodeSeedData"},
    {0x08100000, nullptr,                              "GetLocalFriendCodeSeed"},
    {0x08110084, nullptr,                              "SetSecureInfo"},
    {0x08120000, nullptr,                              "DeleteCreateNANDSecureInfo"},
    {0x08130000, nullptr,                              "VerifySigSecureInfo"},
    {0x08140042, nullptr,                              "SecureInfoGetData"},
    {0x08150042, nullptr,                              "SecureInfoGetSignature"},
    {0x08160000, SecureInfoGetRegion,                  "SecureInfoGetRegion"},
    {0x08170000, nullptr,                              "SecureInfoGetByte101"},
    {0x08180042, nullptr,                              "SecureInfoGetSerialNo"},
};

CFG_I_Interface::CFG_I_Interface() {
    Register(FunctionTable);
}

} // namespace CFG
} // namespace Service