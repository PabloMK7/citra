// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <string>
#include "common/common_types.h"
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"

namespace FileSys {

enum class ExtSaveDataType {
    Normal, ///< Regular non-shared ext save data
    Shared, ///< Shared ext save data
    Boss,   ///< SpotPass ext save data
};

/// File system interface to the ExtSaveData archive
class ArchiveFactory_ExtSaveData final : public ArchiveFactory {
public:
    ArchiveFactory_ExtSaveData(const std::string& mount_point, ExtSaveDataType type_);

    std::string GetName() const override {
        return "ExtSaveData";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info,
                  u64 program_id) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

    const std::string& GetMountPoint() const {
        return mount_point;
    }

    /**
     * Writes the SMDH icon of the ExtSaveData to file
     * @param path Path of this ExtSaveData
     * @param icon_data Binary data of the icon
     * @param icon_size Size of the icon data
     */
    void WriteIcon(const Path& path, std::span<const u8> icon);

private:
    /// Type of ext save data archive being accessed.
    ExtSaveDataType type;

    /**
     * This holds the full directory path for this archive, it is only set after a successful call
     * to Open, this is formed as `<base extsavedatapath>/<type>/<high>/<low>`.
     * See GetExtSaveDataPath for the code that extracts this data from an archive path.
     */
    std::string mount_point;

    /// Returns a path with the correct SaveIdHigh value for Shared extdata paths.
    Path GetCorrectedPath(const Path& path);
};

/**
 * Constructs a path to the concrete ExtData archive in the host filesystem based on the
 * input Path and base mount point.
 * @param mount_point The base mount point of the ExtSaveData archives.
 * @param path The path that identifies the requested concrete ExtSaveData archive.
 * @returns The complete path to the specified extdata archive in the host filesystem
 */
std::string GetExtSaveDataPath(std::string_view mount_point, const Path& path);

/**
 * Constructs a path to the concrete ExtData archive in the host filesystem based on the
 * extdata ID and base mount point.
 * @param mount_point The base mount point of the ExtSaveData archives.
 * @param extdata_id The id of the ExtSaveData
 * @returns The complete path to the specified extdata archive in the host filesystem
 */
std::string GetExtDataPathFromId(std::string_view mount_point, u64 extdata_id);

/**
 * Constructs a path to the base folder to hold concrete ExtSaveData archives in the host file
 * system.
 * @param mount_point The base folder where this folder resides, ie. SDMC or NAND.
 * @param shared Whether this ExtSaveData container is for SharedExtSaveDatas or not.
 * @returns The path to the base ExtSaveData archives' folder in the host file system
 */
std::string GetExtDataContainerPath(std::string_view mount_point, bool shared);

/**
 * Constructs a FileSys::Path object that refers to the ExtData archive identified by
 * the specified media type, high save id and low save id.
 * @param media_type The media type where the archive is located (NAND / SDMC)
 * @param high The high word of the save id for the archive
 * @param low The low word of the save id for the archive
 * @returns A FileSys::Path to the wanted archive
 */
Path ConstructExtDataBinaryPath(u32 media_type, u32 high, u32 low);

class ExtSaveDataDelayGenerator;

} // namespace FileSys
