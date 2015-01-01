// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "common/make_unique.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/hle/hle.h"
#include "core/hle/service/ptm_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace PTM_U

namespace PTM_U {

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
static const GameCoin default_game_coin = { 0x4F00, 42, 0, 0, 0, 2014, 12, 29 };
static std::unique_ptr<FileSys::Archive_ExtSaveData> ptm_shared_extsavedata;
static const std::vector<u8> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

/// Charge levels used by PTM functions
enum class ChargeLevels : u32 {
    CriticalBattery    = 1,
    LowBattery         = 2,
    HalfFull           = 3,
    MostlyFull         = 4,
    CompletelyFull     = 5,
};

static bool shell_open = true;

static bool battery_is_charging = true;

/**
 * It is unknown if GetAdapterState is the same as GetBatteryChargeState,
 * it is likely to just be a duplicate function of GetBatteryChargeState
 * that controls another part of the HW.
 * PTM_U::GetAdapterState service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output of function, 0 = not charging, 1 = charging.
 */
static void GetAdapterState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = 0; // No error
    cmd_buff[2] = battery_is_charging ? 1 : 0;

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

/*
 * PTM_User::GetShellState service function.
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Whether the 3DS's physical shell casing is open (1) or closed (0)
 */
static void GetShellState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0;
    cmd_buff[2] = shell_open ? 1 : 0;

    LOG_TRACE(Service_PTM, "PTM_U::GetShellState called");
}

/**
 * PTM_U::GetBatteryLevel service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Battery level, 5 = completely full battery, 4 = mostly full battery,
 *          3 = half full battery, 2 =  low battery, 1 = critical battery.
 */
static void GetBatteryLevel(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = 0; // No error
    cmd_buff[2] = static_cast<u32>(ChargeLevels::CompletelyFull); // Set to a completely full battery

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

/**
 * PTM_U::GetBatteryChargeState service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output of function, 0 = not charging, 1 = charging.
 */
static void GetBatteryChargeState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = 0; // No error
    cmd_buff[2] = battery_is_charging ? 1 : 0;

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, nullptr,               "RegisterAlarmClient"},
    {0x00020080, nullptr,               "SetRtcAlarm"},
    {0x00030000, nullptr,               "GetRtcAlarm"},
    {0x00040000, nullptr,               "CancelRtcAlarm"},
    {0x00050000, GetAdapterState,       "GetAdapterState"},
    {0x00060000, GetShellState,         "GetShellState"},
    {0x00070000, GetBatteryLevel,       "GetBatteryLevel"},
    {0x00080000, GetBatteryChargeState, "GetBatteryChargeState"},
    {0x00090000, nullptr,               "GetPedometerState"},
    {0x000A0042, nullptr,               "GetStepHistoryEntry"},
    {0x000B00C2, nullptr,               "GetStepHistory"},
    {0x000C0000, nullptr,               "GetTotalStepCount"},
    {0x000D0040, nullptr,               "SetPedometerRecordingMode"},
    {0x000E0000, nullptr,               "GetPedometerRecordingMode"},
    {0x000F0084, nullptr,               "GetStepHistoryAll"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
    // Create the SharedExtSaveData archive 0xF000000B and the gamecoin.dat file
    // TODO(Subv): In the future we should use the FS service to query this archive
    std::string extsavedata_directory = FileUtil::GetUserPath(D_SHAREDEXTSAVEDATA);
    ptm_shared_extsavedata = Common::make_unique<FileSys::Archive_ExtSaveData>(extsavedata_directory);
    if (!ptm_shared_extsavedata->Initialize()) {
        LOG_CRITICAL(Service_PTM, "Could not initialize ExtSaveData archive for the PTM:U service");
        return;
    }
    FileSys::Path archive_path(ptm_shared_extdata_id);
    ResultCode result = ptm_shared_extsavedata->Open(archive_path);
    // If the archive didn't exist, create the files inside
    if (result.description == ErrorDescription::FS_NotFormatted) {
        // Format the archive to clear the directories
        ptm_shared_extsavedata->Format(archive_path);
        // Open it again to get a valid archive now that the folder exists
        ptm_shared_extsavedata->Open(archive_path);
        FileSys::Path gamecoin_path("gamecoin.dat");
        FileSys::Mode open_mode = {};
        open_mode.write_flag = 1;
        open_mode.create_flag = 1;
        // Open the file and write the default gamecoin information
        auto gamecoin = ptm_shared_extsavedata->OpenFile(gamecoin_path, open_mode);
        if (gamecoin != nullptr) {
            gamecoin->Write(0, sizeof(GameCoin), 1, reinterpret_cast<const u8*>(&default_game_coin));
            gamecoin->Close();
        }
    }
}

} // namespace
