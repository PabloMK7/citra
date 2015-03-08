// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hle/result.h"

namespace Service {
namespace PTM {

/// Charge levels used by PTM functions
enum class ChargeLevels : u32 {
    CriticalBattery    = 1,
    LowBattery         = 2,
    HalfFull           = 3,
    MostlyFull         = 4,
    CompletelyFull     = 5,
};

/** 
 * Represents the gamecoin file structure in the SharedExtData archive
 * More information in 3dbrew (http://www.3dbrew.org/wiki/Extdata#Shared_Extdata_0xf000000b_gamecoin.dat)
 */
struct GameCoin {
    u32 magic; ///< Magic number: 0x4F00
    u16 total_coins; ///< Total Play Coins 
    u16 total_coins_on_date; ///< Total Play Coins obtained on the date stored below.
    u32 step_count; ///< Total step count at the time a new Play Coin was obtained. 
    u32 last_step_count; ///< Step count for the day the last Play Coin was obtained
    u16 year;
    u8 month;
    u8 day;
};

/**
 * Returns whether the battery is charging or not.
 * It is unknown if GetAdapterState is the same as GetBatteryChargeState,
 * it is likely to just be a duplicate function of GetBatteryChargeState
 * that controls another part of the HW.
 * @returns 1 if the battery is charging, and 0 otherwise.
 */
u32 GetAdapterState();

/**
 * Returns whether the 3DS's physical shell casing is open or closed
 * @returns 1 if the shell is open, and 0 if otherwise
 */
u32 GetShellState();

/**
 * Get the current battery's charge level.
 * @returns The battery's charge level.
 */
ChargeLevels GetBatteryLevel();

/// Initialize the PTM service
void Init();

/// Shutdown the PTM service
void Shutdown();

} // namespace PTM
} // namespace Service
