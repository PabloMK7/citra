// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace NFC {

/**
 * NFC::GetTagInRangeEvent service function
 *  Inputs:
 *      0 : Header code [0x000B0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Copy handle descriptor
 *      3 : Event Handle
 */
void GetTagInRangeEvent(Interface* self);

/// Initialize all NFC services.
void Init();

/// Shutdown all NFC services.
void Shutdown();

} // namespace NFC
} // namespace Service
