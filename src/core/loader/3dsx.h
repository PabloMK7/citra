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

    FileType GetFileType() override {
        return IdentifyType(file);
    }

    ResultStatus Load(Kernel::SharedPtr<Kernel::Process>& process) override;

    ResultStatus ReadIcon(std::vector<u8>& buffer) override;

    ResultStatus ReadRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file, u64& offset,
                           u64& size) override;

private:
    std::string filename;
    std::string filepath;
};

} // namespace Loader
