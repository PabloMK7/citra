// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_i.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_I

namespace CFG_I {
    
/**
 * CFG_I::GetConfigInfoBlk8 service function
 * This function is called by two command headers,
 * there appears to be no difference between them according to 3dbrew
 *  Inputs:
 *      0 : 0x04010082 / 0x08010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetConfigInfoBlk8(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 size = cmd_buffer[1];
    u32 block_id = cmd_buffer[2];
    u8* data_pointer = Memory::GetPointer(cmd_buffer[4]);

    if (data_pointer == nullptr) {
        cmd_buffer[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buffer[1] = Service::CFG::GetConfigInfoBlock(block_id, size, 0x8, data_pointer).raw;
}

/**
 * CFG_I::UpdateConfigNANDSavegame service function
 * This function is called by two command headers, 
 * there appears to be no difference between them according to 3dbrew
 *  Inputs:
 *      0 : 0x04030000 / 0x08030000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UpdateConfigNANDSavegame(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    cmd_buffer[1] = Service::CFG::UpdateConfigNANDSavegame().raw;
}

/**
 * CFG_I::FormatConfig service function
 *  Inputs:
 *      0 : 0x08060000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void FormatConfig(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    cmd_buffer[1] = Service::CFG::FormatConfig().raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x04010082, GetConfigInfoBlk8,                    "GetConfigInfoBlk8"},
    {0x04020082, nullptr,                              "SetConfigInfoBlk4"},
    {0x04030000, UpdateConfigNANDSavegame,             "UpdateConfigNANDSavegame"},
    {0x04040042, nullptr,                              "GetLocalFriendCodeSeedData"},
    {0x04050000, nullptr,                              "GetLocalFriendCodeSeed"},
    {0x04060000, nullptr,                              "SecureInfoGetRegion"},
    {0x04070000, nullptr,                              "SecureInfoGetByte101"},
    {0x04080042, nullptr,                              "SecureInfoGetSerialNo"},
    {0x04090000, nullptr,                              "UpdateConfigBlk00040003"},
    {0x08010082, GetConfigInfoBlk8,                    "GetConfigInfoBlk8"},
    {0x08020082, nullptr,                              "SetConfigInfoBlk4"},
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
    {0x08160000, nullptr,                              "SecureInfoGetRegion"},
    {0x08170000, nullptr,                              "SecureInfoGetByte101"},
    {0x08180042, nullptr,                              "SecureInfoGetSerialNo"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
