// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/math_util.h"

#include "core/file_sys/archive.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/directory.h"
#include "core/hle/service/service.h"
#include "core/hle/kernel/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

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

class Archive : public Object {
public:
    std::string GetTypeName() const { return "Archive"; }
    std::string GetName() const { return name; }

    static Kernel::HandleType GetStaticHandleType() { return HandleType::Archive; }
    Kernel::HandleType GetHandleType() const { return HandleType::Archive; }

    std::string name;           ///< Name of archive (optional)
    FileSys::Archive* backend;  ///< Archive backend interface

    /**
     * Synchronize kernel object 
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        u32* cmd_buff = Service::GetCommandBuffer();
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
            DEBUG_LOG(KERNEL, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            Kernel::g_object_pool.Destroy<Archive>(GetHandle());
            CloseArchive(backend->GetIdCode());
            break;
        }
        // Unknown command...
        default:
        {
            ERROR_LOG(KERNEL, "Unknown command=0x%08X!", cmd);
            return -1;
        }
        }
        cmd_buff[1] = 0; // No error
        return 0;
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED)");
        return 0;
    }
};

class File : public Object {
public:
    std::string GetTypeName() const { return "File"; }
    std::string GetName() const { return path; }

    static Kernel::HandleType GetStaticHandleType() { return HandleType::File; }
    Kernel::HandleType GetHandleType() const { return HandleType::File; }

    std::string path; ///< Path of the file
    std::unique_ptr<FileSys::File> backend; ///< File backend interface

    /**
     * Synchronize kernel object
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        u32* cmd_buff = Service::GetCommandBuffer();
        FileCommand cmd = static_cast<FileCommand>(cmd_buff[0]);
        switch (cmd) {

        // Read from file...
        case FileCommand::Read:
        {
            u64 offset = cmd_buff[1] | ((u64) cmd_buff[2]) << 32;
            u32 length  = cmd_buff[3];
            u32 address = cmd_buff[5];
            DEBUG_LOG(KERNEL, "Read %s %s: offset=0x%x length=%d address=0x%x",
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
            DEBUG_LOG(KERNEL, "Write %s %s: offset=0x%x length=%d address=0x%x, flush=0x%x",
                      GetTypeName().c_str(), GetName().c_str(), offset, length, address, flush);
            cmd_buff[2] = backend->Write(offset, length, flush, Memory::GetPointer(address));
            break;
        }

        case FileCommand::GetSize:
        {
            DEBUG_LOG(KERNEL, "GetSize %s %s", GetTypeName().c_str(), GetName().c_str());
            u64 size = backend->GetSize();
            cmd_buff[2] = (u32)size;
            cmd_buff[3] = size >> 32;
            break;
        }

        case FileCommand::SetSize:
        {
            u64 size = cmd_buff[1] | ((u64)cmd_buff[2] << 32);
            DEBUG_LOG(KERNEL, "SetSize %s %s size=%d", GetTypeName().c_str(), GetName().c_str(), size);
            backend->SetSize(size);
            break;
        }

        case FileCommand::Close:
        {
            DEBUG_LOG(KERNEL, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            Kernel::g_object_pool.Destroy<File>(GetHandle());
            break;
        }

        // Unknown command...
        default:
            ERROR_LOG(KERNEL, "Unknown command=0x%08X!", cmd);
            cmd_buff[1] = -1; // TODO(Link Mauve): use the correct error code for that.
            return -1;
        }
        cmd_buff[1] = 0; // No error
        return 0;
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED)");
        return 0;
    }
};

class Directory : public Object {
public:
    std::string GetTypeName() const { return "Directory"; }
    std::string GetName() const { return path; }

    static Kernel::HandleType GetStaticHandleType() { return HandleType::Directory; }
    Kernel::HandleType GetHandleType() const { return HandleType::Directory; }

    std::string path; ///< Path of the directory
    std::unique_ptr<FileSys::Directory> backend; ///< File backend interface

    /**
     * Synchronize kernel object
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        u32* cmd_buff = Service::GetCommandBuffer();
        DirectoryCommand cmd = static_cast<DirectoryCommand>(cmd_buff[0]);
        switch (cmd) {

        // Read from directory...
        case DirectoryCommand::Read:
        {
            u32 count = cmd_buff[1];
            u32 address = cmd_buff[3];
            FileSys::Entry* entries = reinterpret_cast<FileSys::Entry*>(Memory::GetPointer(address));
            DEBUG_LOG(KERNEL, "Read %s %s: count=%d", GetTypeName().c_str(), GetName().c_str(), count);

            // Number of entries actually read
            cmd_buff[2] = backend->Read(count, entries);
            break;
        }

        case DirectoryCommand::Close:
        {
            DEBUG_LOG(KERNEL, "Close %s %s", GetTypeName().c_str(), GetName().c_str());
            Kernel::g_object_pool.Destroy<Directory>(GetHandle());
            break;
        }

        // Unknown command...
        default:
            ERROR_LOG(KERNEL, "Unknown command=0x%08X!", cmd);
            cmd_buff[1] = -1; // TODO(Link Mauve): use the correct error code for that.
            return -1;
        }
        cmd_buff[1] = 0; // No error
        return 0;
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED)");
        return 0;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

std::map<FileSys::Archive::IdCode, Handle> g_archive_map; ///< Map of file archives by IdCode

/**
 * Opens an archive
 * @param id_code IdCode of the archive to open
 * @return Handle to archive if it exists, otherwise a null handle (0)
 */
Handle OpenArchive(FileSys::Archive::IdCode id_code) {
    auto itr = g_archive_map.find(id_code);
    if (itr == g_archive_map.end()) {
        return 0;
    }
    return itr->second;
}

/**
 * Closes an archive
 * @param id_code IdCode of the archive to open
 * @return Result of operation, 0 on success, otherwise error code
 */
Result CloseArchive(FileSys::Archive::IdCode id_code) {
    if (1 != g_archive_map.erase(id_code)) {
        ERROR_LOG(KERNEL, "Cannot close archive %d", (int) id_code);
        return -1;
    }

    INFO_LOG(KERNEL, "Closed archive %d", (int) id_code);
    return 0;
}

/**
 * Mounts an archive
 * @param archive Pointer to the archive to mount
 * @return Result of operation, 0 on success, otherwise error code
 */
Result MountArchive(Archive* archive) {
    FileSys::Archive::IdCode id_code = archive->backend->GetIdCode();
    if (0 != OpenArchive(id_code)) {
        ERROR_LOG(KERNEL, "Cannot mount two archives with the same ID code! (%d)", (int) id_code);
        return -1;
    }
    g_archive_map[id_code] = archive->GetHandle();
    INFO_LOG(KERNEL, "Mounted archive %s", archive->GetName().c_str());
    return 0;
}

/**
 * Creates an Archive
 * @param handle Handle to newly created archive object
 * @param backend File system backend interface to the archive
 * @param name Optional name of Archive
 * @return Newly created Archive object
 */
Archive* CreateArchive(Handle& handle, FileSys::Archive* backend, const std::string& name) {
    Archive* archive = new Archive;
    handle = Kernel::g_object_pool.Create(archive);
    archive->name = name;
    archive->backend = backend;

    MountArchive(archive);
    
    return archive;
}

/**
 * Creates an Archive
 * @param backend File system backend interface to the archive
 * @param name Optional name of Archive
 * @return Handle to newly created Archive object
 */
Handle CreateArchive(FileSys::Archive* backend, const std::string& name) {
    Handle handle;
    CreateArchive(handle, backend, name);
    return handle;
}

/**
 * Open a File from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the File inside of the Archive
 * @param mode Mode under which to open the File
 * @return Opened File object
 */
Handle OpenFileFromArchive(Handle archive_handle, const std::string& path, const FileSys::Mode mode) {
    File* file = new File;
    Handle handle = Kernel::g_object_pool.Create(file);

    Archive* archive = Kernel::g_object_pool.GetFast<Archive>(archive_handle);
    file->path = path;
    file->backend = archive->backend->OpenFile(path, mode);

    if (!file->backend)
        return 0;

    return handle;
}

/**
 * Open a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Opened Directory object
 */
Handle OpenDirectoryFromArchive(Handle archive_handle, const std::string& path) {
    Directory* directory = new Directory;
    Handle handle = Kernel::g_object_pool.Create(directory);

    Archive* archive = Kernel::g_object_pool.GetFast<Archive>(archive_handle);
    directory->path = path;
    directory->backend = archive->backend->OpenDirectory(path);

    return handle;
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
        CreateArchive(archive, "SDMC");
    else
        ERROR_LOG(KERNEL, "Can't instantiate SDMC archive with path %s", sdmc_directory.c_str());
}

/// Shutdown archives
void ArchiveShutdown() {
    g_archive_map.clear();
}

} // namespace Kernel
