// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace FileSys {
class ArchiveBackend;
}

namespace Core {
class System;
}

namespace Service::CFG {

enum SystemModel {
    NINTENDO_3DS = 0,
    NINTENDO_3DS_XL = 1,
    NEW_NINTENDO_3DS = 2,
    NINTENDO_2DS = 3,
    NEW_NINTENDO_3DS_XL = 4,
    NEW_NINTENDO_2DS_XL = 5
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
    LANGUAGE_RU = 10,
    LANGUAGE_TW = 11
};

enum SoundOutputMode { SOUND_MONO = 0, SOUND_STEREO = 1, SOUND_SURROUND = 2 };

struct EULAVersion {
    u8 minor;
    u8 major;
};

/// Block header in the config savedata file
struct SaveConfigBlockEntry {
    u32 block_id;       ///< The id of the current block
    u32 offset_or_data; ///< This is the absolute offset to the block data if the size is greater
                        /// than 4 bytes, otherwise it contains the data itself
    u16 size;           ///< The size of the block
    u16 flags;          ///< The flags of the block, possibly used for access control
};

static constexpr u16 C(const char code[2]) {
    return code[0] | (code[1] << 8);
}

static const std::array<u16, 187> country_codes = {{
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
    C("SM"), C("VA"), C("BM"),                                              // 184-186
}};

class Module final {
public:
    Module();
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        /**
         * CFG::GetCountryCodeString service function
         *  Inputs:
         *      1 : Country Code ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Country's 2-char string
         */
        void GetCountryCodeString(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetCountryCodeID service function
         *  Inputs:
         *      1 : Country Code 2-char string
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Country Code ID
         */
        void GetCountryCodeID(Kernel::HLERequestContext& ctx);

        /**
         * CFG::SecureInfoGetRegion service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Region value loaded from SecureInfo offset 0x100
         */
        void SecureInfoGetRegion(Kernel::HLERequestContext& ctx, u16 id);

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
        void GenHashConsoleUnique(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetRegionCanadaUSA service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 1 if the system is a Canada or USA model, 0 otherwise
         */
        void GetRegionCanadaUSA(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetSystemModel service function
         *  Inputs:
         *      0 : 0x00050000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Model of the console
         */
        void GetSystemModel(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetModelNintendo2DS service function
         *  Inputs:
         *      0 : 0x00060000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 0 if the system is a Nintendo 2DS, 1 otherwise
         */
        void GetModelNintendo2DS(Kernel::HLERequestContext& ctx);

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
        void GetConfigInfoBlk2(Kernel::HLERequestContext& ctx);

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
        void GetConfigInfoBlk8(Kernel::HLERequestContext& ctx, u16 id);

        /**
         * CFG::SetConfigInfoBlk4 service function
         *  Inputs:
         *      0 : 0x04020082 / 0x08020082
         *      1 : Block ID
         *      2 : Size
         *      3 : Descriptor for the output buffer
         *      4 : Output buffer pointer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *  Note:
         *      The parameters order is different from GetConfigInfoBlk2/8's,
         *      where Block ID and Size are switched.
         */
        void SetConfigInfoBlk4(Kernel::HLERequestContext& ctx, u16 id);

        /**
         * CFG::UpdateConfigNANDSavegame service function
         *  Inputs:
         *      0 : 0x04030000 / 0x08030000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx, u16 id);

        /**
         * CFG::FormatConfig service function
         *  Inputs:
         *      0 : 0x08060000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void FormatConfig(Kernel::HLERequestContext& ctx);

        /// A helper function for dispatching service functions that have multiple IDs
        template <void (Interface::*function)(Kernel::HLERequestContext& ctx, u16 id), u16 id>
        void D(Kernel::HLERequestContext& ctx) {
            (this->*function)(ctx, id);
        }

    protected:
        std::shared_ptr<Module> cfg;
    };

private:
    ResultVal<void*> GetConfigInfoBlockPointer(u32 block_id, u32 size, u32 flag);

    /**
     * Reads a block with the specified id and flag from the Config savegame buffer
     * and writes the output to output. The input size must match exactly the size of the requested
     * block.
     *
     * @param block_id The id of the block we want to read
     * @param size The size of the block we want to read
     * @param flag The requested block must have this flag set
     * @param output A pointer where we will write the read data
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode GetConfigInfoBlock(u32 block_id, u32 size, u32 flag, void* output);

    /**
     * Reads data from input and writes to a block with the specified id and flag
     * in the Config savegame buffer. The input size must match exactly the size of the target
     * block.
     *
     * @param block_id The id of the block we want to write
     * @param size The size of the block we want to write
     * @param flag The target block must have this flag set
     * @param input A pointer where we will read data and write to Config savegame buffer
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode SetConfigInfoBlock(u32 block_id, u32 size, u32 flag, const void* input);

    /**
     * Creates a block with the specified id and writes the input data to the cfg savegame buffer in
     * memory. The config savegame file in the filesystem is not updated.
     *
     * @param block_id The id of the block we want to create
     * @param size The size of the block we want to create
     * @param flags The flags of the new block
     * @param data A pointer containing the data we will write to the new block
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode CreateConfigInfoBlk(u32 block_id, u16 size, u16 flags, const void* data);

    /**
     * Deletes the config savegame file from the filesystem, the buffer in memory is not affected
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode DeleteConfigNANDSaveFile();

    /**
     * Re-creates the config savegame file in memory and the filesystem with the default blocks
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode FormatConfig();

    /**
     * Open the config savegame file and load it to the memory buffer
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode LoadConfigNANDSaveFile();

public:
    u32 GetRegionValue();

    /**
     * Set the region codes preferred by the game so that CFG will adjust to it when the region
     * setting is auto.
     * @param region_codes the preferred region codes to set
     */
    void SetPreferredRegionCodes(const std::vector<u32>& region_codes);

    // Utilities for frontend to set config data.
    // Note: UpdateConfigNANDSavegame should be called after making changes to config data.

    /**
     * Sets the username in config savegame.
     * @param name the username to set. The maximum size is 10 in char16_t.
     */
    void SetUsername(const std::u16string& name);

    /**
     * Gets the username from config savegame.
     * @returns the username
     */
    std::u16string GetUsername();

    /**
     * Sets the profile birthday in config savegame.
     * @param month the month of birthday.
     * @param day the day of the birthday.
     */
    void SetBirthday(u8 month, u8 day);

    /**
     * Gets the profile birthday from the config savegame.
     * @returns a tuple of (month, day) of birthday
     */
    std::tuple<u8, u8> GetBirthday();

    /**
     * Sets the system language in config savegame.
     * @param language the system language to set.
     */
    void SetSystemLanguage(SystemLanguage language);

    /**
     * Gets the system language from config savegame.
     * @returns the system language
     */
    SystemLanguage GetSystemLanguage();

    /**
     * Sets the sound output mode in config savegame.
     * @param mode the sound output mode to set
     */
    void SetSoundOutputMode(SoundOutputMode mode);

    /**
     * Gets the sound output mode from config savegame.
     * @returns the sound output mode
     */
    SoundOutputMode GetSoundOutputMode();

    /**
     * Sets the country code in config savegame.
     * @param country_code the country code to set
     */
    void SetCountryCode(u8 country_code);

    /**
     * Gets the country code from config savegame.
     * @returns the country code
     */
    u8 GetCountryCode();

    /**
     * Generates a new random console unique id.
     *
     * @returns A pair containing a random number and a random console ID.
     *
     * @note The random number is a random generated 16bit number stored at 0x90002, used for
     *       generating the console ID.
     */
    std::pair<u32, u64> GenerateConsoleUniqueId() const;

    /**
     * Sets the random_number and the  console unique id in the config savegame.
     * @param random_number the random_number to set
     * @param console_id the console id to set
     */
    ResultCode SetConsoleUniqueId(u32 random_number, u64 console_id);

    /**
     * Gets the console unique id from config savegame.
     * @returns the console unique id
     */
    u64 GetConsoleUniqueId();

    /**
     * Sets the accepted EULA version in the config savegame.
     * @param version the version to set
     */
    void SetEULAVersion(const EULAVersion& version);

    /**
     * Gets the accepted EULA version from config savegame.
     * @returns the EULA version
     */
    EULAVersion GetEULAVersion();

    /**
     * Writes the config savegame memory buffer to the config savegame file in the filesystem
     * @returns ResultCode indicating the result of the operation, 0 on success
     */
    ResultCode UpdateConfigNANDSavegame();

private:
    static constexpr u32 CONFIG_SAVEFILE_SIZE = 0x8000;
    std::array<u8, CONFIG_SAVEFILE_SIZE> cfg_config_file_buffer;
    std::unique_ptr<FileSys::ArchiveBackend> cfg_system_save_data_archive;
    u32 preferred_region_code = 0;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

/// Convenience function for getting a SHA256 hash of the Console ID
std::string GetConsoleIdHash(Core::System& system);

} // namespace Service::CFG

BOOST_CLASS_EXPORT_KEY(Service::CFG::Module)
