// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Service {
namespace AM {

/**
 * AM::TitleIDListGetTotal service function
 * Gets the number of installed titles in the requested media type
 *  Inputs:
 *      0 : Command header (0x00010040)
 *      1 : Media type to load the titles from
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : The number of titles in the requested media type
 */
void TitleIDListGetTotal(Service::Interface* self);

/**
 * AM::GetTitleIDList service function
 * Loads information about the desired number of titles from the desired media type into an array
 *  Inputs:
 *      0 : Command header (0x00020082)
 *      1 : The maximum number of titles to load
 *      2 : Media type to load the titles from
 *      3 : Descriptor of the output buffer pointer
 *      4 : Address of the output buffer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : The number of titles loaded from the requested media type
 */
void GetTitleIDList(Service::Interface* self);

/**
 * AM::GetNumContentInfos service function
 *  Inputs:
 *      0 : Command header (0x100100C0)
 *      1 : Unknown
 *      2 : Unknown
 *      3 : Unknown
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Number of content infos plus one
 */
void GetNumContentInfos(Service::Interface* self);

/// Initialize AM service
void Init();

/// Shutdown AM service
void Shutdown();

} // namespace AM
} // namespace Service
