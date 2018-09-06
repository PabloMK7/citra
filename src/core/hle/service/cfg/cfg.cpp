// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_i.h"
#include "core/hle/service/cfg/cfg_nor.h"
#include "core/hle/service/cfg/cfg_s.h"
#include "core/hle/service/cfg/cfg_u.h"
#include "core/settings.h"

namespace Service {
namespace CFG {

/// The maximum number of block entries that can exist in the config file
static const u32 CONFIG_FILE_MAX_BLOCK_ENTRIES = 1479;

namespace {

/**
 * The header of the config savedata file,
 * contains information about the blocks in the file
 */
struct SaveFileConfig {
    u16 total_entries;       ///< The total number of set entries in the config file
    u16 data_entries_offset; ///< The offset where the data for the blocks start, this is hardcoded
                             /// to 0x455C as per hardware
    SaveConfigBlockEntry block_entries[CONFIG_FILE_MAX_BLOCK_ENTRIES]; ///< The block headers, the
                                                                       /// maximum possible value is
    /// 1479 as per hardware
    u32 unknown; ///< This field is unknown, possibly padding, 0 has been observed in hardware
};
static_assert(sizeof(SaveFileConfig) == 0x455C,
              "SaveFileConfig header must be exactly 0x455C bytes");

enum ConfigBlockID {
    StereoCameraSettingsBlockID = 0x00050005,
    SoundOutputModeBlockID = 0x00070001,
    ConsoleUniqueID1BlockID = 0x00090000,
    ConsoleUniqueID2BlockID = 0x00090001,
    ConsoleUniqueID3BlockID = 0x00090002,
    UsernameBlockID = 0x000A0000,
    BirthdayBlockID = 0x000A0001,
    LanguageBlockID = 0x000A0002,
    CountryInfoBlockID = 0x000B0000,
    CountryNameBlockID = 0x000B0001,
    StateNameBlockID = 0x000B0002,
    EULAVersionBlockID = 0x000D0000,
    ConsoleModelBlockID = 0x000F0004,
};

struct UsernameBlock {
    char16_t username[10]; ///< Exactly 20 bytes long, padded with zeros at the end if necessary
    u32 zero;
    u32 ng_word;
};
static_assert(sizeof(UsernameBlock) == 0x1C, "UsernameBlock must be exactly 0x1C bytes");

struct BirthdayBlock {
    u8 month; ///< The month of the birthday
    u8 day;   ///< The day of the birthday
};
static_assert(sizeof(BirthdayBlock) == 2, "BirthdayBlock must be exactly 2 bytes");

struct ConsoleModelInfo {
    u8 model;      ///< The console model (3DS, 2DS, etc)
    u8 unknown[3]; ///< Unknown data
};
static_assert(sizeof(ConsoleModelInfo) == 4, "ConsoleModelInfo must be exactly 4 bytes");

struct ConsoleCountryInfo {
    u8 unknown[3];   ///< Unknown data
    u8 country_code; ///< The country code of the console
};
static_assert(sizeof(ConsoleCountryInfo) == 4, "ConsoleCountryInfo must be exactly 4 bytes");
} // namespace

static const ConsoleModelInfo CONSOLE_MODEL = {NINTENDO_3DS_XL, {0, 0, 0}};
static const u8 CONSOLE_LANGUAGE = LANGUAGE_EN;
static const UsernameBlock CONSOLE_USERNAME_BLOCK = {u"CITRA", 0, 0};
static const BirthdayBlock PROFILE_BIRTHDAY = {3, 25}; // March 25th, 2014
static const u8 SOUND_OUTPUT_MODE = SOUND_SURROUND;
static const u8 UNITED_STATES_COUNTRY_ID = 49;
/// TODO(Subv): Find what the other bytes are
static const ConsoleCountryInfo COUNTRY_INFO = {{0, 0, 0}, UNITED_STATES_COUNTRY_ID};

/**
 * TODO(Subv): Find out what this actually is, these values fix some NaN uniforms in some games,
 * for example Nintendo Zone
 * Thanks Normmatt for providing this information
 */
static const std::array<float, 8> STEREO_CAMERA_SETTINGS = {
    62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
    10.0f, 5.0f,   55.58000183105469f, 21.56999969482422f,
};
static_assert(sizeof(STEREO_CAMERA_SETTINGS) == 0x20,
              "STEREO_CAMERA_SETTINGS must be exactly 0x20 bytes");

static const std::vector<u8> cfg_system_savedata_id = {
    0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00,
};

static std::weak_ptr<Module> current_cfg;

std::shared_ptr<Module> GetCurrentModule() {
    auto cfg = current_cfg.lock();
    ASSERT_MSG(cfg, "No CFG module running!");
    return cfg;
}

Module::Interface::Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cfg(std::move(cfg)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetCountryCodeString(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 1, 0);
    u16 country_code_id = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (country_code_id >= country_codes.size() || 0 == country_codes[country_code_id]) {
        LOG_ERROR(Service_CFG, "requested country code id={} is invalid", country_code_id);
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::Config,
                           ErrorSummary::WrongArgument, ErrorLevel::Permanent));
        rb.Skip(1, false);
        return;
    }

