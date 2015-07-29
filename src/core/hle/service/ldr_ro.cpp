// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/ldr_ro.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

/**
 * LDR_RO::Initialize service function
 *  Inputs:
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Process memory address where the CRS will be mapped
 *      4 : Value, must be zero
 *      5 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crs_buffer_ptr = cmd_buff[1];
    u32 crs_size       = cmd_buff[2];
    u32 address        = cmd_buff[3];
    u32 value          = cmd_buff[4];
    u32 process        = cmd_buff[5];

    if (value != 0) {
        LOG_ERROR(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    // TODO(purpasmart96): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, address=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, address, value, process);
}

/**
 * LDR_RO::LoadCRR service function
 *  Inputs:
 *      1 : CRS buffer pointer
 *      2 : CRS Size
 *      3 : Value, must be zero
 *      4 : KProcess handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void LoadCRR(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 crs_buffer_ptr = cmd_buff[1];
    u32 crs_size       = cmd_buff[2];
    u32 value          = cmd_buff[3];
    u32 process        = cmd_buff[4];

    if (value != 0) {
        LOG_ERROR(Service_LDR, "This value should be zero, but is actually %u!", value);
    }

    // TODO(purpasmart96): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_LDR, "(STUBBED) called. crs_buffer_ptr=0x%08X, crs_size=0x%08X, value=0x%08X, process=0x%08X",
                crs_buffer_ptr, crs_size, value, process);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C2, Initialize,            "Initialize"},
    {0x00020082, LoadCRR,               "LoadCRR"},
    {0x00030042, nullptr,               "UnloadCCR"},
    {0x000402C2, nullptr,               "LoadExeCRO"},
    {0x000500C2, nullptr,               "LoadCROSymbols"},
    {0x00060042, nullptr,               "CRO_Load?"},
    {0x00070042, nullptr,               "LoadCROSymbols"},
    {0x00080042, nullptr,               "Shutdown"},
    {0x000902C2, nullptr,               "LoadExeCRO_New?"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
