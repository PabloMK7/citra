// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <tuple>
#include <boost/serialization/array.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
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

SERIALIZE_EXPORT_IMPL(Service::CFG::Module)

namespace Service::CFG {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar& cfg_config_file_buffer;
    ar& cfg_system_save_data_archive;
    ar& preferred_region_code;
}
SERIALIZE_IMPL(Module)

/// The maximum number of block entries that can exist in the config file
constexpr u32 CONFIG_FILE_MAX_BLOCK_ENTRIES = 1479;

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
    ConfigSavegameVersionBlockID = 0x00000000, // Maybe?
    RtcCompensationBlockID = 0x00010000,
    AudioCalibrationBlockID = 0x00020000,
    LeapYearCounterBlockID = 0x00030000,
    UserTimeOffsetBlockID = 0x00030001,
    SettingsTimeOffsetBlockID = 0x00030002,
    TouchCalibrationBlockID = 0x00040000,
    AnalogStickCalibrationBlockID = 0x00040001, // Maybe?
    GyroscopeCalibrationBlockID = 0x00040002,
    AccelerometerCalibrationBlockID = 0x00040003,
    CStickCalibrationBlockID = 0x00040004,
    ScreenFlickerCalibrationBlockID = 0x00050000,
    BacklightControlsBlockID = 0x00050001,
    BacklightPwmCalibrationBlockID = 0x00050002,
    PowerSavingModeCalibrationBlockID = 0x00050003,
    PowerSavingModeCalibrationLegacyBlockID = 0x00050004,
    StereoCameraSettingsBlockID = 0x00050005,
    _3dSwitchingDelayBlockID = 0x00050006,
    Unknown_0x00050007 = 0x00050007,
    PowerSavingModeExtraConfigBlockID = 0x00050008,
    BacklightControlNew3dsBlockID = 0x00050009,
    Unknown_0x00060000 = 0x00060000,
    _3dFiltersBlockID = 0x00070000,
    SoundOutputModeBlockID = 0x00070001,
    MicrophoneEchoCancellationBlockID = 0x00070002,
    WifiConfigurationSlot0BlockID = 0x00080000,
    WifiConfigurationSlot1BlockID = 0x00080001,
    WifiConfigurationSlot2BlockID = 0x00080002,
    ConsoleUniqueID1BlockID = 0x00090000,
    ConsoleUniqueID2BlockID = 0x00090001,
    ConsoleUniqueID3BlockID = 0x00090002,
    UsernameBlockID = 0x000A0000,
    BirthdayBlockID = 0x000A0001,
    LanguageBlockID = 0x000A0002,
    CountryInfoBlockID = 0x000B0000,
    CountryNameBlockID = 0x000B0001,
    StateNameBlockID = 0x000B0002,
    LatitudeLongitudeBlockID = 0x000B0003,
    RestrictedPhotoExchangeBlockID = 0x000C0000,
    CoppacsRestrictionBlockID = 0x000C0001,
    ParentalRestrictionEmailBlockID = 0x000C0002,
    EULAVersionBlockID = 0x000D0000,
    Unknown_0x000E0000 = 0x000E0000,
    DebugConfigurationBlockID = 0x000F0000,
    Unknown_0x000F0001 = 0x000F0001,
    Unknown_0x000F0003 = 0x000F0003,
    ConsoleModelBlockID = 0x000F0004,
    NetworkUpdatesEnabledBlockID = 0x000F0005,
    XDeviceTokenBlockID = 0x000F0006,
    TwlEulaInfoBlockID = 0x00100000,
    TwlParentalRestrictionsBlockID = 0x00100001,
    TwlCountryCodeBlockID = 0x00100002,
    TwlMovableUniqueBlockIDBlockID = 0x00100003,
    SystemSetupRequiredBlockID = 0x00110000,
    LaunchMenuBlockID = 0x00110001,
    VolumeSliderBoundsBlockID = 0x00120000,
    DebugModeBlockID = 0x00130000,
    ClockSequenceBlockID = 0x00150000,
    Unknown_0x00150001 = 0x00150001,
    NpnsUrlID = 0x00150002, // Maybe? 3dbrew documentation is weirdly written.
    Unknown_0x00160000 = 0x00160000,
    MiiverseAccessKeyBlockID = 0x00170000,
    QtmInfraredLedRelatedBlockID = 0x00180000,
    QtmCalibrationDataBlockID = 0x00180001,
    Unknown_0x00190000 = 0x00190000,
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
    u8 unknown[2];   ///< Unknown data
    u8 state_code;   ///< The state or province code.
    u8 country_code; ///< The country code of the console
};
static_assert(sizeof(ConsoleCountryInfo) == 4, "ConsoleCountryInfo must be exactly 4 bytes");

