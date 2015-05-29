// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/file_util.h"

#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/settings.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_i.h"
#include "core/hle/service/cfg/cfg_s.h"
#include "core/hle/service/cfg/cfg_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

const u64 CFG_SAVE_ID = 0x00010017;
const u64 CONSOLE_UNIQUE_ID = 0xDEADC0DE;
const ConsoleModelInfo CONSOLE_MODEL = { NINTENDO_3DS_XL, { 0, 0, 0 } };
const u8 CONSOLE_LANGUAGE = LANGUAGE_EN;
const char CONSOLE_USERNAME[0x14] = "CITRA";
/// This will be initialized in Init, and will be used when creating the block
UsernameBlock CONSOLE_USERNAME_BLOCK;
/// TODO(Subv): Find out what this actually is
const u8 SOUND_OUTPUT_MODE = 2;
const u8 UNITED_STATES_COUNTRY_ID = 49;
/// TODO(Subv): Find what the other bytes are
const ConsoleCountryInfo COUNTRY_INFO = { { 0, 0, 0 }, UNITED_STATES_COUNTRY_ID };

/**
 * TODO(Subv): Find out what this actually is, these values fix some NaN uniforms in some games,
 * for example Nintendo Zone
 * Thanks Normmatt for providing this information
 */
const std::array<float, 8> STEREO_CAMERA_SETTINGS = {
    62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
    10.0f, 5.0f, 55.58000183105469f, 21.56999969482422f
};

static const u32 CONFIG_SAVEFILE_SIZE = 0x8000;
static std::array<u8, CONFIG_SAVEFILE_SIZE> cfg_config_file_buffer;

static Service::FS::ArchiveHandle cfg_system_save_data_archive;
static const std::vector<u8> cfg_system_savedata_id = { 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00 };

void GetCountryCodeString(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 country_code_id = cmd_buff[1];

    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        LOG_ERROR(Service_CFG, "requested country code id=%d is invalid", country_code_id);
        cmd_buff[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        return;
    }

    cmd_buff[1] = 0;
    cmd_buff[2] = country_codes[country_code_id];
}

void GetCountryCodeID(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u16 country_code = cmd_buff[1];
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    DEBUG_ASSERT(country_codes[0] == 0);

    for (u16 id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    if (0 == country_code_id) {
        LOG_ERROR(Service_CFG, "requested country code name=%c%c is invalid", country_code & 0xff, country_code >> 8);
        cmd_buff[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        cmd_buff[2] = 0xFFFF;
        return;
    }

    cmd_buff[1] = 0;
    cmd_buff[2] = country_code_id;
}

void SecureInfoGetRegion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = Settings::values.region_value;
}

void GenHashConsoleUnique(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 app_id_salt = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0x33646D6F ^ (app_id_salt & 0xFFFFF); // 3dmoo hash
    cmd_buff[3] = 0x6F534841 ^ (app_id_salt & 0xFFFFF);

    LOG_WARNING(Service_CFG, "(STUBBED) called app_id_salt=0x%X", app_id_salt);
}

void GetRegionCanadaUSA(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    u8 canada_or_usa = 1;
    if (canada_or_usa == Settings::values.region_value) {
        cmd_buff[2] = 1;
    } else {
        cmd_buff[2] = 0;
    }
}

void GetSystemModel(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buff[1] = Service::CFG::GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                                   reinterpret_cast<u8*>(&data)).raw;
    cmd_buff[2] = data & 0xFF;
}

void GetModelNintendo2DS(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buff[1] = Service::CFG::GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                                   reinterpret_cast<u8*>(&data)).raw;

    u8 model = data & 0xFF;
    if (model == Service::CFG::NINTENDO_2DS)
        cmd_buff[2] = 0;
    else
        cmd_buff[2] = 1;
}