    rb.Push(RESULT_SUCCESS);
    // the real CFG service copies only three bytes (including the null-terminator) here
    rb.Push<u32>(country_codes[country_code_id]);
}

void Module::Interface::GetCountryCodeID(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0A, 1, 0);
    u16 country_code = rp.Pop<u16>();
    u16 country_code_id = 0;

    // The following algorithm will fail if the first country code isn't 0.
    DEBUG_ASSERT(country_codes[0] == 0);

    for (u16 id = 0; id < country_codes.size(); ++id) {
        if (country_codes[id] == country_code) {
            country_code_id = id;
            break;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (0 == country_code_id) {
        LOG_ERROR(Service_CFG, "requested country code name={}{} is invalid",
                  static_cast<char>(country_code & 0xff), static_cast<char>(country_code >> 8));
        rb.Push(ResultCode(ErrorDescription::NotFound, ErrorModule::Config,
                           ErrorSummary::WrongArgument, ErrorLevel::Permanent));
        rb.Push<u16>(0x00FF);
        return;
    }

    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(country_code_id);
}

u32 Module::GetRegionValue() {
    if (Settings::values.region_value == Settings::REGION_VALUE_AUTO_SELECT)
        return preferred_region_code;

    return Settings::values.region_value;
}

void Module::Interface::SecureInfoGetRegion(Kernel::HLERequestContext& ctx, u16 id) {
    IPC::RequestParser rp(ctx, id, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(static_cast<u8>(cfg->GetRegionValue()));
}

void Module::Interface::GenHashConsoleUnique(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 1, 0);
    const u32 app_id_salt = rp.Pop<u32>() & 0x000FFFFF;

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);

    std::array<u8, 12> buffer;
    const ResultCode result = cfg->GetConfigInfoBlock(ConsoleUniqueID2BlockID, 8, 2, buffer.data());
    rb.Push(result);
    if (result.IsSuccess()) {
        std::memcpy(&buffer[8], &app_id_salt, sizeof(u32));
        std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
        CryptoPP::SHA256().CalculateDigest(hash.data(), buffer.data(), sizeof(buffer));
        u32 low, high;
        memcpy(&low, &hash[hash.size() - 8], sizeof(u32));
        memcpy(&high, &hash[hash.size() - 4], sizeof(u32));
        rb.Push(low);
        rb.Push(high);
    } else {
        rb.Push<u32>(0);
        rb.Push<u32>(0);
    }

    LOG_DEBUG(Service_CFG, "called app_id_salt=0x{:X}", app_id_salt);
}

void Module::Interface::GetRegionCanadaUSA(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);

    u8 canada_or_usa = 1;
    if (canada_or_usa == cfg->GetRegionValue()) {
        rb.Push(true);
    } else {
        rb.Push(false);
    }
}