struct BacklightControls {
    u8 power_saving_enabled; ///< Whether power saving mode is enabled.
    u8 brightness_level;     ///< The configured brightness level.
};
static_assert(sizeof(BacklightControls) == 2, "BacklightControls must be exactly 2 bytes");

struct New3dsBacklightControls {
    u8 unknown_1[4];            ///< Unknown data
    u8 auto_brightness_enabled; ///< Whether auto brightness is enabled.
    u8 unknown_2[3];            ///< Unknown data
};
static_assert(sizeof(New3dsBacklightControls) == 8,
              "New3dsBacklightControls must be exactly 8 bytes");
} // namespace

constexpr EULAVersion MAX_EULA_VERSION{0x7F, 0x7F};
constexpr ConsoleModelInfo CONSOLE_MODEL_OLD{NINTENDO_3DS_XL, {0, 0, 0}};
[[maybe_unused]] constexpr ConsoleModelInfo CONSOLE_MODEL_NEW{NEW_NINTENDO_3DS_XL, {0, 0, 0}};
constexpr u8 CONSOLE_LANGUAGE = LANGUAGE_EN;
constexpr UsernameBlock CONSOLE_USERNAME_BLOCK{u"CITRA", 0, 0};
constexpr BirthdayBlock PROFILE_BIRTHDAY{3, 25}; // March 25th, 2014
constexpr u8 SOUND_OUTPUT_MODE = SOUND_SURROUND;
constexpr u8 UNITED_STATES_COUNTRY_ID = 49;
constexpr u8 WASHINGTON_DC_STATE_ID = 2;
/// TODO(Subv): Find what the other bytes are
constexpr ConsoleCountryInfo COUNTRY_INFO{{0, 0}, WASHINGTON_DC_STATE_ID, UNITED_STATES_COUNTRY_ID};
constexpr BacklightControls BACKLIGHT_CONTROLS{0, 2};
constexpr New3dsBacklightControls NEW_3DS_BACKLIGHT_CONTROLS{{0, 0, 0, 0}, 0, {0, 0, 0}};

/**
 * TODO(Subv): Find out what this actually is, these values fix some NaN uniforms in some games,
 * for example Nintendo Zone
 * Thanks Normmatt for providing this information
 */
constexpr std::array<float, 8> STEREO_CAMERA_SETTINGS = {
    62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
    10.0f, 5.0f,   55.58000183105469f, 21.56999969482422f,
};
static_assert(sizeof(STEREO_CAMERA_SETTINGS) == 0x20,
              "STEREO_CAMERA_SETTINGS must be exactly 0x20 bytes");

constexpr std::array<u8, 8> cfg_system_savedata_id{
    0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x01, 0x00,
};

Module::Interface::Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cfg(std::move(cfg)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetCountryCodeString(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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

std::shared_ptr<Module> Module::Interface::Interface::GetModule() const {
    return cfg;
}

void Module::Interface::GetCountryCodeID(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    if (Settings::values.region_value.GetValue() == Settings::REGION_VALUE_AUTO_SELECT)
        return preferred_region_code;

    return Settings::values.region_value.GetValue();
}

void Module::Interface::GetRegion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(static_cast<u8>(cfg->GetRegionValue()));
}

void Module::Interface::SecureInfoGetByte101(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_DEBUG(Service_CFG, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    // According to 3dbrew this is normally 0.
    rb.Push<u8>(0);
}

void Module::Interface::GetTransferableId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 app_id_salt = rp.Pop<u32>() & 0x000FFFFF;

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);

    std::array<u8, 12> buffer;
    const ResultCode result =
        cfg->GetConfigBlock(ConsoleUniqueID2BlockID, 8, AccessFlag::SystemRead, buffer.data());
    rb.Push(result);
    if (result.IsSuccess()) {
        std::memcpy(&buffer[8], &app_id_salt, sizeof(u32));
        std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
        CryptoPP::SHA256().CalculateDigest(hash.data(), buffer.data(), sizeof(buffer));
        u32 low, high;
        std::memcpy(&low, &hash[hash.size() - 8], sizeof(u32));
        std::memcpy(&high, &hash[hash.size() - 4], sizeof(u32));
        rb.Push(low);
        rb.Push(high);
    } else {
        rb.Push<u32>(0);
        rb.Push<u32>(0);
    }

    LOG_DEBUG(Service_CFG, "called app_id_salt=0x{:X}", app_id_salt);
}

