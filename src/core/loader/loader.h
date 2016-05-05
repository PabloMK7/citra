// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/swap.h"

namespace Kernel {
struct AddressMapping;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// File types supported by CTR
enum class FileType {
    Error,
    Unknown,
    CCI,
    CXI,
    CIA,
    ELF,
    THREEDSX, //3DSX
};

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file open file
 * @return FileType of file
 */
FileType IdentifyFile(FileUtil::IOFile& file);

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file_name path to file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType IdentifyFile(const std::string& file_name);

/**
 * Guess the type of a bootable file from its extension
 * @param extension String extension of bootable file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType GuessFromExtension(const std::string& extension_);

/**
 * Convert a FileType into a string which can be displayed to the user.
 */
const char* GetFileTypeString(FileType type);

/// Return type for functions in Loader namespace
enum class ResultStatus {
    Success,
    Error,
    ErrorInvalidFormat,
    ErrorNotImplemented,
    ErrorNotLoaded,
    ErrorNotUsed,
    ErrorAlreadyLoaded,
    ErrorMemoryAllocationFailed,
    ErrorEncrypted,
};

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

/// SMDH data structure that contains titles, icons etc. See https://www.3dbrew.org/wiki/SMDH
struct SMDH {
    u32_le magic;
    u16_le version;
    INSERT_PADDING_BYTES(2);

    struct Title {
        std::array<u16, 0x40> short_title;
        std::array<u16, 0x80> long_title;
        std::array<u16, 0x40> publisher;
    };
    std::array<Title, 16> titles;

    std::array<u8, 16> ratings;
    u32_le region_lockout;
    u32_le match_maker_id;
    u64_le match_maker_bit_id;
    u32_le flags;
    u16_le eula_version;
    INSERT_PADDING_BYTES(2);
    float_le banner_animation_frame;
    u32_le cec_id;
    INSERT_PADDING_BYTES(8);

    std::array<u8, 0x480> small_icon;
    std::array<u8, 0x1200> large_icon;

    /// indicates the language used for each title entry
    enum class TitleLanguage {
        Japanese = 0,
        English = 1,
        French = 2,
        German = 3,
        Italian = 4,
        Spanish = 5,
        SimplifiedChinese = 6,
        Korean= 7,
        Dutch = 8,
        Portuguese = 9,
        Russian = 10,
        TraditionalChinese = 11
    };
};
static_assert(sizeof(SMDH) == 0x36C0, "SMDH structure size is wrong");

/// Interface for loading an application
class AppLoader : NonCopyable {
public:
    AppLoader(FileUtil::IOFile&& file) : file(std::move(file)) { }
    virtual ~AppLoader() { }

    /**
     * Load the application
     * @return ResultStatus result of function
     */
    virtual ResultStatus Load() = 0;

    /**
     * Get the code (typically .code section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadCode(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the icon (typically icon section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadIcon(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the banner (typically banner section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadBanner(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the logo (typically logo section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadLogo(std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @param offset The offset the romfs begins on
     * @param size The size of the romfs
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file, u64& offset, u64& size) {
        return ResultStatus::ErrorNotImplemented;
    }

protected:
    FileUtil::IOFile file;
    bool is_loaded = false;
};

/**
 * Common address mappings found in most games, used for binary formats that don't have this
 * information.
 */
extern const std::initializer_list<Kernel::AddressMapping> default_address_mappings;

/**
 * Get a loader for a file with a specific type
 * @param file The file to load
 * @param type The type of the file
 * @param filename the file name (without path)
 * @param filepath the file full path (with name)
 * @return std::unique_ptr<AppLoader> a pointer to a loader object;  nullptr for unsupported type
 */
std::unique_ptr<AppLoader> GetLoader(FileUtil::IOFile&& file, FileType type, const std::string& filename, const std::string& filepath);

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
ResultStatus LoadFile(const std::string& filename);

} // namespace
