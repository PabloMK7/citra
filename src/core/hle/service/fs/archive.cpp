// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>
#include <unordered_map>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/math_util.h"

#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/directory_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/kernel/session.h"
#include "core/hle/result.h"

// Specializes std::hash for ArchiveIdCode, so that we can use it in std::unordered_map.
// Workaroung for libstdc++ bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60970
namespace std {
    template <>
    struct hash<Service::FS::ArchiveIdCode> {
        typedef Service::FS::ArchiveIdCode argument_type;
        typedef std::size_t result_type;

        result_type operator()(const argument_type& id_code) const {
            typedef std::underlying_type<argument_type>::type Type;
            return std::hash<Type>()(static_cast<Type>(id_code));
        }
    };
}

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

class Archive {
public:
    Archive(std::unique_ptr<FileSys::ArchiveBackend>&& backend, ArchiveIdCode id_code)
            : backend(std::move(backend)), id_code(id_code) {
    }

    std::string GetName() const { return "Archive: " + backend->GetName(); }

    ArchiveIdCode id_code; ///< Id code of the archive
    std::unique_ptr<FileSys::ArchiveBackend> backend; ///< Archive backend interface
};

class File : public Kernel::Session {
public:
    File(std::unique_ptr<FileSys::FileBackend>&& backend, const FileSys::Path& path)
            : backend(std::move(backend)), path(path) {
    }

    std::string GetName() const override { return "Path: " + path.DebugStr(); }

    FileSys::Path path; ///< Path of the file
    std::unique_ptr<FileSys::FileBackend> backend; ///< File backend interface

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

        case FileCommand::Flush:
        {
            LOG_TRACE(Service_FS, "Flush");
            backend->Flush();
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
    Directory(std::unique_ptr<FileSys::DirectoryBackend>&& backend, const FileSys::Path& path)
            : backend(std::move(backend)), path(path) {
    }

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

/**
 * Map of registered archives, identified by id code. Once an archive is registered here, it is
 * never removed until the FS service is shut down.
 */
static std::unordered_map<ArchiveIdCode, std::unique_ptr<Archive>> id_code_map;

/**
 * Map of active archive handles. Values are pointers to the archives in `idcode_map`.
 */
static std::unordered_map<ArchiveHandle, Archive*> handle_map;
static ArchiveHandle next_handle;

static Archive* GetArchive(ArchiveHandle handle) {
    auto itr = handle_map.find(handle);
    return (itr == handle_map.end()) ? nullptr : itr->second;
}

ResultVal<ArchiveHandle> OpenArchive(ArchiveIdCode id_code) {
    LOG_TRACE(Service_FS, "Opening archive with id code 0x%08X", id_code);

    auto itr = id_code_map.find(id_code);
    if (itr == id_code_map.end()) {
        if (id_code == ArchiveIdCode::SaveData) {
            // When a SaveData archive is created for the first time, it is not yet formatted
            // and the save file/directory structure expected by the game has not yet been initialized. 
            // Returning the NotFormatted error code will signal the game to provision the SaveData archive 
            // with the files and folders that it expects. 
            // The FormatSaveData service call will create the SaveData archive when it is called.
            return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                              ErrorSummary::InvalidState, ErrorLevel::Status);
        }
        // TODO: Verify error against hardware
        return ResultCode(ErrorDescription::NotFound, ErrorModule::FS,
                          ErrorSummary::NotFound, ErrorLevel::Permanent);
    }

    // This should never even happen in the first place with 64-bit handles, 
    while (handle_map.count(next_handle) != 0) {
        ++next_handle;
    }
    handle_map.emplace(next_handle, itr->second.get());
    return MakeResult<ArchiveHandle>(next_handle++);
}

ResultCode CloseArchive(ArchiveHandle handle) {
    if (handle_map.erase(handle) == 0)
        return InvalidHandle(ErrorModule::FS);
    else
        return RESULT_SUCCESS;
}

// TODO(yuriks): This might be what the fs:REG service is for. See the Register/Unregister calls in
// http://3dbrew.org/wiki/Filesystem_services#ProgramRegistry_service_.22fs:REG.22
ResultCode CreateArchive(std::unique_ptr<FileSys::ArchiveBackend>&& backend, ArchiveIdCode id_code) {
    auto result = id_code_map.emplace(id_code, std::make_unique<Archive>(std::move(backend), id_code));

    bool inserted = result.second;
    _dbg_assert_msg_(Service_FS, inserted, "Tried to register more than one archive with same id code");

    auto& archive = result.first->second;
    LOG_DEBUG(Service_FS, "Registered archive %s with id code 0x%08X", archive->GetName().c_str(), id_code);
    return RESULT_SUCCESS;
}

ResultVal<Handle> OpenFileFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path, const FileSys::Mode mode) {
    Archive* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    std::unique_ptr<FileSys::FileBackend> backend = archive->backend->OpenFile(path, mode);
    if (backend == nullptr) {
        return ResultCode(ErrorDescription::FS_NotFound, ErrorModule::FS,
                          ErrorSummary::NotFound, ErrorLevel::Status);
    }

    auto file = std::make_unique<File>(std::move(backend), path);
    Handle handle = Kernel::g_object_pool.Create(file.release());
    return MakeResult<Handle>(handle);
}

