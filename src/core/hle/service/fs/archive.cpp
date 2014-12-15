// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/math_util.h"

#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/directory_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/kernel/session.h"
#include "core/hle/result.h"

namespace Service {
namespace FS {

// Command to access archive file
enum class FileCommand : u32 {
    Dummy1          = 0x000100C6,
    Control         = 0x040100C4,
    OpenSubFile     = 0x08010100,
    Read            = 0x080200C2,
    Write           = 0x08030102,
    GetSize         = 0x08040000,
    SetSize         = 0x08050080,
    GetAttributes   = 0x08060000,
    SetAttributes   = 0x08070040,
    Close           = 0x08080000,
    Flush           = 0x08090000,
};

// Command to access directory
enum class DirectoryCommand : u32 {
    Dummy1          = 0x000100C6,
    Control         = 0x040100C4,
    Read            = 0x08010042,
    Close           = 0x08020000,
};

class Archive : public Kernel::Session {
public:
    std::string GetName() const override { return "Archive: " + backend->GetName(); }

    ArchiveIdCode id_code;      ///< Id code of the archive
    FileSys::ArchiveBackend* backend;  ///< Archive backend interface

    ResultVal<bool> SyncRequest() override {
        u32* cmd_buff = Kernel::GetCommandBuffer();
        FileCommand cmd = static_cast<FileCommand>(cmd_buff[0]);

        switch (cmd) {
        // Read from archive...
        case FileCommand::Read:
        {
            u64 offset  = cmd_buff[1] | ((u64)cmd_buff[2] << 32);
            u32 length  = cmd_buff[3];
            u32 address = cmd_buff[5];

            // Number of bytes read
            cmd_buff[2] = backend->Read(offset, length, Memory::GetPointer(address));
            break;
        }
        // Write to archive...
        case FileCommand::Write:
        {
            u64 offset  = cmd_buff[1] | ((u64)cmd_buff[2] << 32);
            u32 length  = cmd_buff[3];
            u32 flush   = cmd_buff[4];
            u32 address = cmd_buff[6];

            // Number of bytes written
            cmd_buff[2] = backend->Write(offset, length, flush, Memory::GetPointer(address));
            break;
        }
        case FileCommand::GetSize:
        {
            u64 filesize = (u64) backend->GetSize();
            cmd_buff[2]  = (u32) filesize;         // Lower word
            cmd_buff[3]  = (u32) (filesize >> 32); // Upper word
            break;
        }
        case FileCommand::SetSize:
        {
            backend->SetSize(cmd_buff[1] | ((u64)cmd_buff[2] << 32));
            break;
        }
        case FileCommand::Close:
        {
            LOG_TRACE(Service_FS, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            CloseArchive(id_code);
            break;
        }
        // Unknown command...
        default:
        {
            LOG_ERROR(Service_FS, "Unknown command=0x%08X", cmd);
            cmd_buff[0] = UnimplementedFunction(ErrorModule::FS).raw;
            return MakeResult<bool>(false);
        }
        }
        cmd_buff[1] = 0; // No error
        return MakeResult<bool>(false);
    }
};

class File : public Kernel::Session {
public:
    std::string GetName() const override { return "Path: " + path.DebugStr(); }

    FileSys::Path path; ///< Path of the file
    std::unique_ptr<FileSys::File> backend; ///< File backend interface

