// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/log.h"
#include "common/make_unique.h"
#include "common/string_util.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/hle/hle.h"
#include "core/hle/service/cfg_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CFG_U

namespace CFG_U {

enum SystemModel {
    NINTENDO_3DS        = 0,
    NINTENDO_3DS_XL     = 1,
    NEW_NINTENDO_3DS    = 2,
    NINTENDO_2DS        = 3,
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

struct UsernameBlock {
    char16_t username[10]; ///< Exactly 20 bytes long, padded with zeros at the end if necessary
    u32 zero;
    u32 ng_word;
};
static_assert(sizeof(UsernameBlock) == 0x1C, "Size of UsernameBlock must be 0x1C");

static std::unique_ptr<FileSys::Archive_SystemSaveData> cfg_system_save_data;
static const u64 CFG_SAVE_ID = 0x00010017;
static const u64 CONSOLE_UNIQUE_ID = 0xDEADC0DE;
static const u32 CONSOLE_MODEL = NINTENDO_3DS_XL;
static const u8 CONSOLE_LANGUAGE = LANGUAGE_EN;
static const char CONSOLE_USERNAME[0x14] = "CITRA";
/// This will be initialized in the Interface constructor, and will be used when creating the block
static UsernameBlock CONSOLE_USERNAME_BLOCK;
/// TODO(Subv): Find out what this actually is
static const u8 SOUND_OUTPUT_MODE = 2;
static const u32 CONFIG_SAVEFILE_SIZE = 0x8000;
static std::array<u8, CONFIG_SAVEFILE_SIZE> cfg_config_file_buffer = { };

/**
 * TODO(Subv): Find out what this actually is, these values fix some NaN uniforms in some games,
 * for example Nintendo Zone
 * Thanks Normmatt for providing this information
 */
static const std::array<float, 8> STEREO_CAMERA_SETTINGS = {
    62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
    10.0f, 5.0f, 55.58000183105469f, 21.56999969482422f
};

// TODO(Link Mauve): use a constexpr once MSVC starts supporting it.
#define C(code) ((code)[0] | ((code)[1] << 8))

static const u8 UNITED_STATES_COUNTRY_ID = 49;
/// TODO(Subv): Find what the other bytes are
static const std::array<u8, 4> COUNTRY_INFO = { 0, 0, 0, UNITED_STATES_COUNTRY_ID };

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
 * CFG_User::GetCountryCodeString service function
 *  Inputs:
 *      1 : Country Code ID
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country's 2-char string
 */
static void GetCountryCodeString(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 country_code_id = cmd_buffer[1];

    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        LOG_ERROR(Service_CFG, "requested country code id=%d is invalid", country_code_id);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        return;
    }

    cmd_buffer[1] = 0;
    cmd_buffer[2] = country_codes[country_code_id];
}

/**
 * CFG_User::GetCountryCodeID service function
 *  Inputs:
 *      1 : Country Code 2-char string
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Country Code ID
 */
static void GetCountryCodeID(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u16 country_code = cmd_buffer[1];
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    _dbg_assert_(Service_CFG, country_codes[0] == 0);

    for (size_t id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    if (0 == country_code_id) {
        LOG_ERROR(Service_CFG, "requested country code name=%c%c is invalid", country_code & 0xff, country_code >> 8);
        cmd_buffer[1] = ResultCode(ErrorDescription::NotFound, ErrorModule::Config, ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        cmd_buffer[2] = 0xFFFF;
        return;
    }

    cmd_buffer[1] = 0;
    cmd_buffer[2] = country_code_id;
}

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

/**
 * Reads a block with the specified id and flag from the Config savegame buffer
 * and writes the output to output.
 * The input size must match exactly the size of the requested block
 * TODO(Subv): This should actually be in some file common to the CFG process
 * @param block_id The id of the block we want to read
 * @param size The size of the block we want to read
 * @param flag The requested block must have this flag set
 * @param output A pointer where we will write the read data
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
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

/**
 * Creates a block with the specified id and writes the input data to the cfg savegame buffer in memory.
 * The config savegame file in the filesystem is not updated.
 * TODO(Subv): This should actually be in some file common to the CFG process
 * @param block_id The id of the block we want to create
 * @param size The size of the block we want to create
 * @param flag The flags of the new block
 * @param data A pointer containing the data we will write to the new block
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode CreateConfigInfoBlk(u32 block_id, u32 size, u32 flags, const u8* data) {
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    if (config->total_entries >= CONFIG_FILE_MAX_BLOCK_ENTRIES)
        return ResultCode(-1); // TODO(Subv): Find the right error code

    // Insert the block header with offset 0 for now
    config->block_entries[config->total_entries] = { block_id, 0, size, flags };
    if (size > 4) {
        s32 total_entries = config->total_entries - 1;
        u32 offset = config->data_entries_offset;
        // Perform a search to locate the next offset for the new data
        // use the offset and size of the previous block to determine the new position
        while (total_entries >= 0) {
            // Ignore the blocks that don't have a separate data offset
            if (config->block_entries[total_entries].size > 4) {
                offset = config->block_entries[total_entries].offset_or_data +
                    config->block_entries[total_entries].size;
                break;
            }

            --total_entries;
        }

        config->block_entries[config->total_entries].offset_or_data = offset;

        // Write the data at the new offset
        memcpy(&cfg_config_file_buffer[offset], data, size);
    } else {
        // The offset_or_data field in the header contains the data itself if it's 4 bytes or less
        memcpy(&config->block_entries[config->total_entries].offset_or_data, data, size);
    }

    ++config->total_entries;
    return RESULT_SUCCESS;
}

/**
 * Deletes the config savegame file from the filesystem, the buffer in memory is not affected
 * TODO(Subv): This should actually be in some file common to the CFG process
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode DeleteConfigNANDSaveFile() {
    FileSys::Path path("config");
    if (cfg_system_save_data->DeleteFile(path))
        return RESULT_SUCCESS;
    return ResultCode(-1); // TODO(Subv): Find the right error code
}

/**
 * Writes the config savegame memory buffer to the config savegame file in the filesystem
 * TODO(Subv): This should actually be in some file common to the CFG process
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
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

/**
 * Re-creates the config savegame file in memory and the filesystem with the default blocks
 * TODO(Subv): This should actually be in some file common to the CFG process
 * @returns ResultCode indicating the result of the operation, 0 on success
 */
ResultCode FormatConfig() {
    ResultCode res = DeleteConfigNANDSaveFile();
    if (!res.IsSuccess())
        return res;
    // Delete the old data
    std::fill(cfg_config_file_buffer.begin(), cfg_config_file_buffer.end(), 0);
    // Create the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    // This value is hardcoded, taken from 3dbrew, verified by hardware, it's always the same value
    config->data_entries_offset = 0x455C;
    // Insert the default blocks
    res = CreateConfigInfoBlk(0x00050005, 0x20, 0xE, 
                              reinterpret_cast<const u8*>(STEREO_CAMERA_SETTINGS.data()));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x00090001, 0x8, 0xE, reinterpret_cast<const u8*>(&CONSOLE_UNIQUE_ID));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000F0004, 0x4, 0x8, reinterpret_cast<const u8*>(&CONSOLE_MODEL));
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000A0002, 0x1, 0xA, &CONSOLE_LANGUAGE);
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x00070001, 0x1, 0xE, &SOUND_OUTPUT_MODE);
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000B0000, 0x4, 0xE, COUNTRY_INFO.data());
    if (!res.IsSuccess())
        return res;
    res = CreateConfigInfoBlk(0x000A0000, 0x1C, 0xE, reinterpret_cast<const u8*>(&CONSOLE_USERNAME_BLOCK));
    if (!res.IsSuccess())
        return res;
    // Save the buffer to the file
    res = UpdateConfigNANDSavegame();
    if (!res.IsSuccess())
        return res;
    return RESULT_SUCCESS;
}

