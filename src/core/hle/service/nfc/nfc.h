// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace NFC {

enum class TagState : u8 {
    NotInitialized = 0,
    NotScanning = 1,
    Scanning = 2,
    TagInRange = 3,
    TagOutOfRange = 4,
    TagDataLoaded = 5,
};

enum class CommunicationStatus : u8 {
    AttemptInitialize = 1,
    NfcInitialized = 2,
};

/**
 * NFC::Initialize service function
 *  Inputs:
 *      0 : Header code [0x00010040]
 *      1 : (u8) unknown parameter. Can be either value 0x1 or 0x2
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Initialize(Interface* self);

/**
 * NFC::Shutdown service function
 *  Inputs:
 *      0 : Header code [0x00020040]
 *      1 : (u8) unknown parameter
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Shutdown(Interface* self);

/**
 * NFC::StartCommunication service function
 *  Inputs:
 *      0 : Header code [0x00030000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StartCommunication(Interface* self);

/**
 * NFC::StopCommunication service function
 *  Inputs:
 *      0 : Header code [0x00040000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StopCommunication(Interface* self);

/**
 * NFC::StartTagScanning service function
 *  Inputs:
 *      0 : Header code [0x00050040]
 *      1 : (u16) unknown. This is normally 0x0
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StartTagScanning(Interface* self);

/**
 * NFC::StopTagScanning service function
 *  Inputs:
 *      0 : Header code [0x00060000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void StopTagScanning(Interface* self);

/**
 * NFC::LoadAmiiboData service function
 *  Inputs:
 *      0 : Header code [0x00070000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void LoadAmiiboData(Interface* self);

/**
 * NFC::ResetTagScanState service function
 *  Inputs:
 *      0 : Header code [0x00080000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void ResetTagScanState(Interface* self);

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

/**
 * NFC::GetTagOutOfRangeEvent service function
 *  Inputs:
 *      0 : Header code [0x000C0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Copy handle descriptor
 *      3 : Event Handle
 */
void GetTagOutOfRangeEvent(Interface* self);

/**
 * NFC::GetTagState service function
 *  Inputs:
 *      0 : Header code [0x000D0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : (u8) Tag state
 */
void GetTagState(Interface* self);

/**
 * NFC::CommunicationGetStatus service function
 *  Inputs:
 *      0 : Header code [0x000F0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : (u8) Communication state
 */
void CommunicationGetStatus(Interface* self);

/// Initialize all NFC services.
void Init();

/// Shutdown all NFC services.
void Shutdown();

} // namespace NFC
} // namespace Service
