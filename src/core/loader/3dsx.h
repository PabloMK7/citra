// Copyright 2014 Dolphin Emulator Project / Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

/// Loads an 3DSX file
class AppLoader_THREEDSX final : public AppLoader {
public:
    AppLoader_THREEDSX(FileUtil::IOFile&& file, const std::string& filename,
                       const std::string& filepath)
        : AppLoader(std::move(file)), filename(std::move(filename)), filepath(filepath) {}

    /**
     * Returns the type of the file
     * @param file FileUtil::IOFile open file
     * @return FileType found, or FileType::Error if this loader doesn't know it
     */
    static FileType IdentifyType(FileUtil::IOFile& file);

    /**
     * Returns the type of this file
     * @return FileType corresponding to the loaded file
     */
    FileType GetFileType() override {
        return IdentifyType(file);
    }

    /**
     * Load the bootable file
     * @return ResultStatus result of function
     */
    ResultStatus Load() override;

    /**
     * Get the icon (typically icon section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    /**
     * Get the RomFS of the application
     * @param romfs_file Reference to buffer to store data
     * @param offset     Offset in the file to the RomFS
     * @param size       Size of the RomFS in bytes
     * @return ResultStatus result of function
     */
    ResultStatus ReadRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file, u64& offset,
                           u64& size) override;

private:
    std::string filename;
    std::string filepath;
};

} // namespace Loader
