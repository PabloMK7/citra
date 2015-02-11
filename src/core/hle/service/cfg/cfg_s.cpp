// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_s.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_S

namespace CFG_S {
    
/**
 * CFG_S::GetConfigInfoBlk2 service function
 *  Inputs:
 *      0 : 0x00010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetConfigInfoBlk2(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 size = cmd_buffer[1];
    u32 block_id = cmd_buffer[2];
    u8* data_pointer = Memory::GetPointer(cmd_buffer[4]);

    if (data_pointer == nullptr) {
        cmd_buffer[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buffer[1] = Service::CFG::GetConfigInfoBlock(block_id, size, 0x2, data_pointer).raw;
}

/**
 * CFG_S::GetConfigInfoBlk8 service function
 *  Inputs:
 *      0 : 0x04010082
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
 * CFG_S::UpdateConfigNANDSavegame service function
 *  Inputs:
 *      0 : 0x04030000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void UpdateConfigNANDSavegame(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    cmd_buffer[1] = Service::CFG::UpdateConfigNANDSavegame().raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, GetConfigInfoBlk2,                    "GetConfigInfoBlk2"},
    {0x00020000, nullptr,                              "SecureInfoGetRegion"},
    {0x04010082, GetConfigInfoBlk8,                    "GetConfigInfoBlk8"},
    {0x04020082, nullptr,                              "SetConfigInfoBlk4"},
    {0x04030000, UpdateConfigNANDSavegame,             "UpdateConfigNANDSavegame"},
    {0x04040042, nullptr,                              "GetLocalFriendCodeSeedData"},
    {0x04050000, nullptr,                              "GetLocalFriendCodeSeed"},
    {0x04060000, nullptr,                              "SecureInfoGetRegion"},
    {0x04070000, nullptr,                              "SecureInfoGetByte101"},
    {0x04080042, nullptr,                              "SecureInfoGetSerialNo"},
    {0x04090000, nullptr,                              "UpdateConfigBlk00040003"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
