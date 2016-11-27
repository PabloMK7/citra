// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/file_sys/archive_sdmc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * Archive backend for SDMC write-only archive.
 * The behaviour of SDMCWriteOnlyArchive is almost the same as SDMCArchive, except for
 *  - OpenDirectory is unsupported;
 *  - OpenFile with read flag is unsupported.
 */
class SDMCWriteOnlyArchive : public SDMCArchive {
public:
    SDMCWriteOnlyArchive(const std::string& mount_point) : SDMCArchive(mount_point) {}

    std::string GetName() const override {
        return "SDMCWriteOnlyArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override;

    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) const override;
};

/// File system interface to the SDMC write-only archive
class ArchiveFactory_SDMCWriteOnly final : public ArchiveFactory {
public:
    ArchiveFactory_SDMCWriteOnly(const std::string& mount_point);

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    std::string GetName() const override {
        return "SDMCWriteOnly";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    std::string sdmc_directory;
};

} // namespace FileSys
