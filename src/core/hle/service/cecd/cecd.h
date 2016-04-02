// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace CECD {

/**
 * GetCecInfoEventHandle service function
 *  Inputs:
 *      0: 0x000F0000
 *  Outputs:
 *      1: ResultCode
 *      3: Event Handle
 */
void GetCecInfoEventHandle(Service::Interface* self);

/**
 * GetChangeStateEventHandle service function
 *  Inputs:
 *      0: 0x00100000
 *  Outputs:
 *      1: ResultCode
 *      3: Event Handle
 */
void GetChangeStateEventHandle(Service::Interface* self);

/// Initialize CECD service(s)
void Init();

/// Shutdown CECD service(s)
void Shutdown();

} // namespace CECD
} // namespace Service