    ResultVal<bool> SyncRequest() override {
        u32* cmd_buff = Kernel::GetCommandBuffer();
        FileCommand cmd = static_cast<FileCommand>(cmd_buff[0]);
        switch (cmd) {

        // Read from file...
        case FileCommand::Read:
        {
            u64 offset = cmd_buff[1] | ((u64) cmd_buff[2]) << 32;
            u32 length  = cmd_buff[3];
            u32 address = cmd_buff[5];
            LOG_TRACE(Service_FS, "Read %s %s: offset=0x%llx length=%d address=0x%x",
                      GetTypeName().c_str(), GetName().c_str(), offset, length, address);
            cmd_buff[2] = backend->Read(offset, length, Memory::GetPointer(address));
            break;
        }

        // Write to file...
        case FileCommand::Write:
        {
            u64 offset  = cmd_buff[1] | ((u64) cmd_buff[2]) << 32;
            u32 length  = cmd_buff[3];
            u32 flush   = cmd_buff[4];
            u32 address = cmd_buff[6];
            LOG_TRACE(Service_FS, "Write %s %s: offset=0x%llx length=%d address=0x%x, flush=0x%x",
                      GetTypeName().c_str(), GetName().c_str(), offset, length, address, flush);
            cmd_buff[2] = backend->Write(offset, length, flush, Memory::GetPointer(address));
            break;
        }

        case FileCommand::GetSize:
        {
            LOG_TRACE(Service_FS, "GetSize %s %s", GetTypeName().c_str(), GetName().c_str());
            u64 size = backend->GetSize();
            cmd_buff[2] = (u32)size;
            cmd_buff[3] = size >> 32;
            break;
        }

        case FileCommand::SetSize:
        {
            u64 size = cmd_buff[1] | ((u64)cmd_buff[2] << 32);
            LOG_TRACE(Service_FS, "SetSize %s %s size=%llu",
                    GetTypeName().c_str(), GetName().c_str(), size);
            backend->SetSize(size);
            break;
        }

        case FileCommand::Close:
        {
            LOG_TRACE(Service_FS, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            Kernel::g_object_pool.Destroy<File>(GetHandle());
            break;
        }

        // Unknown command...
        default:
            LOG_ERROR(Service_FS, "Unknown command=0x%08X!", cmd);
            ResultCode error = UnimplementedFunction(ErrorModule::FS);
            cmd_buff[1] = error.raw; // TODO(Link Mauve): use the correct error code for that.
            return error;
        }
        cmd_buff[1] = 0; // No error
        return MakeResult<bool>(false);
    }
};

class Directory : public Kernel::Session {
public:
    std::string GetName() const override { return "Directory: " + path.DebugStr(); }

    FileSys::Path path; ///< Path of the directory
    std::unique_ptr<FileSys::DirectoryBackend> backend; ///< File backend interface

    ResultVal<bool> SyncRequest() override {
        u32* cmd_buff = Kernel::GetCommandBuffer();
        DirectoryCommand cmd = static_cast<DirectoryCommand>(cmd_buff[0]);
        switch (cmd) {

        // Read from directory...
        case DirectoryCommand::Read:
        {
            u32 count = cmd_buff[1];
            u32 address = cmd_buff[3];
            auto entries = reinterpret_cast<FileSys::Entry*>(Memory::GetPointer(address));
            LOG_TRACE(Service_FS, "Read %s %s: count=%d",
                    GetTypeName().c_str(), GetName().c_str(), count);

            // Number of entries actually read
            cmd_buff[2] = backend->Read(count, entries);
            break;
        }

        case DirectoryCommand::Close:
        {
            LOG_TRACE(Service_FS, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            Kernel::g_object_pool.Destroy<Directory>(GetHandle());
            break;
        }

        // Unknown command...
        default:
            LOG_ERROR(Service_FS, "Unknown command=0x%08X!", cmd);
            ResultCode error = UnimplementedFunction(ErrorModule::FS);
            cmd_buff[1] = error.raw; // TODO(Link Mauve): use the correct error code for that.
            return MakeResult<bool>(false);
        }
        cmd_buff[1] = 0; // No error
        return MakeResult<bool>(false);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

std::map<ArchiveIdCode, Handle> g_archive_map; ///< Map of file archives by IdCode

ResultVal<Handle> OpenArchive(ArchiveIdCode id_code) {
    auto itr = g_archive_map.find(id_code);
    if (itr == g_archive_map.end()) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::FS,
                ErrorSummary::NotFound, ErrorLevel::Permanent);
    }

    return MakeResult<Handle>(itr->second);
}

ResultCode CloseArchive(ArchiveIdCode id_code) {
    auto itr = g_archive_map.find(id_code);
    if (itr == g_archive_map.end()) {
        LOG_ERROR(Service_FS, "Cannot close archive %d, does not exist!", (int)id_code);
        return InvalidHandle(ErrorModule::FS);
    }

    LOG_TRACE(Service_FS, "Closed archive %d", (int) id_code);
    return RESULT_SUCCESS;
}

/**
 * Mounts an archive
 * @param archive Pointer to the archive to mount
 */
ResultCode MountArchive(Archive* archive) {
    ArchiveIdCode id_code = archive->id_code;
    ResultVal<Handle> archive_handle = OpenArchive(id_code);
    if (archive_handle.Succeeded()) {
        LOG_ERROR(Service_FS, "Cannot mount two archives with the same ID code! (%d)", (int) id_code);
        return archive_handle.Code();
    }
    g_archive_map[id_code] = archive->GetHandle();
    LOG_TRACE(Service_FS, "Mounted archive %s", archive->GetName().c_str());
    return RESULT_SUCCESS;
}

ResultCode CreateArchive(FileSys::ArchiveBackend* backend, ArchiveIdCode id_code) {
    Archive* archive = new Archive;
    Handle handle = Kernel::g_object_pool.Create(archive);
    archive->id_code = id_code;
    archive->backend = backend;

    ResultCode result = MountArchive(archive);
    if (result.IsError()) {
        return result;
    }
    
    return RESULT_SUCCESS;
}

ResultVal<Handle> OpenFileFromArchive(Handle archive_handle, const FileSys::Path& path, const FileSys::Mode mode) {
    // TODO(bunnei): Binary type files get a raw file pointer to the archive. Currently, we create
    // the archive file handles at app loading, and then keep them persistent throughout execution.
    // Archives file handles are just reused and not actually freed until emulation shut down.
    // Verify if real hardware works this way, or if new handles are created each time
    if (path.GetType() == FileSys::Binary)
        // TODO(bunnei): FixMe - this is a hack to compensate for an incorrect FileSys backend
        // design. While the functionally of this is OK, our implementation decision to separate
        // normal files from archive file pointers is very likely wrong.
        // See https://github.com/citra-emu/citra/issues/205
        return MakeResult<Handle>(archive_handle);

    File* file = new File;
    Handle handle = Kernel::g_object_pool.Create(file);

    Archive* archive = Kernel::g_object_pool.Get<Archive>(archive_handle);
    if (archive == nullptr) {
        return InvalidHandle(ErrorModule::FS);
    }
    file->path = path;
    file->backend = archive->backend->OpenFile(path, mode);

    if (!file->backend) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::FS,
                ErrorSummary::NotFound, ErrorLevel::Permanent);
    }

