// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/am_sys.h"

namespace AM_SYS {

/**
 * Gets the number of installed titles in the requested media type
 *  Inputs:
 *    0: Command header (0x00010040)
 *    1: Media type to load the titles from
 *  Outputs:
 *    1: Result, 0 on success, otherwise error code
 *    2: The number of titles in the requested media type
 */
static void TitleIDListGetTotal(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 media_type = cmd_buff[1] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    LOG_WARNING(Service_CFG, "(STUBBED) media_type %u", media_type);
}

/**
 * Loads information about the desired number of titles from the desired media type into an array
 *  Inputs:
 *    0: Command header (0x00020082)
 *    1: The maximum number of titles to load
 *    2: Media type to load the titles from
 *    3: Descriptor of the output buffer pointer
 *    4: Address of the output buffer
 *  Outputs:
 *    1: Result, 0 on success, otherwise error code
 *    2: The number of titles loaded from the requested media type
 */
static void GetTitleIDList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 num_titles = cmd_buff[1];
    u32 media_type = cmd_buff[2] & 0xFF;
    u32 addr = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    LOG_WARNING(Service_CFG, "(STUBBED) Requested %u titles from media type %u", num_titles, media_type);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, TitleIDListGetTotal, "TitleIDListGetTotal"},
    {0x00020082, GetTitleIDList, "GetTitleIDList"},
};

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