/**
 * CFG_User::GetConfigInfoBlk2 service function
 *  Inputs:
 *      1 : Size
 *      2 : Block ID
 *      3 : Descriptor for the output buffer
 *      4 : Output buffer pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetConfigInfoBlk2(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 size = cmd_buffer[1];
    u32 block_id = cmd_buffer[2];
    u8* data_pointer = Memory::GetPointer(cmd_buffer[4]);
    
    if (data_pointer == nullptr) {
        cmd_buffer[1] = -1; // TODO(Subv): Find the right error code
        return;
    }

    cmd_buffer[1] = GetConfigInfoBlock(block_id, size, 0x2, data_pointer).raw;
}

/**
 * CFG_User::GetSystemModel service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Model of the console
 */
static void GetSystemModel(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buffer[1] = GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                       reinterpret_cast<u8*>(&data)).raw; 
    cmd_buffer[2] = data & 0xFF;
}

/**
 * CFG_User::GetModelNintendo2DS service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 0 if the system is a Nintendo 2DS, 1 otherwise
 */
static void GetModelNintendo2DS(Service::Interface* self) {
    u32* cmd_buffer = Kernel::GetCommandBuffer();
    u32 data;

    // TODO(Subv): Find out the correct error codes
    cmd_buffer[1] = GetConfigInfoBlock(0x000F0004, 4, 0x8,
                                       reinterpret_cast<u8*>(&data)).raw; 
    
    u8 model = data & 0xFF;
    if (model == NINTENDO_2DS)
        cmd_buffer[2] = 0;
    else
        cmd_buffer[2] = 1;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, GetConfigInfoBlk2,     "GetConfigInfoBlk2"},
    {0x00020000, nullptr,               "SecureInfoGetRegion"},
    {0x00030000, nullptr,               "GenHashConsoleUnique"},
    {0x00040000, nullptr,               "GetRegionCanadaUSA"},
    {0x00050000, GetSystemModel,        "GetSystemModel"},
    {0x00060000, GetModelNintendo2DS,   "GetModelNintendo2DS"},
    {0x00070040, nullptr,               "unknown"},
    {0x00080080, nullptr,               "unknown"},
    {0x00090040, GetCountryCodeString,  "GetCountryCodeString"},
    {0x000A0040, GetCountryCodeID,      "GetCountryCodeID"},
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
    // TODO(Subv): In the future we should use the FS service to query this archive, 
    // currently it is not possible because you can only have one open archive of the same type at any time
    std::string syssavedata_directory = FileUtil::GetUserPath(D_SYSSAVEDATA_IDX);
    cfg_system_save_data = Common::make_unique<FileSys::Archive_SystemSaveData>(syssavedata_directory, 
        CFG_SAVE_ID);
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

Interface::~Interface() {
}

} // namespace
