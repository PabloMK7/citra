// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common.h"

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
};

/// Return type for functions in Loader namespace
enum class ResultStatus {
    Success,
    Error,
    ErrorInvalidFormat,
    ErrorNotImplemented,
    ErrorNotLoaded,
    ErrorAlreadyLoaded,
};

/// Interface for loading an application
class AppLoader : NonCopyable {
public:
    AppLoader() { }
    virtual ~AppLoader() { }

    /**
     * Load the application
     * @return ResultStatus result of function
     */
    virtual const ResultStatus Load() = 0;

    /**
     * Get the code (typically .code section) of the application
     * @param error ResultStatus result of function
     * @return Reference to code buffer
     */
    virtual const std::vector<u8>& GetCode(ResultStatus& error) const {
        error = ResultStatus::ErrorNotImplemented;
        return code;
    }

    /**
     * Get the icon (typically .icon section) of the application
     * @param error ResultStatus result of function
     * @return Reference to icon buffer
     */
    virtual const std::vector<u8>& GetIcon(ResultStatus& error) const {
        error = ResultStatus::ErrorNotImplemented;
        return icon;
    }

    /**
     * Get the banner (typically .banner section) of the application
     * @param error ResultStatus result of function
     * @return Reference to banner buffer
     */
    virtual const std::vector<u8>& GetBanner(ResultStatus& error) const {
        error = ResultStatus::ErrorNotImplemented;
        return banner;
    }

    /**
     * Get the logo (typically .logo section) of the application
     * @param error ResultStatus result of function
     * @return Reference to logo buffer
     */
    virtual const std::vector<u8>& GetLogo(ResultStatus& error) const {
        error = ResultStatus::ErrorNotImplemented;
        return logo;
    }

    /**
     * Get the RomFs archive of the application
     * @param error ResultStatus result of function
     * @return Reference to RomFs archive buffer
     */
    virtual const std::vector<u8>& GetRomFs(ResultStatus error) const {
        error = ResultStatus::ErrorNotImplemented;
        return romfs;
    }

protected:
    std::vector<u8> code;   ///< ExeFS .code section
    std::vector<u8> icon;   ///< ExeFS .icon section
    std::vector<u8> banner; ///< ExeFS .banner section
    std::vector<u8> logo;   ///< ExeFS .logo section
    std::vector<u8> romfs;  ///< RomFs archive
};

/**
 * Identifies the type of a bootable file
 * @param filename String filename of bootable file
 * @return FileType of file
 */
const FileType IdentifyFile(const std::string &filename);

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
const ResultStatus LoadFile(std::string& filename);

} // namespace
