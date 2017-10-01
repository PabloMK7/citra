// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace NIM {

/**
 * NIM::CheckForSysUpdateEvent service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Copy handle descriptor
 *      3 : System Update event handle
 */
void CheckForSysUpdateEvent(Service::Interface* self);

/**
 * NIM::CheckSysUpdateAvailable service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : flag, 0 = no system update available, 1 = system update available.
 */
void CheckSysUpdateAvailable(Service::Interface* self);

/// Initialize NIM service(s)
void Init();

/// Shutdown NIM service(s)
void Shutdown();

} // namespace NIM
} // namespace Service
