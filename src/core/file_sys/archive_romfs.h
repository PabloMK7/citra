// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

#include "core/file_sys/archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the RomFS archive
class Archive_RomFS final : public Archive {
public:
    Archive_RomFS(const Loader::AppLoader& app_loader);
    ~Archive_RomFS() override;

    /**
     * Get the IdCode of the archive (e.g. RomFS, SaveData, etc.)
     * @return IdCode of the archive
     */
    IdCode GetIdCode() const override { return IdCode::RomFS; };

    /**
     * Read data from the archive
     * @param offset Offset in bytes to start reading archive from
     * @param length Length in bytes to read data from archive
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    size_t Read(const u64 offset, const u32 length, u8* buffer) const override;

    /**
     * Get the size of the archive in bytes
     * @return Size of the archive in bytes
     */
    size_t GetSize() const override;

private:
    std::vector<u8> raw_data;
};

} // namespace FileSys
