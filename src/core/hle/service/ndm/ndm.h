// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace NDM {

/**
 *  SuspendDaemons
 *  Inputs:
 *      0 : Command header (0x00020082)
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void SuspendDaemons(Service::Interface* self);

/**
 *  ResumeDaemons
 *  Inputs:
 *      0 : Command header (0x00020082)
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void ResumeDaemons(Service::Interface* self);

/**
 *  OverrideDefaultDaemons
 *  Inputs:
 *      0 : Command header (0x00020082)
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void OverrideDefaultDaemons(Service::Interface* self);

/// Initialize NDM service
void Init();

/// Shutdown NDM service
void Shutdown();

}// namespace NDM
}// namespace Service
