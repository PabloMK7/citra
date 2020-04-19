// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/archives.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_gets.h"
#include "core/hle/service/ptm/ptm_play.h"
#include "core/hle/service/ptm/ptm_sets.h"
#include "core/hle/service/ptm/ptm_sysm.h"
#include "core/hle/service/ptm/ptm_u.h"
#include "core/settings.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::Module)

namespace Service::PTM {

/// Values for the default gamecoin.dat file
static const GameCoin default_game_coin = {0x4F00, 42, 0, 0, 0, 2014, 12, 29};

void Module::Interface::GetAdapterState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->battery_is_charging);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetShellState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x6, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->shell_open);
}

void Module::Interface::GetBatteryLevel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x7, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(ChargeLevels::CompletelyFull)); // Set to a completely full battery

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetBatteryChargeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x8, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->battery_is_charging);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetPedometerState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ptm->pedometer_is_counting);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetStepHistory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 3, 2);

    u32 hours = rp.Pop<u32>();
    u64 start_time = rp.Pop<u64>();
    auto& buffer = rp.PopMappedBuffer();
    ASSERT_MSG(sizeof(u16) * hours == buffer.GetSize(),
               "Buffer for steps count has incorrect size");

    // Stub: set zero steps count for every hour
    for (u32 i = 0; i < hours; ++i) {
        const u16_le steps_per_hour = 0;
        buffer.Write(&steps_per_hour, i * sizeof(u16), sizeof(u16));
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_PTM, "(STUBBED) called, from time(raw): 0x{:x}, for {} hours", start_time,
                hours);
}

void Module::Interface::GetTotalStepCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xC, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void Module::Interface::GetSoftwareClosedFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

void CheckNew3DS(IPC::RequestBuilder& rb) {
    const bool is_new_3ds = Settings::values.is_new_3ds;

    rb.Push(RESULT_SUCCESS);
    rb.Push(is_new_3ds);

    LOG_DEBUG(Service_PTM, "called isNew3DS = 0x{:08x}", static_cast<u32>(is_new_3ds));
}

void Module::Interface::CheckNew3DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x40A, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    Service::PTM::CheckNew3DS(rb);
}

static void WriteGameCoinData(GameCoin gamecoin_data) {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    FileSys::Path archive_path(ptm_shared_extdata_id);
    auto archive_result = extdata_archive_factory.Open(archive_path, 0);
    std::unique_ptr<FileSys::ArchiveBackend> archive;

    FileSys::Path gamecoin_path("/gamecoin.dat");
    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        // Format the archive to create the directories
        extdata_archive_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);
        // Open it again to get a valid archive now that the folder exists
        archive = extdata_archive_factory.Open(archive_path, 0).Unwrap();
        // Create the game coin file
        archive->CreateFile(gamecoin_path, sizeof(GameCoin));
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the PTM SharedExtSaveData archive!");
        archive = std::move(archive_result).Unwrap();
    }

    FileSys::Mode open_mode = {};
    open_mode.write_flag.Assign(1);
    // Open the file and write the default gamecoin information
    auto gamecoin_result = archive->OpenFile(gamecoin_path, open_mode);
    if (gamecoin_result.Succeeded()) {
        auto gamecoin = std::move(gamecoin_result).Unwrap();
        gamecoin->Write(0, sizeof(GameCoin), true, reinterpret_cast<const u8*>(&gamecoin_data));
        gamecoin->Close();
    }
}

static GameCoin ReadGameCoinData() {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    FileSys::Path archive_path(ptm_shared_extdata_id);
    auto archive_result = extdata_archive_factory.Open(archive_path, 0);
    if (!archive_result.Succeeded()) {
        LOG_ERROR(Service_PTM, "Could not open the PTM SharedExtSaveData archive!");
        return default_game_coin;
    }

    FileSys::Path gamecoin_path("/gamecoin.dat");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    auto gamecoin_result = (*archive_result)->OpenFile(gamecoin_path, open_mode);
    if (!gamecoin_result.Succeeded()) {
        LOG_ERROR(Service_PTM, "Could not open the game coin data file!");
        return default_game_coin;
    }

    auto gamecoin = std::move(gamecoin_result).Unwrap();
    GameCoin gamecoin_data;
    gamecoin->Read(0, sizeof(GameCoin), reinterpret_cast<u8*>(&gamecoin_data));
    gamecoin->Close();
    return gamecoin_data;
}

Module::Module() {
    // Open the SharedExtSaveData archive 0xF000000B and create the gamecoin.dat file if it doesn't
    // exist
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);
    const FileSys::Path archive_path(ptm_shared_extdata_id);
    const auto archive_result = extdata_archive_factory.Open(archive_path, 0);
    // If the archive didn't exist, write the default game coin file
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        WriteGameCoinData(default_game_coin);
    }
}

u16 Module::GetPlayCoins() {
    return ReadGameCoinData().total_coins;
}

void Module::SetPlayCoins(u16 play_coins) {
    GameCoin game_coin = ReadGameCoinData();
    game_coin.total_coins = play_coins;
    // TODO: This may introduce potential race condition if the game is reading the
    // game coin data at the same time
    WriteGameCoinData(game_coin);
}

Module::Interface::Interface(std::shared_ptr<Module> ptm, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), ptm(std::move(ptm)) {}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto ptm = std::make_shared<Module>();
    std::make_shared<PTM_Gets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Play>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sets>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_S>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_Sysm>(ptm)->InstallAsService(service_manager);
    std::make_shared<PTM_U>(ptm)->InstallAsService(service_manager);
}

} // namespace Service::PTM
