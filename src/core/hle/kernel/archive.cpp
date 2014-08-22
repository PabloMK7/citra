// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/math_util.h"

#include "core/file_sys/archive.h"
#include "core/hle/service/service.h"
#include "core/hle/kernel/kernel.h"
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

/// Initialize archives
void ArchiveInit() {
    g_archive_map.clear();
}

/// Shutdown archives
void ArchiveShutdown() {
    g_archive_map.clear();
}

} // namespace Kernel
