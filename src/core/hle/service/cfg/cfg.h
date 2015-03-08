// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hle/result.h"

namespace Service {
namespace CFG {

enum SystemModel {
    NINTENDO_3DS = 0,
    NINTENDO_3DS_XL = 1,
    NEW_NINTENDO_3DS = 2,
    NINTENDO_2DS = 3,
    NEW_NINTENDO_3DS_XL = 4
};

enum SystemLanguage {
    LANGUAGE_JP = 0,
    LANGUAGE_EN = 1,
    LANGUAGE_FR = 2,
    LANGUAGE_DE = 3,
    LANGUAGE_IT = 4,
    LANGUAGE_ES = 5,
    LANGUAGE_ZH = 6,
    LANGUAGE_KO = 7,
    LANGUAGE_NL = 8,
    LANGUAGE_PT = 9,
    LANGUAGE_RU = 10
};

/// Block header in the config savedata file
struct SaveConfigBlockEntry {
    u32 block_id;       ///< The id of the current block
    u32 offset_or_data; ///< This is the absolute offset to the block data if the size is greater than 4 bytes, otherwise it contains the data itself
    u16 size;           ///< The size of the block
    u16 flags;          ///< The flags of the block, possibly used for access control
};

/// The maximum number of block entries that can exist in the config file
static const u32 CONFIG_FILE_MAX_BLOCK_ENTRIES = 1479;

/**
* The header of the config savedata file,
* contains information about the blocks in the file
*/
struct SaveFileConfig {
    u16 total_entries;                        ///< The total number of set entries in the config file
    u16 data_entries_offset;                  ///< The offset where the data for the blocks start, this is hardcoded to 0x455C as per hardware
    SaveConfigBlockEntry block_entries[CONFIG_FILE_MAX_BLOCK_ENTRIES]; ///< The block headers, the maximum possible value is 1479 as per hardware
    u32 unknown;                              ///< This field is unknown, possibly padding, 0 has been observed in hardware
};
static_assert(sizeof(SaveFileConfig) == 0x455C, "The SaveFileConfig header must be exactly 0x455C bytes");

struct UsernameBlock {
    char16_t username[10]; ///< Exactly 20 bytes long, padded with zeros at the end if necessary
    u32 zero;
    u32 ng_word;
};
static_assert(sizeof(UsernameBlock) == 0x1C, "Size of UsernameBlock must be 0x1C");

struct ConsoleModelInfo {
    u8 model;       ///< The console model (3DS, 2DS, etc)
    u8 unknown[3];  ///< Unknown data
};
static_assert(sizeof(ConsoleModelInfo) == 4, "ConsoleModelInfo must be exactly 4 bytes");

struct ConsoleCountryInfo {
    u8 unknown[3];   ///< Unknown data
    u8 country_code; ///< The country code of the console
};
static_assert(sizeof(ConsoleCountryInfo) == 4, "ConsoleCountryInfo must be exactly 4 bytes");

extern const u64 CFG_SAVE_ID;
extern const u64 CONSOLE_UNIQUE_ID;
extern const ConsoleModelInfo CONSOLE_MODEL;
extern const u8 CONSOLE_LANGUAGE;
extern const char CONSOLE_USERNAME[0x14];
/// This will be initialized in the Interface constructor, and will be used when creating the block
extern UsernameBlock CONSOLE_USERNAME_BLOCK;
/// TODO(Subv): Find out what this actually is
extern const u8 SOUND_OUTPUT_MODE;
extern const u8 UNITED_STATES_COUNTRY_ID;
/// TODO(Subv): Find what the other bytes are
extern const ConsoleCountryInfo COUNTRY_INFO;
extern const std::array<float, 8> STEREO_CAMERA_SETTINGS;

static_assert(sizeof(STEREO_CAMERA_SETTINGS) == 0x20, "STEREO_CAMERA_SETTINGS must be exactly 0x20 bytes");
static_assert(sizeof(CONSOLE_UNIQUE_ID) == 8, "CONSOLE_UNIQUE_ID must be exactly 8 bytes");
static_assert(sizeof(CONSOLE_LANGUAGE) == 1, "CONSOLE_LANGUAGE must be exactly 1 byte");
static_assert(sizeof(SOUND_OUTPUT_MODE) == 1, "SOUND_OUTPUT_MODE must be exactly 1 byte");

/**
 * Reads a block with the specified id and flag from the Config savegame buffer
 * and writes the output to output.
 * The input size must match exactly the size of the requested block
 * @param block_id The id of the block we want to read
 * @param size The size of the block we want to read
 * @param flag The requested block must have this flag set
 * @param output A pointer where we will write the read data
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, u8* output);

/**
 * Creates a block with the specified id and writes the input data to the cfg savegame buffer in memory.
 * The config savegame file in the filesystem is not updated.
 * @param block_id The id of the block we want to create
 * @param size The size of the block we want to create
 * @param flags The flags of the new block
 * @param data A pointer containing the data we will write to the new block
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode CreateConfigInfoBlk(u32 block_id, u16 size, u16 flags, const u8* data);

/**
 * Deletes the config savegame file from the filesystem, the buffer in memory is not affected
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode DeleteConfigNANDSaveFile();

/**
 * Writes the config savegame memory buffer to the config savegame file in the filesystem
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode UpdateConfigNANDSavegame();

/**
 * Re-creates the config savegame file in memory and the filesystem with the default blocks
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode FormatConfig();

/// Initialize the config service
void Init();

/// Shutdown the config service
void Shutdown();

} // namespace CFG
} // namespace Service
