// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hle/result.h"
#include "core/hle/service/service.h"

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

// TODO(Link Mauve): use a constexpr once MSVC starts supporting it.
#define C(code) ((code)[0] | ((code)[1] << 8))

static const std::array<u16, 187> country_codes = {
    0,       C("JP"), 0,       0,       0,       0,       0,       0,       // 0-7
    C("AI"), C("AG"), C("AR"), C("AW"), C("BS"), C("BB"), C("BZ"), C("BO"), // 8-15
    C("BR"), C("VG"), C("CA"), C("KY"), C("CL"), C("CO"), C("CR"), C("DM"), // 16-23
    C("DO"), C("EC"), C("SV"), C("GF"), C("GD"), C("GP"), C("GT"), C("GY"), // 24-31
    C("HT"), C("HN"), C("JM"), C("MQ"), C("MX"), C("MS"), C("AN"), C("NI"), // 32-39
    C("PA"), C("PY"), C("PE"), C("KN"), C("LC"), C("VC"), C("SR"), C("TT"), // 40-47
    C("TC"), C("US"), C("UY"), C("VI"), C("VE"), 0,       0,       0,       // 48-55
    0,       0,       0,       0,       0,       0,       0,       0,       // 56-63
    C("AL"), C("AU"), C("AT"), C("BE"), C("BA"), C("BW"), C("BG"), C("HR"), // 64-71
    C("CY"), C("CZ"), C("DK"), C("EE"), C("FI"), C("FR"), C("DE"), C("GR"), // 72-79
    C("HU"), C("IS"), C("IE"), C("IT"), C("LV"), C("LS"), C("LI"), C("LT"), // 80-87
    C("LU"), C("MK"), C("MT"), C("ME"), C("MZ"), C("NA"), C("NL"), C("NZ"), // 88-95
    C("NO"), C("PL"), C("PT"), C("RO"), C("RU"), C("RS"), C("SK"), C("SI"), // 96-103
    C("ZA"), C("ES"), C("SZ"), C("SE"), C("CH"), C("TR"), C("GB"), C("ZM"), // 104-111
    C("ZW"), C("AZ"), C("MR"), C("ML"), C("NE"), C("TD"), C("SD"), C("ER"), // 112-119
    C("DJ"), C("SO"), C("AD"), C("GI"), C("GG"), C("IM"), C("JE"), C("MC"), // 120-127
    C("TW"), 0,       0,       0,       0,       0,       0,       0,       // 128-135
    C("KR"), 0,       0,       0,       0,       0,       0,       0,       // 136-143
    C("HK"), C("MO"), 0,       0,       0,       0,       0,       0,       // 144-151
    C("ID"), C("SG"), C("TH"), C("PH"), C("MY"), 0,       0,       0,       // 152-159
    C("CN"), 0,       0,       0,       0,       0,       0,       0,       // 160-167
    C("AE"), C("IN"), C("EG"), C("OM"), C("QA"), C("KW"), C("SA"), C("SY"), // 168-175
    C("BH"), C("JO"), 0,       0,       0,       0,       0,       0,       // 176-183
    C("SM"), C("VA"), C("BM")                                               // 184-186
};

#undef C

/**
 * CFG::GetCountryCodeString service function
 *  Inputs:
 *      1 : Country Code ID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country's 2-char string
 */
void GetCountryCodeString(Service::Interface* self);

/**
 * CFG::GetCountryCodeID service function
 *  Inputs:
 *      1 : Country Code 2-char string
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country Code ID
 */
void GetCountryCodeID(Service::Interface* self);

/**
 * CFG::GetConfigInfoBlk2 service function
 *  Inputs:
 *      0 : 0x00010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetConfigInfoBlk2(Service::Interface* self);

/**
 * CFG::SecureInfoGetRegion service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Region value loaded from SecureInfo offset 0x100
 */
void SecureInfoGetRegion(Service::Interface* self);

/**
 * CFG::GenHashConsoleUnique service function
 *  Inputs:
 *      1 : 20 bit application ID salt
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Hash/"ID" lower word
 *      3 : Hash/"ID" upper word
 */
void GenHashConsoleUnique(Service::Interface* self);

/**
 * CFG::GetRegionCanadaUSA service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Result Header code
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 1 if the system is a Canada or USA model, 0 otherwise
 */
void GetRegionCanadaUSA(Service::Interface* self);

/**
 * CFG::GetSystemModel service function
 *  Inputs:
 *      0 : 0x00050000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Model of the console
 */
void GetSystemModel(Service::Interface* self);

/**
 * CFG::GetModelNintendo2DS service function
 *  Inputs:
 *      0 : 0x00060000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 0 if the system is a Nintendo 2DS, 1 otherwise
 */
void GetModelNintendo2DS(Service::Interface* self);

/**
 * CFG::GetConfigInfoBlk2 service function
 *  Inputs:
 *      0 : 0x00010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetConfigInfoBlk2(Service::Interface* self);

/**
 * CFG::GetConfigInfoBlk8 service function
 *  Inputs:
 *      0 : 0x04010082 / 0x08010082
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetConfigInfoBlk8(Service::Interface* self);

/**
 * CFG::UpdateConfigNANDSavegame service function
 *  Inputs:
 *      0 : 0x04030000 / 0x08030000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void UpdateConfigNANDSavegame(Service::Interface* self);

/**
 * CFG::FormatConfig service function
 *  Inputs:
 *      0 : 0x08060000
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void FormatConfig(Service::Interface* self);

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