void Module::Interface::IsCoppacsSupported(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data{};

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigBlock(ConsoleModelBlockID, 4, AccessFlag::SystemRead,
                                reinterpret_cast<u8*>(&data)));
    ConsoleModelInfo model;
    std::memcpy(&model, &data, 4);
    if ((model.model == NINTENDO_3DS || model.model == NINTENDO_3DS_XL ||
         model.model == NINTENDO_2DS) &&
        Settings::values.is_new_3ds) {
        model.model = NEW_NINTENDO_3DS_XL;
    } else if ((model.model == NEW_NINTENDO_3DS || model.model == NEW_NINTENDO_3DS_XL ||
                model.model == NEW_NINTENDO_2DS_XL) &&
               !Settings::values.is_new_3ds) {
        model.model = NINTENDO_3DS_XL;
    }
    std::memcpy(&data, &model, 4);
    rb.Push<u8>(data & 0xFF);
}

void Module::Interface::GetModelNintendo2DS(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    u32 data{};

    // TODO(Subv): Find out the correct error codes
    rb.Push(cfg->GetConfigBlock(ConsoleModelBlockID, 4, AccessFlag::SystemRead,
                                reinterpret_cast<u8*>(&data)));
    u8 model = data & 0xFF;
    rb.Push(model != Service::CFG::NINTENDO_2DS);
}

void Module::Interface::GetConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigBlock(block_id, size, AccessFlag::UserRead, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::GetSystemConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 size = rp.Pop<u32>();
    u32 block_id = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    std::vector<u8> data(size);
    rb.Push(cfg->GetConfigBlock(block_id, size, AccessFlag::SystemRead, data.data()));
    buffer.Write(data.data(), 0, data.size());
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::SetSystemConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 block_id = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    std::vector<u8> data(size);
    buffer.Read(data.data(), 0, data.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(cfg->SetConfigBlock(block_id, size, AccessFlag::SystemWrite, data.data()));
    rb.PushMappedBuffer(buffer);
}

void Module::Interface::UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->UpdateConfigNANDSavegame());
}

void Module::Interface::FormatConfig(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(cfg->FormatConfig());
}

ResultVal<void*> Module::GetConfigBlockPointer(u32 block_id, u32 size, AccessFlag accesss_flag) {
    // Read the header
    auto config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    auto itr =
        std::find_if(std::begin(config->block_entries), std::end(config->block_entries),
                     [&](const SaveConfigBlockEntry& entry) { return entry.block_id == block_id; });

    if (itr == std::end(config->block_entries)) {
        LOG_ERROR(Service_CFG, "Config block 0x{:X} with flags {} and size {} was not found",
                  block_id, accesss_flag, size);
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if (False(itr->access_flags & accesss_flag)) {
        LOG_ERROR(Service_CFG, "Invalid access flag {:X} for config block 0x{:X} with size {}",
                  accesss_flag, block_id, size);
        return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    if (itr->size != size) {
        LOG_ERROR(Service_CFG, "Invalid size {} for config block 0x{:X} with flags {}", size,
                  block_id, accesss_flag);
        return ResultCode(ErrorDescription::InvalidSize, ErrorModule::Config,
                          ErrorSummary::WrongArgument, ErrorLevel::Permanent);
    }

    void* pointer;

    // The data is located in the block header itself if the size is less than 4 bytes
    if (itr->size <= 4)
        pointer = &itr->offset_or_data;
    else
        pointer = &cfg_config_file_buffer[itr->offset_or_data];

    return pointer;
}

ResultCode Module::GetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag, void* output) {
    void* pointer = nullptr;
    CASCADE_RESULT(pointer, GetConfigBlockPointer(block_id, size, accesss_flag));
    std::memcpy(output, pointer, size);

    return RESULT_SUCCESS;
}

ResultCode Module::SetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag,
                                  const void* input) {
    void* pointer = nullptr;
    CASCADE_RESULT(pointer, GetConfigBlockPointer(block_id, size, accesss_flag));
    std::memcpy(pointer, input, size);
    return RESULT_SUCCESS;
}

