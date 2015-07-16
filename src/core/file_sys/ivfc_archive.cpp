// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/make_unique.h"

#include "core/file_sys/ivfc_archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::string IVFCArchive::GetName() const {
    return "IVFC";
}

std::unique_ptr<FileBackend> IVFCArchive::OpenFile(const Path& path, const Mode mode) const {
    return Common::make_unique<IVFCFile>(romfs_file, data_offset, data_size);
}

bool IVFCArchive::DeleteFile(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a file from an IVFC archive (%s).", GetName().c_str());
    return false;
}

bool IVFCArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive (%s).", GetName().c_str());
    return false;
}

bool IVFCArchive::DeleteDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an IVFC archive (%s).", GetName().c_str());
    return false;
}

ResultCode IVFCArchive::CreateFile(const Path& path, u32 size) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a file in an IVFC archive (%s).", GetName().c_str());
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported, ErrorLevel::Permanent);
}

bool IVFCArchive::CreateDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a directory in an IVFC archive (%s).", GetName().c_str());
    return false;
}

bool IVFCArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive (%s).", GetName().c_str());
    return false;
}

std::unique_ptr<DirectoryBackend> IVFCArchive::OpenDirectory(const Path& path) const {
    return Common::make_unique<IVFCDirectory>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

size_t IVFCFile::Read(const u64 offset, const size_t length, u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset=%llu, length=%d", offset, length);
    romfs_file->Seek(data_offset + offset, SEEK_SET);
    size_t read_length = (size_t)std::min((u64)length, data_size - offset);

    return romfs_file->ReadBytes(buffer, read_length);
}

size_t IVFCFile::Write(const u64 offset, const size_t length, const bool flush, const u8* buffer) const {
    LOG_ERROR(Service_FS, "Attempted to write to IVFC file");
    return 0;
}

u64 IVFCFile::GetSize() const {
    return data_size;
}

bool IVFCFile::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an IVFC file");
    return false;
}

} // namespace FileSys
