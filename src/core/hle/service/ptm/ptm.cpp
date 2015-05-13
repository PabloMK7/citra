// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/file_sys/file_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_play.h"
#include "core/hle/service/ptm/ptm_sysm.h"
#include "core/hle/service/ptm/ptm_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

/// Values for the default gamecoin.dat file
static const GameCoin default_game_coin = { 0x4F00, 42, 0, 0, 0, 2014, 12, 29 };

/// Id of the SharedExtData archive used by the PTM process
static const std::vector<u8> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

static bool shell_open;

static bool battery_is_charging;

void GetAdapterState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = battery_is_charging ? 1 : 0;

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetShellState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = shell_open ? 1 : 0;
}

void GetBatteryLevel(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(ChargeLevels::CompletelyFull); // Set to a completely full battery

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetBatteryChargeState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = battery_is_charging ? 1 : 0;

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void IsLegacyPowerOff(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Init() {
    AddService(new PTM_Play_Interface);
    AddService(new PTM_Sysm_Interface);
    AddService(new PTM_U_Interface);

    shell_open = true;
    battery_is_charging = true;

    // Open the SharedExtSaveData archive 0xF000000B and create the gamecoin.dat file if it doesn't exist
    FileSys::Path archive_path(ptm_shared_extdata_id);
    auto archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
    // If the archive didn't exist, create the files inside
    if (archive_result.Code().description == ErrorDescription::FS_NotFormatted) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
        // Open it again to get a valid archive now that the folder exists
        archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the PTM SharedExtSaveData archive!");

        FileSys::Path gamecoin_path("gamecoin.dat");
        FileSys::Mode open_mode = {};
        open_mode.write_flag = 1;
        open_mode.create_flag = 1;
        // Open the file and write the default gamecoin information
        auto gamecoin_result = Service::FS::OpenFileFromArchive(*archive_result, gamecoin_path, open_mode);
        if (gamecoin_result.Succeeded()) {
            auto gamecoin = gamecoin_result.MoveFrom();
            gamecoin->backend->Write(0, sizeof(GameCoin), 1, reinterpret_cast<const u8*>(&default_game_coin));
            gamecoin->backend->Close();
        }
    }
}

void Shutdown() {

}

} // namespace PTM
} // namespace Service