    return MakeResult<Handle>(handle);
}

ResultCode DeleteFileFromArchive(Handle archive_handle, const FileSys::Path& path) {
    Archive* archive = Kernel::g_object_pool.GetFast<Archive>(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);
    if (archive->backend->DeleteFile(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode RenameFileBetweenArchives(Handle src_archive_handle, const FileSys::Path& src_path,
                                     Handle dest_archive_handle, const FileSys::Path& dest_path) {
    Archive* src_archive = Kernel::g_object_pool.GetFast<Archive>(src_archive_handle);
    Archive* dest_archive = Kernel::g_object_pool.GetFast<Archive>(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return InvalidHandle(ErrorModule::FS);
    if (src_archive == dest_archive) {
        if (src_archive->backend->RenameFile(src_path, dest_path))
            return RESULT_SUCCESS;
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

ResultCode DeleteDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path) {
    Archive* archive = Kernel::g_object_pool.GetFast<Archive>(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);
    if (archive->backend->DeleteDirectory(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode CreateDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path) {
    Archive* archive = Kernel::g_object_pool.GetFast<Archive>(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);
    if (archive->backend->CreateDirectory(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode RenameDirectoryBetweenArchives(Handle src_archive_handle, const FileSys::Path& src_path,
                                          Handle dest_archive_handle, const FileSys::Path& dest_path) {
    Archive* src_archive = Kernel::g_object_pool.GetFast<Archive>(src_archive_handle);
    Archive* dest_archive = Kernel::g_object_pool.GetFast<Archive>(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return InvalidHandle(ErrorModule::FS);
    if (src_archive == dest_archive) {
        if (src_archive->backend->RenameDirectory(src_path, dest_path))
            return RESULT_SUCCESS;
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

/**
 * Open a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Opened Directory object
 */
ResultVal<Handle> OpenDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path) {
    Directory* directory = new Directory;
    Handle handle = Kernel::g_object_pool.Create(directory);

    Archive* archive = Kernel::g_object_pool.Get<Archive>(archive_handle);
    if (archive == nullptr) {
        return InvalidHandle(ErrorModule::FS);
    }
    directory->path = path;
    directory->backend = archive->backend->OpenDirectory(path);

    if (!directory->backend) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::FS,
                          ErrorSummary::NotFound, ErrorLevel::Permanent);
    }

    return MakeResult<Handle>(handle);
}

/// Initialize archives
void ArchiveInit() {
    g_archive_map.clear();

    // TODO(Link Mauve): Add the other archive types (see here for the known types:
    // http://3dbrew.org/wiki/FS:OpenArchive#Archive_idcodes).  Currently the only half-finished
    // archive type is SDMC, so it is the only one getting exposed.

    std::string sdmc_directory = FileUtil::GetUserPath(D_SDMC_IDX);
    auto archive = new FileSys::Archive_SDMC(sdmc_directory);
    if (archive->Initialize())
        CreateArchive(archive, ArchiveIdCode::SDMC);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path %s", sdmc_directory.c_str());
}

/// Shutdown archives
void ArchiveShutdown() {
    g_archive_map.clear();
}

} // namespace FS
} // namespace Service
