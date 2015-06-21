// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"

#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the SystemSaveData archive
class ArchiveFactory_SystemSaveData final : public ArchiveFactory {
public:
    ArchiveFactory_SystemSaveData(const std::string& mount_point);

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path) override;

    std::string GetName() const override { return "SystemSaveData"; }

private:
    std::string base_path;
};

/**
 * Constructs a path to the concrete SystemSaveData archive in the host filesystem based on the
 * input Path and base mount point.
 * @param mount_point The base mount point of the SystemSaveData archives.
 * @param path The path that identifies the requested concrete SystemSaveData archive.
 * @returns The complete path to the specified SystemSaveData archive in the host filesystem
 */
std::string GetSystemSaveDataPath(const std::string& mount_point, const Path& path);

/**
 * Constructs a path to the base folder to hold concrete SystemSaveData archives in the host file system.
 * @param mount_point The base folder where this folder resides, ie. SDMC or NAND.
 * @returns The path to the base SystemSaveData archives' folder in the host file system
 */
std::string GetSystemSaveDataContainerPath(const std::string& mount_point);

/**
 * Constructs a FileSys::Path object that refers to the SystemSaveData archive identified by
 * the specified high save id and low save id.
 * @param high The high word of the save id for the archive
 * @param low The low word of the save id for the archive
 * @returns A FileSys::Path to the wanted archive
 */
Path ConstructSystemSaveDataBinaryPath(u32 high, u32 low);

} // namespace FileSys
