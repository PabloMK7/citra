// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "common/common_types.h"
#include "common/file_util.h"

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
};

static inline u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

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
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
ResultStatus LoadFile(const std::string& filename);

} // namespace
