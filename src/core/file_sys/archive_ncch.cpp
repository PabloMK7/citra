// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ivfc_archive.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

enum class NCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1,
    ExeFS = 2,
};

struct NCCHArchivePath {
    u64_le tid;
    u32_le media_type;
    u32_le unknown;
};
static_assert(sizeof(NCCHArchivePath) == 0x10, "NCCHArchivePath has wrong size!");

struct NCCHFilePath {
    u32_le open_type;
    u32_le content_index;
    u32_le filepath_type;
    std::array<char, 8> exefs_filepath;
};
static_assert(sizeof(NCCHFilePath) == 0x14, "NCCHFilePath has wrong size!");

ResultVal<std::unique_ptr<FileBackend>> NCCHArchive::OpenFile(const Path& path,
                                                              const Mode& mode) const {
    if (path.GetType() != LowPathType::Binary) {
        LOG_ERROR(Service_FS, "Path need to be Binary");
        return ERROR_INVALID_PATH;
    }

    std::vector<u8> binary = path.AsBinary();
    if (binary.size() != sizeof(NCCHFilePath)) {
        LOG_ERROR(Service_FS, "Wrong path size %zu", binary.size());
        return ERROR_INVALID_PATH;
    }

    NCCHFilePath openfile_path;
    std::memcpy(&openfile_path, binary.data(), sizeof(NCCHFilePath));

    std::string file_path =
        Service::AM::GetTitleContentPath(media_type, title_id, openfile_path.content_index);
    auto ncch_container = NCCHContainer(file_path);

    Loader::ResultStatus result;
    std::unique_ptr<FileBackend> file;

    // NCCH RomFS
    NCCHFilePathType filepath_type = static_cast<NCCHFilePathType>(openfile_path.filepath_type);
    if (filepath_type == NCCHFilePathType::RomFS) {
        std::shared_ptr<FileUtil::IOFile> romfs_file;
        u64 romfs_offset = 0;
        u64 romfs_size = 0;

        result = ncch_container.ReadRomFS(romfs_file, romfs_offset, romfs_size);
        file = std::make_unique<IVFCFile>(std::move(romfs_file), romfs_offset, romfs_size);
    } else if (filepath_type == NCCHFilePathType::Code ||
               filepath_type == NCCHFilePathType::ExeFS) {
        std::vector<u8> buffer;

        // Load NCCH .code or icon/banner/logo
        result = ncch_container.LoadSectionExeFS(openfile_path.exefs_filepath.data(), buffer);
        file = std::make_unique<NCCHFile>(std::move(buffer));
    } else {
        LOG_ERROR(Service_FS, "Unknown NCCH archive type %u!", openfile_path.filepath_type);
        result = Loader::ResultStatus::Error;
    }

    if (result != Loader::ResultStatus::Success) {
        // High Title ID of the archive: The category (https://3dbrew.org/wiki/Title_list).
        constexpr u32 shared_data_archive = 0x0004009B;
        constexpr u32 system_data_archive = 0x000400DB;

        // Low Title IDs.
        constexpr u32 mii_data = 0x00010202;
        constexpr u32 region_manifest = 0x00010402;
        constexpr u32 ng_word_list = 0x00010302;

        u32 high = static_cast<u32>(title_id >> 32);
        u32 low = static_cast<u32>(title_id & 0xFFFFFFFF);

        LOG_DEBUG(Service_FS, "Full Path: %s. Category: 0x%X. Path: 0x%X.", path.DebugStr().c_str(),
                  high, low);

        std::string archive_name;
        if (high == shared_data_archive) {
            if (low == mii_data)
                archive_name = "Mii Data";
            else if (low == region_manifest)
                archive_name = "Region manifest";
        } else if (high == system_data_archive) {
            if (low == ng_word_list)
                archive_name = "NG bad word list";
        }

        if (!archive_name.empty()) {
            LOG_ERROR(Service_FS, "Failed to get a handle for shared data archive: %s. ",
                      archive_name.c_str());
            Core::System::GetInstance().SetStatus(Core::System::ResultStatus::ErrorSystemFiles,
                                                  archive_name.c_str());
        }
        return ERROR_NOT_FOUND;
    }

    return MakeResult<std::unique_ptr<FileBackend>>(std::move(file));
}

ResultCode NCCHArchive::DeleteFile(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a file from an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(Subv): Verify error code
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                      ErrorLevel::Status);
}

ResultCode NCCHArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::DeleteDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::DeleteDirectoryRecursively(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::CreateFile(const Path& path, u64 size) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a file in an NCCH archive (%s).",
                 GetName().c_str());
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultCode NCCHArchive::CreateDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a directory in an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode NCCHArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultVal<std::unique_ptr<DirectoryBackend>> NCCHArchive::OpenDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to open a directory within an NCCH archive (%s).",
                 GetName().c_str());
    // TODO(shinyquagsire23): Use correct error code
    return ResultCode(-1);
}

u64 NCCHArchive::GetFreeBytes() const {
    LOG_WARNING(Service_FS, "Attempted to get the free space in an NCCH archive");
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

NCCHFile::NCCHFile(std::vector<u8> buffer) : file_buffer(std::move(buffer)) {}

ResultVal<size_t> NCCHFile::Read(const u64 offset, const size_t length, u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset=%" PRIu64 ", length=%zu", offset, length);
    size_t length_left = static_cast<size_t>(data_size - offset);
    size_t read_length = static_cast<size_t>(std::min(length, length_left));

    size_t available_size = static_cast<size_t>(file_buffer.size() - offset);
    size_t copy_size = std::min(length, available_size);
    memcpy(buffer, file_buffer.data() + offset, copy_size);

    return MakeResult<size_t>(copy_size);
}

ResultVal<size_t> NCCHFile::Write(const u64 offset, const size_t length, const bool flush,
                                  const u8* buffer) {
    LOG_ERROR(Service_FS, "Attempted to write to NCCH file");
    // TODO(shinyquagsire23): Find error code
    return MakeResult<size_t>(0);
}

u64 NCCHFile::GetSize() const {
    return file_buffer.size();
}

bool NCCHFile::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an NCCH file");
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ArchiveFactory_NCCH::ArchiveFactory_NCCH() {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_NCCH::Open(const Path& path) {
    if (path.GetType() != LowPathType::Binary) {
        LOG_ERROR(Service_FS, "Path need to be Binary");
        return ERROR_INVALID_PATH;
    }

    std::vector<u8> binary = path.AsBinary();
    if (binary.size() != sizeof(NCCHArchivePath)) {
        LOG_ERROR(Service_FS, "Wrong path size %zu", binary.size());
        return ERROR_INVALID_PATH;
    }

    NCCHArchivePath open_path;
    std::memcpy(&open_path, binary.data(), sizeof(NCCHArchivePath));

    auto archive = std::make_unique<NCCHArchive>(
        open_path.tid, static_cast<Service::FS::MediaType>(open_path.media_type & 0xFF));
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_NCCH::Format(const Path& path,
                                       const FileSys::ArchiveFormatInfo& format_info) {
    LOG_ERROR(Service_FS, "Attempted to format a NCCH archive.");
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_NCCH::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive %s", GetName().c_str());
    return ResultCode(-1);
}

} // namespace FileSys
