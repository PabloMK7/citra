// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace AC {

/**
 * AC::CreateDefaultConfig service function
 *  Inputs:
 *      64 : ACConfig size << 14 | 2
 *      65 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void CreateDefaultConfig(Interface* self);

/**
 * AC::ConnectAsync service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Connection Event handle
 *      5 : ACConfig size << 14 | 2
 *      6 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void ConnectAsync(Interface* self);

/**
 * AC::GetConnectResult service function
 *  Inputs:
 *      1 : ProcessId Header
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetConnectResult(Interface* self);

/**
 * AC::CloseAsync service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Event handle, should be signaled when AC connection is closed
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void CloseAsync(Interface* self);

/**
 * AC::GetCloseResult service function
 *  Inputs:
 *      1 : ProcessId Header
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetCloseResult(Interface* self);

/**
 * AC::GetWifiStatus service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output connection type, 0 = none, 1 = Old3DS Internet, 2 = New3DS Internet.
 */
void GetWifiStatus(Interface* self);

/**
 * AC::GetInfraPriority service function
 *  Inputs:
 *      1 : ACConfig size << 14 | 2
 *      2 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Infra Priority
 */
void GetInfraPriority(Interface* self);

/**
 * AC::SetRequestEulaVersion service function
 *  Inputs:
 *      1 : Eula Version major
 *      2 : Eula Version minor
 *      3 : ACConfig size << 14 | 2
 *      4 : Input pointer to ACConfig struct
 *      64 : ACConfig size << 14 | 2
 *      65 : Output pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Infra Priority
 */
void SetRequestEulaVersion(Interface* self);

/**
 * AC::RegisterDisconnectEvent service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Event handle, should be signaled when AC connection is closed
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void RegisterDisconnectEvent(Interface* self);

/**
 * AC::IsConnected service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : bool, is connected
 */
void IsConnected(Interface* self);

/**
 * AC::SetClientVersion service function
 *  Inputs:
 *      1 : Used SDK Version
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetClientVersion(Interface* self);

/// Initialize AC service
void Init();

/// Shutdown AC service
void Shutdown();

} // namespace AC
} // namespace Service
