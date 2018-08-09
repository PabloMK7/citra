// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace Service {
namespace FS {
enum class MediaType : u32;
}
} // namespace Service

namespace FileSys {

enum class NCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1,
    ExeFS = 2,
};

enum class NCCHFileOpenType : u32 {
    NCCHData = 0,
    SaveData = 1,
};

/// Helper function to generate a Path for NCCH archives
Path MakeNCCHArchivePath(u64 tid, Service::FS::MediaType media_type);

/// Helper function to generate a Path for NCCH files
Path MakeNCCHFilePath(NCCHFileOpenType open_type, u32 content_index, NCCHFilePathType filepath_type,
                      std::array<char, 8>& exefs_filepath);

/// Archive backend for NCCH Archives (RomFS, ExeFS)
class NCCHArchive : public ArchiveBackend {
public:
    explicit NCCHArchive(u64 title_id, Service::FS::MediaType media_type)
        : title_id(title_id), media_type(media_type) {}

    std::string GetName() const override {
        return "NCCHArchive";
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override;
    ResultCode DeleteFile(const Path& path) const override;
    ResultCode RenameFile(const Path& src_path, const Path& dest_path) const override;
    ResultCode DeleteDirectory(const Path& path) const override;
    ResultCode DeleteDirectoryRecursively(const Path& path) const override;
    ResultCode CreateFile(const Path& path, u64 size) const override;
    ResultCode CreateDirectory(const Path& path) const override;
    ResultCode RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) const override;
    u64 GetFreeBytes() const override;

protected:
    u64 title_id;
    Service::FS::MediaType media_type;
};

// File backend for NCCH files
class NCCHFile : public FileBackend {
public:
    explicit NCCHFile(std::vector<u8> buffer, std::unique_ptr<DelayGenerator> delay_generator_);

    ResultVal<size_t> Read(u64 offset, size_t length, u8* buffer) const override;
    ResultVal<size_t> Write(u64 offset, size_t length, bool flush, const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override {
        return false;
    }
    void Flush() const override {}

private:
    std::vector<u8> file_buffer;
    u64 data_offset;
    u64 data_size;
};

/// File system interface to the NCCH archive
class ArchiveFactory_NCCH final : public ArchiveFactory {
public:
    explicit ArchiveFactory_NCCH();

    std::string GetName() const override {
        return "NCCH";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;
};

} // namespace FileSys