void GetConfigInfoBlk2(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 size = cmd_buff[1];
    u32 block_id = cmd_buff[2];
    u8* data_pointer = Memory::GetPointer(cmd_buff[4]);

    if (data_pointer == nullptr) {
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buff[1] = Service::CFG::GetConfigInfoBlock(block_id, size, 0x2, data_pointer).raw;
}

void GetConfigInfoBlk8(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 size = cmd_buff[1];
    u32 block_id = cmd_buff[2];
    u8* data_pointer = Memory::GetPointer(cmd_buff[4]);

    if (data_pointer == nullptr) {
        cmd_buff[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buff[1] = Service::CFG::GetConfigInfoBlock(block_id, size, 0x8, data_pointer).raw;
}

void UpdateConfigNANDSavegame(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = Service::CFG::UpdateConfigNANDSavegame().raw;
}

void FormatConfig(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = Service::CFG::FormatConfig().raw;
}

ResultCode GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, u8* output) {
    // Read the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());

    auto itr = std::find_if(std::begin(config->block_entries), std::end(config->block_entries),
        [&](const SaveConfigBlockEntry& entry) {
            return entry.block_id == block_id && (entry.flags & flag);
        });

    if (itr == std::end(config->block_entries)) {
        LOG_ERROR(Service_CFG, "Config block 0x%X with flags %u and size %u was not found", block_id, flag, size);
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if (itr->size != size) {
        LOG_ERROR(Service_CFG, "Invalid size %u for config block 0x%X with flags %u", size, block_id, flag);
        return ResultCode(ErrorDescription::InvalidSize, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    // The data is located in the block header itself if the size is less than 4 bytes
    if (itr->size <= 4)
        memcpy(output, &itr->offset_or_data, itr->size);
    else
        memcpy(output, &cfg_config_file_buffer[itr->offset_or_data], itr->size);

    return RESULT_SUCCESS;
}

ResultCode CreateConfigInfoBlk(u32 block_id, u16 size, u16 flags, const u8* data) {
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    if (config->total_entries >= CONFIG_FILE_MAX_BLOCK_ENTRIES)
        return ResultCode(-1); // TODO(Subv): Find the right error code

    // Insert the block header with offset 0 for now
    config->block_entries[config->total_entries] = { block_id, 0, size, flags };
    if (size > 4) {
        u32 offset = config->data_entries_offset;
        // Perform a search to locate the next offset for the new data
        // use the offset and size of the previous block to determine the new position
        for (int i = config->total_entries - 1; i >= 0; --i) {
            // Ignore the blocks that don't have a separate data offset
            if (config->block_entries[i].size > 4) {
                offset = config->block_entries[i].offset_or_data +
                         config->block_entries[i].size;
                break;
            }
        }

        config->block_entries[config->total_entries].offset_or_data = offset;

        // Write the data at the new offset
        memcpy(&cfg_config_file_buffer[offset], data, size);
    }
    else {
        // The offset_or_data field in the header contains the data itself if it's 4 bytes or less
        memcpy(&config->block_entries[config->total_entries].offset_or_data, data, size);
    }

    ++config->total_entries;
    return RESULT_SUCCESS;
}

ResultCode DeleteConfigNANDSaveFile() {
    FileSys::Path path("config");
    return Service::FS::DeleteFileFromArchive(cfg_system_save_data_archive, path);
}

ResultCode UpdateConfigNANDSavegame() {
    FileSys::Mode mode = {};
    mode.write_flag = 1;
    mode.create_flag = 1;

    FileSys::Path path("config");

    auto config_result = Service::FS::OpenFileFromArchive(cfg_system_save_data_archive, path, mode);
    ASSERT_MSG(config_result.Succeeded(), "could not open file");

    auto config = config_result.MoveFrom();
    config->backend->Write(0, CONFIG_SAVEFILE_SIZE, 1, cfg_config_file_buffer.data());

    return RESULT_SUCCESS;
}

ResultCode FormatConfig() {
    ResultCode res = DeleteConfigNANDSaveFile();
    if (!res.IsSuccess())
        return res;
    // Delete the old data
    cfg_config_file_buffer.fill(0);
    // Create the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    // This value is hardcoded, taken from 3dbrew, verified by hardware, it's always the same value
    config->data_entries_offset = 0x455C;
    // Insert the default blocks
    res = CreateConfigInfoBlk(0x00050005, sizeof(STEREO_CAMERA_SETTINGS), 0xE,
                              reinterpret_cast<const u8*>(STEREO_CAMERA_SETTINGS.data()));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x00090001, sizeof(CONSOLE_UNIQUE_ID), 0xE,
                              reinterpret_cast<const u8*>(&CONSOLE_UNIQUE_ID));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000F0004, sizeof(CONSOLE_MODEL), 0x8,
                              reinterpret_cast<const u8*>(&CONSOLE_MODEL));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000A0002, sizeof(CONSOLE_LANGUAGE), 0xA, &CONSOLE_LANGUAGE);
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x00070001, sizeof(SOUND_OUTPUT_MODE), 0xE, &SOUND_OUTPUT_MODE);
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000B0000, sizeof(COUNTRY_INFO), 0xE,
                              reinterpret_cast<const u8*>(&COUNTRY_INFO));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000A0000, sizeof(CONSOLE_USERNAME_BLOCK), 0xE,
                              reinterpret_cast<const u8*>(&CONSOLE_USERNAME_BLOCK));
    if (!res.IsSuccess())
        return res;
    // Save the buffer to the file
    res = UpdateConfigNANDSavegame();
    if (!res.IsSuccess())
        return res;
    return RESULT_SUCCESS;
}

void Init() {
    AddService(new CFG_I_Interface);
    AddService(new CFG_S_Interface);
    AddService(new CFG_U_Interface);

    // Open the SystemSaveData archive 0x00010017
    FileSys::Path archive_path(cfg_system_savedata_id);
    auto archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code().description == ErrorDescription::FS_NotFormatted) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);

        // Open it again to get a valid archive now that the folder exists
        archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);
    }

    ASSERT_MSG(archive_result.Succeeded(), "Could not open the CFG SystemSaveData archive!");

    cfg_system_save_data_archive = *archive_result;

    FileSys::Path config_path("config");
    FileSys::Mode open_mode = {};
    open_mode.read_flag = 1;

    auto config_result = Service::FS::OpenFileFromArchive(*archive_result, config_path, open_mode);

    // Read the file if it already exists
    if (config_result.Succeeded()) {
        auto config = config_result.MoveFrom();
        config->backend->Read(0, CONFIG_SAVEFILE_SIZE, cfg_config_file_buffer.data());
        return;
    }

    // Initialize the Username block
    // TODO(Subv): Initialize this directly in the variable when MSVC supports char16_t string literals
    memset(&CONSOLE_USERNAME_BLOCK, 0, sizeof(CONSOLE_USERNAME_BLOCK));
    CONSOLE_USERNAME_BLOCK.ng_word = 0;
    CONSOLE_USERNAME_BLOCK.zero = 0;

    // Copy string to buffer and pad with zeros at the end
    auto size = Common::UTF8ToUTF16(CONSOLE_USERNAME).copy(CONSOLE_USERNAME_BLOCK.username, 0x14);
    std::fill(std::begin(CONSOLE_USERNAME_BLOCK.username) + size,
              std::end(CONSOLE_USERNAME_BLOCK.username), 0);

    FormatConfig();
}

void Shutdown() {
}

} // namespace CFG
} // namespace Service
