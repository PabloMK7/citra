// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_gets.h"
#include "core/hle/service/ptm/ptm_play.h"
#include "core/hle/service/ptm/ptm_sets.h"
#include "core/hle/service/ptm/ptm_sysm.h"
#include "core/hle/service/ptm/ptm_u.h"
#include "core/hle/service/service.h"
#include "core/settings.h"

namespace Service {
namespace PTM {

/// Values for the default gamecoin.dat file
static const GameCoin default_game_coin = {0x4F00, 42, 0, 0, 0, 2014, 12, 29};

/// Id of the SharedExtData archive used by the PTM process
static const std::vector<u8> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

static bool shell_open;

static bool battery_is_charging;

static bool pedometer_is_counting;

void GetAdapterState(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x5, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(battery_is_charging);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetShellState(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x6, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(shell_open);
}

void GetBatteryLevel(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x7, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(ChargeLevels::CompletelyFull)); // Set to a completely full battery

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetBatteryChargeState(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x8, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(battery_is_charging);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetPedometerState(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x9, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(pedometer_is_counting);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetTotalStepCount(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xC, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void GetSoftwareClosedFlag(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x80F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void CheckNew3DS(IPC::RequestBuilder& rb) {
    const bool is_new_3ds = Settings::values.is_new_3ds;

    if (is_new_3ds) {
        LOG_CRITICAL(Service_PTM, "The option 'is_new_3ds' is enabled as part of the 'System' "
                                  "settings. Citra does not fully support New 3DS emulation yet!");
    }

    rb.Push(RESULT_SUCCESS);
    rb.Push(is_new_3ds);

    LOG_WARNING(Service_PTM, "(STUBBED) called isNew3DS = 0x%08x", static_cast<u32>(is_new_3ds));
}

void CheckNew3DS(Service::Interface* self) {
    IPC::RequestBuilder rb(Kernel::GetCommandBuffer(), 0x40A, 0, 0); // 0x040A0000
    CheckNew3DS(rb);
}

void Init() {
    AddService(new PTM_Gets);
    AddService(new PTM_Play);
    AddService(new PTM_S);
    AddService(new PTM_Sets);
    AddService(new PTM_Sysm);
    AddService(new PTM_U);

    shell_open = true;
    battery_is_charging = true;
    pedometer_is_counting = false;

    // Open the SharedExtSaveData archive 0xF000000B and create the gamecoin.dat file if it doesn't
    // exist
    FileSys::Path archive_path(ptm_shared_extdata_id);
    auto archive_result =
        Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
    // If the archive didn't exist, create the files inside
    if (archive_result.Code().description == static_cast<u32>(ErrorDescription::FS_NotFormatted)) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SharedExtSaveData,
                                   FileSys::ArchiveFormatInfo(), archive_path);
        // Open it again to get a valid archive now that the folder exists
        archive_result =
            Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SharedExtSaveData, archive_path);
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the PTM SharedExtSaveData archive!");

        FileSys::Path gamecoin_path("/gamecoin.dat");
        Service::FS::CreateFileInArchive(*archive_result, gamecoin_path, sizeof(GameCoin));
        FileSys::Mode open_mode = {};
        open_mode.write_flag.Assign(1);
        // Open the file and write the default gamecoin information
        auto gamecoin_result =
            Service::FS::OpenFileFromArchive(*archive_result, gamecoin_path, open_mode);
        if (gamecoin_result.Succeeded()) {
            auto gamecoin = gamecoin_result.MoveFrom();
            gamecoin->backend->Write(0, sizeof(GameCoin), true,
                                     reinterpret_cast<const u8*>(&default_game_coin));
            gamecoin->backend->Close();
        }
    }
}

void Shutdown() {}

} // namespace PTM
} // namespace Service