ResultCode DeleteFileFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    Archive* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    if (archive->backend->DeleteFile(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode RenameFileBetweenArchives(ArchiveHandle src_archive_handle, const FileSys::Path& src_path,
                                     ArchiveHandle dest_archive_handle, const FileSys::Path& dest_path) {
    Archive* src_archive = GetArchive(src_archive_handle);
    Archive* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    if (src_archive == dest_archive) {
        if (src_archive->backend->RenameFile(src_path, dest_path))
            return RESULT_SUCCESS;
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

ResultCode DeleteDirectoryFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    Archive* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    if (archive->backend->DeleteDirectory(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode CreateDirectoryFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    Archive* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    if (archive->backend->CreateDirectory(path))
        return RESULT_SUCCESS;
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::Canceled, ErrorLevel::Status);
}

ResultCode RenameDirectoryBetweenArchives(ArchiveHandle src_archive_handle, const FileSys::Path& src_path,
                                          ArchiveHandle dest_archive_handle, const FileSys::Path& dest_path) {
    Archive* src_archive = GetArchive(src_archive_handle);
    Archive* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    if (src_archive == dest_archive) {
        if (src_archive->backend->RenameDirectory(src_path, dest_path))
            return RESULT_SUCCESS;
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

/**
 * Open a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Opened Directory object
 */
ResultVal<Handle> OpenDirectoryFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    Archive* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return InvalidHandle(ErrorModule::FS);

    std::unique_ptr<FileSys::DirectoryBackend> backend = archive->backend->OpenDirectory(path);
    if (backend == nullptr) {
        return ResultCode(ErrorDescription::NotFound, ErrorModule::FS,
                          ErrorSummary::NotFound, ErrorLevel::Permanent);
    }

    auto directory = std::make_unique<Directory>(std::move(backend), path);
    Handle handle = Kernel::g_object_pool.Create(directory.release());
    return MakeResult<Handle>(handle);
}

ResultCode FormatSaveData() {
    // TODO(Subv): Actually wipe the savedata folder after creating or opening it

    // Do not create the archive again if it already exists
    if (id_code_map.find(ArchiveIdCode::SaveData) != id_code_map.end())
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the correct error code

    // Create the SaveData archive
    std::string savedata_directory = FileUtil::GetUserPath(D_SAVEDATA_IDX);
    auto savedata_archive = std::make_unique<FileSys::Archive_SaveData>(savedata_directory,
        Kernel::g_program_id);

    if (savedata_archive->Initialize()) {
        CreateArchive(std::move(savedata_archive), ArchiveIdCode::SaveData);
        return RESULT_SUCCESS;
    } else {
        LOG_ERROR(Service_FS, "Can't instantiate SaveData archive with path %s",
            savedata_archive->GetMountPoint().c_str());
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the proper error code
    }
}

/// Initialize archives
void ArchiveInit() {
    next_handle = 1;

    // TODO(Link Mauve): Add the other archive types (see here for the known types:
    // http://3dbrew.org/wiki/FS:OpenArchive#Archive_idcodes).  Currently the only half-finished
    // archive type is SDMC, so it is the only one getting exposed.

    std::string sdmc_directory = FileUtil::GetUserPath(D_SDMC_IDX);
    auto sdmc_archive = std::make_unique<FileSys::Archive_SDMC>(sdmc_directory);
    if (sdmc_archive->Initialize())
        CreateArchive(std::move(sdmc_archive), ArchiveIdCode::SDMC);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path %s", sdmc_directory.c_str());
}

/// Shutdown archives
void ArchiveShutdown() {
    handle_map.clear();
    id_code_map.clear();
}

} // namespace FS
} // namespace Service