ResultCode Module::CreateConfigBlock(u32 block_id, u16 size, AccessFlag access_flags,
                                     const void* data) {
    SaveFileConfig* config = reinterpret_cast<SaveFileConfig*>(cfg_config_file_buffer.data());
    if (config->total_entries >= CONFIG_FILE_MAX_BLOCK_ENTRIES)
        return ResultCode(-1); // TODO(Subv): Find the right error code

    // Insert the block header with offset 0 for now
    config->block_entries[config->total_entries] = {block_id, 0, size, access_flags};
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
        std::memcpy(&cfg_config_file_buffer[offset], data, size);
    } else {
        // The offset_or_data field in the header contains the data itself if it's 4 bytes or less
        std::memcpy(&config->block_entries[config->total_entries].offset_or_data, data, size);
    }

    ++config->total_entries;
    return RESULT_SUCCESS;
}

ResultCode Module::DeleteConfigNANDSaveFile() {
    FileSys::Path path("/config");
    return cfg_system_save_data_archive->DeleteFile(path);
}

ResultCode Module::UpdateConfigNANDSavegame() {
    FileSys::Mode mode = {};
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    FileSys::Path path("/config");

    auto config_result = cfg_system_save_data_archive->OpenFile(path, mode);
    ASSERT_MSG(config_result.Succeeded(), "could not open file");

    auto config = std::move(config_result).Unwrap();
    config->Write(0, CONFIG_SAVEFILE_SIZE, 1, cfg_config_file_buffer.data());

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

    // 0x00030001 - User time offset (Read by CECD): displayed timestamp - RTC timestamp
    res = CreateConfigBlock(UserTimeOffsetBlockID, 0x8, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x00050001 - Backlight controls
    res = CreateConfigBlock(BacklightControlsBlockID, sizeof(BACKLIGHT_CONTROLS),
                            AccessFlag::System, &BACKLIGHT_CONTROLS);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(StereoCameraSettingsBlockID, sizeof(STEREO_CAMERA_SETTINGS),
                            AccessFlag::Global, STEREO_CAMERA_SETTINGS.data());
    if (!res.IsSuccess())
        return res;

    // 0x00050009 - New 3DS backlight controls
    res = CreateConfigBlock(BacklightControlNew3dsBlockID, sizeof(NEW_3DS_BACKLIGHT_CONTROLS),
                            AccessFlag::System, &NEW_3DS_BACKLIGHT_CONTROLS);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(SoundOutputModeBlockID, sizeof(SOUND_OUTPUT_MODE), AccessFlag::Global,
                            &SOUND_OUTPUT_MODE);
    if (!res.IsSuccess())
        return res;

    const auto [random_number, console_id] = GenerateConsoleUniqueId();

    u64_le console_id_le = console_id;
    res = CreateConfigBlock(ConsoleUniqueID1BlockID, sizeof(console_id_le), AccessFlag::Global,
                            &console_id_le);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), AccessFlag::Global,
                            &console_id_le);
    if (!res.IsSuccess())
        return res;

    u32_le random_number_le = random_number;
    res = CreateConfigBlock(ConsoleUniqueID3BlockID, sizeof(random_number_le), AccessFlag::Global,
                            &random_number_le);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(UsernameBlockID, sizeof(CONSOLE_USERNAME_BLOCK), AccessFlag::Global,
                            &CONSOLE_USERNAME_BLOCK);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(BirthdayBlockID, sizeof(PROFILE_BIRTHDAY), AccessFlag::Global,
                            &PROFILE_BIRTHDAY);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(LanguageBlockID, sizeof(CONSOLE_LANGUAGE), AccessFlag::Global,
                            &CONSOLE_LANGUAGE);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(CountryInfoBlockID, sizeof(COUNTRY_INFO), AccessFlag::Global,
                            &COUNTRY_INFO);
    if (!res.IsSuccess())
        return res;

    u16_le country_name_buffer[16][0x40] = {};
    std::u16string region_name = Common::UTF8ToUTF16("Gensokyo");
    for (std::size_t i = 0; i < 16; ++i) {
        std::copy(region_name.cbegin(), region_name.cend(), country_name_buffer[i]);
    }
    // 0x000B0001 - Localized names for the profile Country
    res = CreateConfigBlock(CountryNameBlockID, sizeof(country_name_buffer), AccessFlag::Global,
                            country_name_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000B0002 - Localized names for the profile State/Province
    res = CreateConfigBlock(StateNameBlockID, sizeof(country_name_buffer), AccessFlag::Global,
                            country_name_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000B0003 - Coordinates. A pair of s16 represents latitude and longitude, respectively. One
    // need to multiply both value by 180/32768 to get coordinates in degrees
    res = CreateConfigBlock(LatitudeLongitudeBlockID, 0x4, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000C0000 - Restricted photo exchange data, and other info (includes a mirror of Parental
    // Restrictions PIN/Secret Answer)
    res = CreateConfigBlock(RestrictedPhotoExchangeBlockID, 0xC0, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000C0001 - COPPACS restriction data
    res = CreateConfigBlock(CoppacsRestrictionBlockID, 0x14, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x000D0000 - Accepted EULA version
    u32_le data = MAX_EULA_VERSION.minor + (MAX_EULA_VERSION.major << 8);
    res = CreateConfigBlock(EULAVersionBlockID, sizeof(data), AccessFlag::Global, &data);
    if (!res.IsSuccess())
        return res;

    u8 unknown_data = 0;
    res = CreateConfigBlock(Unknown_0x000E0000, sizeof(unknown_data), AccessFlag::Global,
                            &unknown_data);
    if (!res.IsSuccess())
        return res;

    res = CreateConfigBlock(ConsoleModelBlockID, sizeof(CONSOLE_MODEL_OLD), AccessFlag::System,
                            &CONSOLE_MODEL_OLD);
    if (!res.IsSuccess())
        return res;

    // 0x000F0006 - In NIM, taken as a (hopefully null terminated) string used for the
    // "X-Device-Token" http header field for NPNS url.
    res = CreateConfigBlock(XDeviceTokenBlockID, 0x28, AccessFlag::System, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x00110000 - The low u16 indicates whether the system setup is required, such as when the
    // system is booted for the first time or after doing a System Format: 0 = setup required,
    // non-zero = no setup required
    u32 system_setup_flag = 1;
    res =
        CreateConfigBlock(SystemSetupRequiredBlockID, 0x4, AccessFlag::System, &system_setup_flag);
    if (!res.IsSuccess())
        return res;

    // 0x00130000 - DebugMode (0x100 for debug mode)
    res = CreateConfigBlock(DebugModeBlockID, 0x4, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x00160000 - Unknown, first byte is used by config service-cmd 0x00070040.
    res = CreateConfigBlock(0x00160000, 0x4, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // 0x00170000 - Miiverse (OLV) access key
    res = CreateConfigBlock(MiiverseAccessKeyBlockID, 0x4, AccessFlag::Global, zero_buffer);
    if (!res.IsSuccess())
        return res;

    // Save the buffer to the file
    res = UpdateConfigNANDSavegame();
    if (!res.IsSuccess())
        return res;
    return RESULT_SUCCESS;
}

ResultCode Module::LoadConfigNANDSaveFile() {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010017
    FileSys::Path archive_path(cfg_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ERROR_NOT_FOUND) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0);

        // Open it again to get a valid archive now that the folder exists
        cfg_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the CFG SystemSaveData archive!");

        cfg_system_save_data_archive = std::move(archive_result).Unwrap();
    }

    FileSys::Path config_path("/config");
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);

    auto config_result = cfg_system_save_data_archive->OpenFile(config_path, open_mode);

    // Read the file if it already exists
    if (config_result.Succeeded()) {
        auto config = std::move(config_result).Unwrap();
        config->Read(0, CONFIG_SAVEFILE_SIZE, cfg_config_file_buffer.data());
        return RESULT_SUCCESS;
    }

    return FormatConfig();
}

Module::Module() {
    LoadConfigNANDSaveFile();
    // Check the config savegame EULA Version and update it to 0x7F7F if necessary
    // so users will never get a promt to accept EULA
    EULAVersion version = GetEULAVersion();
    if (version.major != MAX_EULA_VERSION.major || version.minor != MAX_EULA_VERSION.minor) {
        LOG_INFO(Service_CFG, "Updating accepted EULA version to {}.{}", MAX_EULA_VERSION.major,
                 MAX_EULA_VERSION.minor);
        SetEULAVersion(Service::CFG::MAX_EULA_VERSION);
        UpdateConfigNANDSavegame();
    }
}

Module::~Module() = default;

/// Checks if the language is available in the chosen region, and returns a proper one
static std::tuple<u32 /*region*/, SystemLanguage> AdjustLanguageInfoBlock(
    std::span<const u32> region_code, SystemLanguage language) {
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
    // Check if any available region supports the languages
    for (u32 region : region_code) {
        const auto& available = region_languages[region];
        if (std::find(available.begin(), available.end(), language) != available.end()) {
            // found a proper region, so return this region - language pair
            return {region, language};
        }
    }
    // The language is not available in any available region, so default to the first region and
    // language
    const u32 default_region = region_code[0];
    return {default_region, region_languages[default_region][0]};
}

void Module::SetPreferredRegionCodes(std::span<const u32> region_codes) {
    const SystemLanguage current_language = GetSystemLanguage();
    const auto [region, adjusted_language] =
        AdjustLanguageInfoBlock(region_codes, current_language);

    preferred_region_code = region;
    LOG_INFO(Service_CFG, "Preferred region code set to {}", preferred_region_code);

    if (Settings::values.region_value.GetValue() == Settings::REGION_VALUE_AUTO_SELECT) {
        if (current_language != adjusted_language) {
            LOG_WARNING(Service_CFG, "System language {} does not fit the region. Adjusted to {}",
                        current_language, adjusted_language);
            SetSystemLanguage(adjusted_language);
        }
    }
}

void Module::SetUsername(const std::u16string& name) {
    ASSERT(name.size() <= 10);
    UsernameBlock block{};
    name.copy(block.username, name.size());
    SetConfigBlock(UsernameBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

std::u16string Module::GetUsername() {
    UsernameBlock block;
    GetConfigBlock(UsernameBlockID, sizeof(block), AccessFlag::SystemRead, &block);

    // the username string in the block isn't null-terminated,
    // so we need to find the end manually.
    std::u16string username(block.username, std::size(block.username));
    const std::size_t pos = username.find(u'\0');
    if (pos != std::u16string::npos)
        username.erase(pos);
    return username;
}

void Module::SetBirthday(u8 month, u8 day) {
    BirthdayBlock block = {month, day};
    SetConfigBlock(BirthdayBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

std::tuple<u8, u8> Module::GetBirthday() {
    BirthdayBlock block;
    GetConfigBlock(BirthdayBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return std::make_tuple(block.month, block.day);
}

void Module::SetSystemLanguage(SystemLanguage language) {
    u8 block = language;
    SetConfigBlock(LanguageBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

SystemLanguage Module::GetSystemLanguage() {
    u8 block{};
    GetConfigBlock(LanguageBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return static_cast<SystemLanguage>(block);
}

void Module::SetSoundOutputMode(SoundOutputMode mode) {
    u8 block = mode;
    SetConfigBlock(SoundOutputModeBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

SoundOutputMode Module::GetSoundOutputMode() {
    u8 block{};
    GetConfigBlock(SoundOutputModeBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return static_cast<SoundOutputMode>(block);
}

void Module::SetCountryCode(u8 country_code) {
    ConsoleCountryInfo block = {{0, 0}, default_subregion[country_code], country_code};
    SetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

u8 Module::GetCountryCode() {
    ConsoleCountryInfo block{};
    GetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return block.country_code;
}

void Module::SetCountryInfo(u8 country_code, u8 state_code) {
    ConsoleCountryInfo block = {{0, 0}, state_code, country_code};
    SetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemWrite, &block);
}

u8 Module::GetStateCode() {
    ConsoleCountryInfo block{};
    GetConfigBlock(CountryInfoBlockID, sizeof(block), AccessFlag::SystemRead, &block);
    return block.state_code;
}

std::pair<u32, u64> Module::GenerateConsoleUniqueId() const {
    CryptoPP::AutoSeededRandomPool rng;
    const u32 random_number = rng.GenerateWord32(0, 0xFFFF);

    u64_le local_friend_code_seed;
    rng.GenerateBlock(reinterpret_cast<CryptoPP::byte*>(&local_friend_code_seed),
                      sizeof(local_friend_code_seed));

    const u64 console_id =
        (local_friend_code_seed & 0x3FFFFFFFF) | (static_cast<u64>(random_number) << 48);

    return std::make_pair(random_number, console_id);
}

ResultCode Module::SetConsoleUniqueId(u32 random_number, u64 console_id) {
    u64_le console_id_le = console_id;
    ResultCode res = SetConfigBlock(ConsoleUniqueID1BlockID, sizeof(console_id_le),
                                    AccessFlag::Global, &console_id_le);
    if (!res.IsSuccess())
        return res;

    res = SetConfigBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), AccessFlag::Global,
                         &console_id_le);
    if (!res.IsSuccess())
        return res;

    u32_le random_number_le = random_number;
    res = SetConfigBlock(ConsoleUniqueID3BlockID, sizeof(random_number_le), AccessFlag::Global,
                         &random_number_le);
    if (!res.IsSuccess())
        return res;

    return RESULT_SUCCESS;
}

u64 Module::GetConsoleUniqueId() {
    u64_le console_id_le{};
    GetConfigBlock(ConsoleUniqueID2BlockID, sizeof(console_id_le), AccessFlag::Global,
                   &console_id_le);
    return console_id_le;
}

EULAVersion Module::GetEULAVersion() {
    u32_le data{};
    GetConfigBlock(EULAVersionBlockID, sizeof(data), AccessFlag::Global, &data);
    EULAVersion version;
    version.minor = data & 0xFF;
    version.major = (data >> 8) & 0xFF;
    return version;
}

void Module::SetEULAVersion(const EULAVersion& version) {
    u32_le data = version.minor + (version.major << 8);
    SetConfigBlock(EULAVersionBlockID, sizeof(data), AccessFlag::Global, &data);
}

void Module::SetSystemSetupNeeded(bool setup_needed) {
    u32 block = setup_needed ? 0 : 1;
    SetConfigBlock(SystemSetupRequiredBlockID, sizeof(block), AccessFlag::System, &block);
}

bool Module::IsSystemSetupNeeded() {
    u32 block{};
    GetConfigBlock(SystemSetupRequiredBlockID, sizeof(block), AccessFlag::System, &block);
    return (block & 0xFFFF) == 0;
}

std::shared_ptr<Module> GetModule(Core::System& system) {
    auto cfg = system.ServiceManager().GetService<Service::CFG::Module::Interface>("cfg:u");
    if (!cfg)
        return nullptr;
    return cfg->GetModule();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto cfg = std::make_shared<Module>();
    std::make_shared<CFG_I>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_S>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_U>(cfg)->InstallAsService(service_manager);
    std::make_shared<CFG_NOR>()->InstallAsService(service_manager);
}

std::string GetConsoleIdHash(Core::System& system) {
    u64_le console_id{};
    std::array<u8, sizeof(console_id)> buffer;
    if (system.IsPoweredOn()) {
        auto cfg = GetModule(system);
        ASSERT_MSG(cfg, "CFG Module missing!");
        console_id = cfg->GetConsoleUniqueId();
    } else {
        console_id = std::make_unique<Service::CFG::Module>()->GetConsoleUniqueId();
    }
    std::memcpy(buffer.data(), &console_id, sizeof(console_id));

    std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
    CryptoPP::SHA256().CalculateDigest(hash.data(), buffer.data(), sizeof(buffer));
    return fmt::format("{:02x}", fmt::join(hash.begin(), hash.end(), ""));
}

} // namespace Service::CFG