void Module::Interface::GetSystemModel(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data;

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigInfoBlock(ConsoleModelBlockID, 4, 0x8, reinterpret_cast<u8*>(&data)));
    rb.Push<u8>(data & 0xFF);
}

void Module::Interface::GetModelNintendo2DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data;

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigInfoBlock(ConsoleModelBlockID, 4, 0x8, reinterpret_cast<u8*>(&data)));
    u8 model = data & 0xFF;
    rb.Push(model != Service::CFG::NINTENDO_2DS);
}

void Module::Interface::GetConfigInfoBlk2(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 2, 2);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigInfoBlock(block_id, size, 0x2, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::GetConfigInfoBlk8(Kernel::HLERequestContext& ctx, u16 id) {
    IPC::RequestParser rp(ctx, id, 2, 2);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigInfoBlock(block_id, size, 0x8, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::SetConfigInfoBlk4(Kernel::HLERequestContext& ctx, u16 id) {
    IPC::RequestParser rp(ctx, id, 2, 2);
    u32 block_id = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    std::vector<u8> data(size);
    buffer.Read(data.data(), 0, data.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(cfg->SetConfigInfoBlock(block_id, size, 0x4, data.data()));
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx, u16 id) {
    IPC::RequestParser rp(ctx, id, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->UpdateConfigNANDSavegame());
}

void Module::Interface::FormatConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0806, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->FormatConfig());
}

ResultVal<void*> Module::GetConfigInfoBlockPointer(u32 block_id, u32 size, u32 flag) {
    // Read the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());

    auto itr =
        std::find_if(std::begin(config->block_entries), std::end(config->block_entries),
                     [&](const SaveConfigBlockEntry& entry) { return entry.block_id == block_id; });

    if (itr == std::end(config->block_entries)) {
        LOG_ERROR(Service_CFG, "Config block 0x{:X} with flags {} and size {} was not found",
                  block_id, flag, size);
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if ((itr->flags & flag) == 0) {
        LOG_ERROR(Service_CFG, "Invalid flag {} for config block 0x{:X} with size {}", flag,
                  block_id, size);
        return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if (itr->size != size) {
        LOG_ERROR(Service_CFG, "Invalid size {} for config block 0x{:X} with flags {}", size,
                  block_id, flag);
        return ResultCode(ErrorDescription::InvalidSize, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    void* pointer;

    // The data is located in the block header itself if the size is less than 4 bytes
    if (itr->size <= 4)
        pointer = &itr->offset_or_data;
    else
        pointer = &cfg_config_file_buffer[itr->offset_or_data];

    return MakeResult<void*>(pointer);
}

ResultCode Module::GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, void* output) {
    void* pointer;
    CASCADE_RESULT(pointer, GetConfigInfoBlockPointer(block_id, size, flag));
    memcpy(output, pointer, size);

    return RESULT_SUCCESS;
}

ResultCode Module::SetConfigInfoBlock(u32 block_id, u32 size, u32 flag, const void* input) {
    void* pointer;
    CASCADE_RESULT(pointer, GetConfigInfoBlockPointer(block_id, size, flag));
    memcpy(pointer, input, size);
    return RESULT_SUCCESS;
}

ResultCode Module::CreateConfigInfoBlk(u32 block_id, u16 size, u16 flags, const void* data) {
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    if (config->total_entries >= CONFIG_FILE_MAX_BLOCK_ENTRIES)
        return ResultCode(-1); // TODO(Subv): Find the right error code

    // Insert the block header with offset 0 for now
    config->block_entries[config->total_entries] = {block_id, 0, size, flags};
    if (size > 4) {
        u32 offset = config->data_entries_offset;
        // Perform a search to locate the next offset for the new data
        // use the offset and size of the previous block to determine the new position
        for (int i = config->total_entries - 1; i >= 0; --i) {
            // Ignore the blocks that don't have a separate data offset
            if (config->block_entries[i].size > 4) {
                offset = config->block_entries[i].offset_or_data + config->block_entries[i].size;
                break;
            }
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

ResultCode Module::DeleteConfigNANDSaveFile() {
    FileSys::Path path("/config");
    return Service::FS::DeleteFileFromArchive(cfg_system_save_data_archive, path);
}

ResultCode Module::UpdateConfigNANDSavegame() {
    FileSys::Mode mode = {};
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    FileSys::Path path("/config");

    auto config_result = Service::FS::OpenFileFromArchive(cfg_system_save_data_archive, path, mode);
    ASSERT_MSG(config_result.Succeeded(), "could not open file");

    auto config = std::move(config_result).Unwrap();
    config->backend->Write(0, CONFIG_SAVEFILE_SIZE, 1, cfg_config_file_buffer.data());

    return RESULT_SUCCESS;
}

ResultCode Module::FormatConfig() {
    ResultCode res = DeleteConfigNANDSaveFile();
    // The delete command fails if the file doesn't exist, so we have to check that too
    if (!res.IsSuccess() && res != FileSys::ERROR_FILE_NOT_FOUND) {
        return res;
    }
    // Delete the old data
    cfg_config_file_buffer.fill(0);
    // Create the header
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    // This value is hardcoded, taken from 3dbrew, verified by hardware, it's always the same value
    config->data_entries_offset = 0x455C;

    // Insert the default blocks
    u8 zero_buffer[0xC0] = {};

    // 0x00030001 - Unknown
    res = CreateConfigInfoBlk(0x00030001, 0x8, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(StereoCameraSettingsBlockID, sizeof(STEREO_CAMERA_SETTINGS), 0xE,
                              STEREO_CAMERA_SETTINGS.data());
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(SoundOutputModeBlockID, sizeof(SOUND_OUTPUT_MODE), 0xE,
                              &SOUND_OUTPUT_MODE);
    if (!res.IsSuccess())
        return res;

    u32 random_number;
    u64 console_id;
    GenerateConsoleUniqueId(random_number, console_id);

    u64_le console_id_le = console_id;
    res = CreateConfigInfoBlk(ConsoleUniqueID1BlockID, sizeof(console_id_le), 0xE, &console_id_le);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(ConsoleUniqueID2BlockID, sizeof(console_id_le), 0xE, &console_id_le);
    if (!res.IsSuccess())
        return res;

    u32_le random_number_le = random_number;
    res = CreateConfigInfoBlk(ConsoleUniqueID3BlockID, sizeof(random_number_le), 0xE,
                              &random_number_le);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(UsernameBlockID, sizeof(CONSOLE_USERNAME_BLOCK), 0xE,
                              &CONSOLE_USERNAME_BLOCK);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(BirthdayBlockID, sizeof(PROFILE_BIRTHDAY), 0xE, &PROFILE_BIRTHDAY);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(LanguageBlockID, sizeof(CONSOLE_LANGUAGE), 0xE, &CONSOLE_LANGUAGE);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(CountryInfoBlockID, sizeof(COUNTRY_INFO), 0xE, &COUNTRY_INFO);
    if (!res.IsSuccess())
        return res;

    u16_le country_name_buffer[16][0x40] = {};
    std::u16string region_name = Common::UTF8ToUTF16("Gensokyo");
    for (std::size_t i = 0; i < 16; ++i) {
        std::copy(region_name.cbegin(), region_name.cend(), country_name_buffer[i]);
    }
    // 0x000B0001 - Localized names for the profile Country
    res = CreateConfigInfoBlk(CountryNameBlockID, sizeof(country_name_buffer), 0xE,
                              country_name_buffer);
    if (!res.IsSuccess())
        return res;
    // 0x000B0002 - Localized names for the profile State/Province
    res = CreateConfigInfoBlk(StateNameBlockID, sizeof(country_name_buffer), 0xE,
                              country_name_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000B0003 - Unknown, related to country/address (zip code?)
    res = CreateConfigInfoBlk(0x000B0003, 0x4, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000C0000 - Unknown
    res = CreateConfigInfoBlk(0x000C0000, 0xC0, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000C0001 - Unknown
    res = CreateConfigInfoBlk(0x000C0001, 0x14, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000D0000 - Accepted EULA version
    res = CreateConfigInfoBlk(EULAVersionBlockID, 0x4, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigInfoBlk(ConsoleModelBlockID, sizeof(CONSOLE_MODEL), 0xC, &CONSOLE_MODEL);
    if (!res.IsSuccess())
        return res;

    // 0x00170000 - Unknown
    res = CreateConfigInfoBlk(0x00170000, 0x4, 0xE, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // Save the buffer to the file
    res = UpdateConfigNANDSavegame();
    if (!res.IsSuccess())
        return res;
    return RESULT_SUCCESS;
}

ResultCode Module::LoadConfigNANDSaveFile() {
    // Open the SystemSaveData archive 0x00010017
    FileSys::Path archive_path(cfg_system_savedata_id);
    auto archive_result =
        Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERR_NOT_FORMATTED) {
        // Format the archive to create the directories
        Service::FS::FormatArchive(Service::FS::ArchiveIdCode::SystemSaveData,
                                   FileSys::ArchiveFormatInfo(), archive_path);

        // Open it again to get a valid archive now that the folder exists
        archive_result =
            Service::FS::OpenArchive(Service::FS::ArchiveIdCode::SystemSaveData, archive_path);
    }

    ASSERT_MSG(archive_result.Succeeded(), "Could not open the CFG SystemSaveData archive!");

    cfg_system_save_data_archive = *archive_result;

    FileSys::Path config_path("/config");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    auto config_result = Service::FS::OpenFileFromArchive(*archive_result, config_path, open_mode);

    // Read the file if it already exists
    if (config_result.Succeeded()) {
        auto config = std::move(config_result).Unwrap();
        config->backend->Read(0, CONFIG_SAVEFILE_SIZE, cfg_config_file_buffer.data());
        return RESULT_SUCCESS;
    }

    return FormatConfig();
}

Module::Module() {
    LoadConfigNANDSaveFile();
}

Module::~Module() = default;

/// Checks if the language is available in the chosen region, and returns a proper one
static SystemLanguage AdjustLanguageInfoBlock(u32 region, SystemLanguage language) {
    static const std::array<std::vector<SystemLanguage>, 7> region_languages{{
        // JPN
        {LANGUAGE_JP},
        // USA
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_ES, LANGUAGE_PT},
        // EUR
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_DE, LANGUAGE_IT, LANGUAGE_ES, LANGUAGE_NL, LANGUAGE_PT,
         LANGUAGE_RU},
        // AUS
        {LANGUAGE_EN, LANGUAGE_FR, LANGUAGE_DE, LANGUAGE_IT, LANGUAGE_ES, LANGUAGE_NL, LANGUAGE_PT,
         LANGUAGE_RU},
        // CHN
        {LANGUAGE_ZH},
        // KOR
        {LANGUAGE_KO},
        // TWN
        {LANGUAGE_TW},
    }};
    const auto& available = region_languages[region];
    if (std::find(available.begin(), available.end(), language) == available.end()) {
        return available[0];
    }
    return language;
}

void Module::SetPreferredRegionCode(u32 region_code) {
    preferred_region_code = region_code;
    LOG_INFO(Service_CFG, "Preferred region code set to {}", preferred_region_code);

    if (Settings::values.region_value == Settings::REGION_VALUE_AUTO_SELECT) {
        const SystemLanguage current_language = GetSystemLanguage();
        const SystemLanguage adjusted_language =
            AdjustLanguageInfoBlock(region_code, current_language);
        if (current_language != adjusted_language) {
            LOG_WARNING(Service_CFG, "System language {} does not fit the region. Adjusted to {}",
                        static_cast<int>(current_language), static_cast<int>(adjusted_language));
            SetSystemLanguage(adjusted_language);
        }
    }
}

void Module::SetUsername(const std::u16string& name) {
    ASSERT(name.size() <= 10);
    UsernameBlock block{};
    name.copy(block.username, name.size());
    SetConfigInfoBlock(UsernameBlockID, sizeof(block), 4, &block);
}

std::u16string Module::GetUsername() {
    UsernameBlock block;
    GetConfigInfoBlock(UsernameBlockID, sizeof(block), 8, &block);

    // the username string in the block isn't null-terminated,
    // so we need to find the end manually.
    std::u16string username(block.username, ARRAY_SIZE(block.username));
    const std::size_t pos = username.find(u'\0');
    if (pos != std::u16string::npos)
        username.erase(pos);
    return username;
}

void Module::SetBirthday(u8 month, u8 day) {
    BirthdayBlock block = {month, day};
    SetConfigInfoBlock(BirthdayBlockID, sizeof(block), 4, &block);
}

std::tuple<u8, u8> Module::GetBirthday() {
    BirthdayBlock block;
    GetConfigInfoBlock(BirthdayBlockID, sizeof(block), 8, &block);
    return std::make_tuple(block.month, block.day);
}

void Module::SetSystemLanguage(SystemLanguage language) {
    u8 block = language;
    SetConfigInfoBlock(LanguageBlockID, sizeof(block), 4, &block);
}

SystemLanguage Module::GetSystemLanguage() {
    u8 block;
    GetConfigInfoBlock(LanguageBlockID, sizeof(block), 8, &block);
    return static_cast<SystemLanguage>(block);
}

void Module::SetSoundOutputMode(SoundOutputMode mode) {
    u8 block = mode;
    SetConfigInfoBlock(SoundOutputModeBlockID, sizeof(block), 4, &block);
}

SoundOutputMode Module::GetSoundOutputMode() {
    u8 block;
    GetConfigInfoBlock(SoundOutputModeBlockID, sizeof(block), 8, &block);
    return static_cast<SoundOutputMode>(block);
}

void Module::SetCountryCode(u8 country_code) {
    ConsoleCountryInfo block = {{0, 0, 0}, country_code};
    SetConfigInfoBlock(CountryInfoBlockID, sizeof(block), 4, &block);
}

u8 Module::GetCountryCode() {
    ConsoleCountryInfo block;
    GetConfigInfoBlock(CountryInfoBlockID, sizeof(block), 8, &block);
    return block.country_code;
}

void Module::GenerateConsoleUniqueId(u32& random_number, u64& console_id) {
    CryptoPP::AutoSeededRandomPool rng;
    random_number = rng.GenerateWord32(0, 0xFFFF);
    u64_le local_friend_code_seed;
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&local_friend_code_seed),
                      sizeof(local_friend_code_seed));
    console_id = (local_friend_code_seed & 0x3FFFFFFFF) | (static_cast<u64>(random_number) << 48);
}

ResultCode Module::SetConsoleUniqueId(u32 random_number, u64 console_id) {
    u64_le console_id_le = console_id;
    ResultCode res =
        SetConfigInfoBlock(ConsoleUniqueID1BlockID, sizeof(console_id_le), 0xE, &console_id_le);
    if (!res.IsSuccess())
        return res;

    res = SetConfigInfoBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), 0xE, &console_id_le);
    if (!res.IsSuccess())
        return res;

    u32_le random_number_le = random_number;
    res = SetConfigInfoBlock(ConsoleUniqueID3BlockID, sizeof(random_number_le), 0xE,
                             &random_number_le);
    if (!res.IsSuccess())
        return res;

    return RESULT_SUCCESS;
}

u64 Module::GetConsoleUniqueId() {
    u64_le console_id_le;
    GetConfigInfoBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), 0xE, &console_id_le);
    return console_id_le;
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cfg = std::make_shared<Module>();
    std::make_shared<CFG_I>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_S>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_U>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_NOR>()->InstallAsService(service_manager);
    current_cfg = cfg;
}

} // namespace CFG
} // namespace Service
