// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/log.h"
#include "common/make_unique.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/hle/service/cfg/cfg.h"

namespace Service {
namespace CFG {

const u64 CFG_SAVE_ID = 0x00010017;
const u64 CONSOLE_UNIQUE_ID = 0xDEADC0DE;
const ConsoleModelInfo CONSOLE_MODEL = { NINTENDO_3DS_XL, 0, 0, 0 };
const u8 CONSOLE_LANGUAGE = LANGUAGE_EN;
const char CONSOLE_USERNAME[0x14] = "CITRA";
/// This will be initialized in CFGInit, and will be used when creating the block
UsernameBlock CONSOLE_USERNAME_BLOCK;
/// TODO(Subv): Find out what this actually is
const u8 SOUND_OUTPUT_MODE = 2;
const u8 UNITED_STATES_COUNTRY_ID = 49;
/// TODO(Subv): Find what the other bytes are
const ConsoleCountryInfo COUNTRY_INFO = { 0, 0, 0, UNITED_STATES_COUNTRY_ID };

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

static std::unique_ptr<FileSys::Archive_SystemSaveData> cfg_system_save_data;

ResultCode GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, u8* output) {
    // Read the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());

    auto itr = std::find_if(std::begin(config->block_entries), std::end(config->block_entries),
            [&](const SaveConfigBlockEntry& entry) {
                return entry.block_id == block_id && entry.size == size && (entry.flags & flag);
            });

    if (itr == std::end(config->block_entries)) {
        LOG_ERROR(Service_CFG, "Config block %u with size %u and flags %u not found", block_id, size, flag);
        return ResultCode(-1); // TODO(Subv): Find the correct error code
    }

    // The data is located in the block header itself if the size is less than 4 bytes
    if (itr->size <= 4)
        memcpy(output, &itr->offset_or_data, itr->size);
    else
        memcpy(output, &cfg_config_file_buffer[itr->offset_or_data], itr->size);

    return RESULT_SUCCESS;
}

ResultCode CreateConfigInfoBlk(u32 block_id, u32 size, u32 flags, const u8* data) {
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
    if (cfg_system_save_data->DeleteFile(path))
        return RESULT_SUCCESS;
    return ResultCode(-1); // TODO(Subv): Find the right error code
}

ResultCode UpdateConfigNANDSavegame() {
    FileSys::Mode mode = {};
    mode.write_flag = 1;
    mode.create_flag = 1;
    FileSys::Path path("config");
    auto file = cfg_system_save_data->OpenFile(path, mode);
    _assert_msg_(Service_CFG, file != nullptr, "could not open file");
    file->Write(0, CONFIG_SAVEFILE_SIZE, 1, cfg_config_file_buffer.data());
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

void CFGInit() {
    // TODO(Subv): In the future we should use the FS service to query this archive, 
    // currently it is not possible because you can only have one open archive of the same type at any time
    std::string syssavedata_directory = FileUtil::GetUserPath(D_SYSSAVEDATA_IDX);
    cfg_system_save_data = Common::make_unique<FileSys::Archive_SystemSaveData>(
                           syssavedata_directory, CFG_SAVE_ID);
    if (!cfg_system_save_data->Initialize()) {
        LOG_CRITICAL(Service_CFG, "Could not initialize SystemSaveData archive for the CFG:U service");
        return;
    }

    // TODO(Subv): All this code should be moved to cfg:i, 
    // it's only here because we do not currently emulate the lower level code that uses that service
    // Try to open the file in read-only mode to check its existence
    FileSys::Mode mode = {};
    mode.read_flag = 1;
    FileSys::Path path("config");
    auto file = cfg_system_save_data->OpenFile(path, mode);

    // Load the config if it already exists
    if (file != nullptr) {
        file->Read(0, CONFIG_SAVEFILE_SIZE, cfg_config_file_buffer.data());
        return;
    }

    // Initialize the Username block
    // TODO(Subv): Initialize this directly in the variable when MSVC supports char16_t string literals
    CONSOLE_USERNAME_BLOCK.ng_word = 0;
    CONSOLE_USERNAME_BLOCK.zero = 0;
    // Copy string to buffer and pad with zeros at the end
    auto size = Common::UTF8ToUTF16(CONSOLE_USERNAME).copy(CONSOLE_USERNAME_BLOCK.username, 0x14);
    std::fill(std::begin(CONSOLE_USERNAME_BLOCK.username) + size,
              std::end(CONSOLE_USERNAME_BLOCK.username), 0);
    FormatConfig();
}

void CFGShutdown() {

}

} // namespace CFG
} // namespace Service
