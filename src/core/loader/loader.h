// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common.h"
#include "common/file_util.h"

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
    BIN,
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

/// Interface for loading an application
class AppLoader : NonCopyable {
public:
    AppLoader(std::unique_ptr<FileUtil::IOFile>&& file) : file(std::move(file)) { }
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
    virtual ResultStatus ReadCode(std::vector<u8>& buffer) const {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the icon (typically icon section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadIcon(std::vector<u8>& buffer) const {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the banner (typically banner section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadBanner(std::vector<u8>& buffer) const {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the logo (typically logo section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadLogo(std::vector<u8>& buffer) const {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(std::vector<u8>& buffer) const {
        return ResultStatus::ErrorNotImplemented;
    }

protected:
    std::unique_ptr<FileUtil::IOFile> file;
    bool                              is_loaded = false;
};

/**
 * Identifies the type of a bootable file
 * @param filename String filename of bootable file
 * @return FileType of file
 */
FileType IdentifyFile(const std::string &filename);

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
ResultStatus LoadFile(const std::string& filename);

} // namespace
