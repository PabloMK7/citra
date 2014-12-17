// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg_i.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_I

namespace CFG_I {

const Interface::FunctionInfo FunctionTable[] = {
    {0x04010082, nullptr,               "GetConfigInfoBlk8"},
    {0x04020082, nullptr,               "GetConfigInfoBlk4"},
    {0x04030000, nullptr,               "UpdateConfigNANDSavegame"},
    {0x04040042, nullptr,               "GetLocalFriendCodeSeedData"},
    {0x04050000, nullptr,               "GetLocalFriendCodeSeed"},
    {0x04060000, nullptr,               "SecureInfoGetRegion"},
    {0x04070000, nullptr,               "SecureInfoGetByte101"},
    {0x04080042, nullptr,               "SecureInfoGetSerialNo"},
    {0x04090000, nullptr,               "UpdateConfigBlk00040003"},
    {0x08010082, nullptr,               "GetConfigInfoBlk8"},
    {0x08020082, nullptr,               "GetConfigInfoBlk4"},
    {0x08030000, nullptr,               "UpdateConfigNANDSavegame"},
    {0x080400C2, nullptr,               "CreateConfigInfoBlk"},
    {0x08050000, nullptr,               "DeleteConfigNANDSavefile"},
    {0x08060000, nullptr,               "FormatConfig"},
    {0x08070000, nullptr,               "Unknown"},
    {0x08080000, nullptr,               "UpdateConfigBlk1"},
    {0x08090000, nullptr,               "UpdateConfigBlk2"},
    {0x080A0000, nullptr,               "UpdateConfigBlk3"},
    {0x080B0082, nullptr,               "SetGetLocalFriendCodeSeedData"},
    {0x080C0042, nullptr,               "SetLocalFriendCodeSeedSignature"},
    {0x080D0000, nullptr,               "DeleteCreateNANDLocalFriendCodeSeed"},
    {0x080E0000, nullptr,               "VerifySigLocalFriendCodeSeed"},
    {0x080F0042, nullptr,               "GetLocalFriendCodeSeedData"},
    {0x08100000, nullptr,               "GetLocalFriendCodeSeed"},
    {0x08110084, nullptr,               "SetSecureInfo"},
    {0x08120000, nullptr,               "DeleteCreateNANDSecureInfo"},
    {0x08130000, nullptr,               "VerifySigSecureInfo"},
    {0x08140042, nullptr,               "SecureInfoGetData"},
    {0x08150042, nullptr,               "SecureInfoGetSignature"},
    {0x08160000, nullptr,               "SecureInfoGetRegion"},
    {0x08170000, nullptr,               "SecureInfoGetByte101"},
    {0x08180042, nullptr,               "SecureInfoGetSerialNo"},
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
