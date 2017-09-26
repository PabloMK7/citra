// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <boost/container/flat_map.hpp>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/archive_other_savedata.h"
#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

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
} // namespace std

static constexpr Kernel::Handle INVALID_HANDLE{};

namespace Service {
namespace FS {

// Command to access archive file
enum class FileCommand : u32 {
    Dummy1 = 0x000100C6,
    Control = 0x040100C4,
    OpenSubFile = 0x08010100,
    Read = 0x080200C2,
    Write = 0x08030102,
    GetSize = 0x08040000,
    SetSize = 0x08050080,
    GetAttributes = 0x08060000,
    SetAttributes = 0x08070040,
    Close = 0x08080000,
    Flush = 0x08090000,
    SetPriority = 0x080A0040,
    GetPriority = 0x080B0000,
    OpenLinkFile = 0x080C0000,
};

// Command to access directory
enum class DirectoryCommand : u32 {
    Dummy1 = 0x000100C6,
    Control = 0x040100C4,
    Read = 0x08010042,
    Close = 0x08020000,
};

File::File(std::unique_ptr<FileSys::FileBackend>&& backend, const FileSys::Path& path)
    : path(path), priority(0), backend(std::move(backend)) {}

File::~File() {}

void File::HandleSyncRequest(Kernel::SharedPtr<Kernel::ServerSession> server_session) {
    using Kernel::ClientSession;
    using Kernel::ServerSession;
    using Kernel::SharedPtr;

    u32* cmd_buff = Kernel::GetCommandBuffer();
    FileCommand cmd = static_cast<FileCommand>(cmd_buff[0]);
    switch (cmd) {

    // Read from file...
    case FileCommand::Read: {
        u64 offset = cmd_buff[1] | ((u64)cmd_buff[2]) << 32;
        u32 length = cmd_buff[3];
        u32 address = cmd_buff[5];
        LOG_TRACE(Service_FS, "Read %s: offset=0x%llx length=%d address=0x%x", GetName().c_str(),
                  offset, length, address);

        if (offset + length > backend->GetSize()) {
            LOG_ERROR(Service_FS,
                      "Reading from out of bounds offset=0x%llX length=0x%08X file_size=0x%llX",
                      offset, length, backend->GetSize());
        }

        std::vector<u8> data(length);
        ResultVal<size_t> read = backend->Read(offset, data.size(), data.data());
        if (read.Failed()) {
            cmd_buff[1] = read.Code().raw;
            return;
        }
        Memory::WriteBlock(address, data.data(), *read);
        cmd_buff[2] = static_cast<u32>(*read);
        break;
    }

    // Write to file...
    case FileCommand::Write: {
        u64 offset = cmd_buff[1] | ((u64)cmd_buff[2]) << 32;
        u32 length = cmd_buff[3];
        u32 flush = cmd_buff[4];
        u32 address = cmd_buff[6];
        LOG_TRACE(Service_FS, "Write %s: offset=0x%llx length=%d address=0x%x, flush=0x%x",
                  GetName().c_str(), offset, length, address, flush);

        std::vector<u8> data(length);
        Memory::ReadBlock(address, data.data(), data.size());
        ResultVal<size_t> written = backend->Write(offset, data.size(), flush != 0, data.data());
        if (written.Failed()) {
            cmd_buff[1] = written.Code().raw;
            return;
        }
        cmd_buff[2] = static_cast<u32>(*written);
        break;
    }

    case FileCommand::GetSize: {
        LOG_TRACE(Service_FS, "GetSize %s", GetName().c_str());
        u64 size = backend->GetSize();
        cmd_buff[2] = (u32)size;
        cmd_buff[3] = size >> 32;
        break;
    }

    case FileCommand::SetSize: {
        u64 size = cmd_buff[1] | ((u64)cmd_buff[2] << 32);
        LOG_TRACE(Service_FS, "SetSize %s size=%llu", GetName().c_str(), size);
        backend->SetSize(size);
        break;
    }

    case FileCommand::Close: {
        LOG_TRACE(Service_FS, "Close %s", GetName().c_str());
        backend->Close();
        break;
    }

    case FileCommand::Flush: {
        LOG_TRACE(Service_FS, "Flush");
        backend->Flush();
        break;
    }

    case FileCommand::OpenLinkFile: {
        LOG_WARNING(Service_FS, "(STUBBED) File command OpenLinkFile %s", GetName().c_str());
        auto sessions = ServerSession::CreateSessionPair(GetName());
        ClientConnected(std::get<SharedPtr<ServerSession>>(sessions));
        cmd_buff[3] = Kernel::g_handle_table.Create(std::get<SharedPtr<ClientSession>>(sessions))
                          .ValueOr(INVALID_HANDLE);
        break;
    }

    case FileCommand::SetPriority: {
        priority = cmd_buff[1];
        LOG_TRACE(Service_FS, "SetPriority %u", priority);
        break;
    }

    case FileCommand::GetPriority: {
        cmd_buff[2] = priority;
        LOG_TRACE(Service_FS, "GetPriority");
        break;
    }

    // Unknown command...
    default:
        LOG_ERROR(Service_FS, "Unknown command=0x%08X!", cmd);
        ResultCode error = UnimplementedFunction(ErrorModule::FS);
        cmd_buff[1] = error.raw; // TODO(Link Mauve): use the correct error code for that.
        return;
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

Directory::Directory(std::unique_ptr<FileSys::DirectoryBackend>&& backend,
                     const FileSys::Path& path)
    : path(path), backend(std::move(backend)) {}

Directory::~Directory() {}

void Directory::HandleSyncRequest(Kernel::SharedPtr<Kernel::ServerSession> server_session) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    DirectoryCommand cmd = static_cast<DirectoryCommand>(cmd_buff[0]);
    switch (cmd) {
    // Read from directory...
    case DirectoryCommand::Read: {
        u32 count = cmd_buff[1];
        u32 address = cmd_buff[3];
        std::vector<FileSys::Entry> entries(count);
        LOG_TRACE(Service_FS, "Read %s: count=%d", GetName().c_str(), count);

        // Number of entries actually read
        u32 read = backend->Read(static_cast<u32>(entries.size()), entries.data());
        cmd_buff[2] = read;
        Memory::WriteBlock(address, entries.data(), read * sizeof(FileSys::Entry));
        break;
    }

    case DirectoryCommand::Close: {
        LOG_TRACE(Service_FS, "Close %s", GetName().c_str());
        backend->Close();
        break;
    }

    // Unknown command...
    default:
        LOG_ERROR(Service_FS, "Unknown command=0x%08X!", cmd);
        ResultCode error = UnimplementedFunction(ErrorModule::FS);
        cmd_buff[1] = error.raw; // TODO(Link Mauve): use the correct error code for that.
        return;
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

////////////////////////////////////////////////////////////////////////////////////////////////////

using FileSys::ArchiveBackend;
using FileSys::ArchiveFactory;

/**
 * Map of registered archives, identified by id code. Once an archive is registered here, it is
 * never removed until UnregisterArchiveTypes is called.
 */
static boost::container::flat_map<ArchiveIdCode, std::unique_ptr<ArchiveFactory>> id_code_map;

/**
 * Map of active archive handles. Values are pointers to the archives in `idcode_map`.
 */
static std::unordered_map<ArchiveHandle, std::unique_ptr<ArchiveBackend>> handle_map;
static ArchiveHandle next_handle;

static ArchiveBackend* GetArchive(ArchiveHandle handle) {
    auto itr = handle_map.find(handle);
    return (itr == handle_map.end()) ? nullptr : itr->second.get();
}

ResultVal<ArchiveHandle> OpenArchive(ArchiveIdCode id_code, FileSys::Path& archive_path) {
    LOG_TRACE(Service_FS, "Opening archive with id code 0x%08X", id_code);

    auto itr = id_code_map.find(id_code);
    if (itr == id_code_map.end()) {
        return FileSys::ERROR_NOT_FOUND;
    }

    CASCADE_RESULT(std::unique_ptr<ArchiveBackend> res, itr->second->Open(archive_path));

    // This should never even happen in the first place with 64-bit handles,
    while (handle_map.count(next_handle) != 0) {
        ++next_handle;
    }
    handle_map.emplace(next_handle, std::move(res));
    return MakeResult<ArchiveHandle>(next_handle++);
}

ResultCode CloseArchive(ArchiveHandle handle) {
    if (handle_map.erase(handle) == 0)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    else
        return RESULT_SUCCESS;
}

// TODO(yuriks): This might be what the fs:REG service is for. See the Register/Unregister calls in
// http://3dbrew.org/wiki/Filesystem_services#ProgramRegistry_service_.22fs:REG.22
ResultCode RegisterArchiveType(std::unique_ptr<FileSys::ArchiveFactory>&& factory,
                               ArchiveIdCode id_code) {
    auto result = id_code_map.emplace(id_code, std::move(factory));

    bool inserted = result.second;
    ASSERT_MSG(inserted, "Tried to register more than one archive with same id code");

    auto& archive = result.first->second;
    LOG_DEBUG(Service_FS, "Registered archive %s with id code 0x%08X", archive->GetName().c_str(),
              id_code);
    return RESULT_SUCCESS;
}

ResultVal<std::shared_ptr<File>> OpenFileFromArchive(ArchiveHandle archive_handle,
                                                     const FileSys::Path& path,
                                                     const FileSys::Mode mode) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    auto backend = archive->OpenFile(path, mode);
    if (backend.Failed())
        return backend.Code();

    auto file = std::shared_ptr<File>(new File(std::move(backend).Unwrap(), path));
    return MakeResult<std::shared_ptr<File>>(std::move(file));
}

ResultCode DeleteFileFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    return archive->DeleteFile(path);
}

ResultCode RenameFileBetweenArchives(ArchiveHandle src_archive_handle,
                                     const FileSys::Path& src_path,
                                     ArchiveHandle dest_archive_handle,
                                     const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive = GetArchive(src_archive_handle);
    ArchiveBackend* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    if (src_archive == dest_archive) {
        return src_archive->RenameFile(src_path, dest_path);
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
}

ResultCode DeleteDirectoryFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    return archive->DeleteDirectory(path);
}

ResultCode DeleteDirectoryRecursivelyFromArchive(ArchiveHandle archive_handle,
                                                 const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    return archive->DeleteDirectoryRecursively(path);
}

ResultCode CreateFileInArchive(ArchiveHandle archive_handle, const FileSys::Path& path,
                               u64 file_size) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    return archive->CreateFile(path, file_size);
}

ResultCode CreateDirectoryFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    return archive->CreateDirectory(path);
}

ResultCode RenameDirectoryBetweenArchives(ArchiveHandle src_archive_handle,
                                          const FileSys::Path& src_path,
                                          ArchiveHandle dest_archive_handle,
                                          const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive = GetArchive(src_archive_handle);
    ArchiveBackend* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    if (src_archive == dest_archive) {
        return src_archive->RenameDirectory(src_path, dest_path);
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
}

ResultVal<std::shared_ptr<Directory>> OpenDirectoryFromArchive(ArchiveHandle archive_handle,
                                                               const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;

    auto backend = archive->OpenDirectory(path);
    if (backend.Failed())
        return backend.Code();

    auto directory = std::shared_ptr<Directory>(new Directory(std::move(backend).Unwrap(), path));
    return MakeResult<std::shared_ptr<Directory>>(std::move(directory));
}

ResultVal<u64> GetFreeBytesInArchive(ArchiveHandle archive_handle) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ERR_INVALID_ARCHIVE_HANDLE;
    return MakeResult<u64>(archive->GetFreeBytes());
}

ResultCode FormatArchive(ArchiveIdCode id_code, const FileSys::ArchiveFormatInfo& format_info,
                         const FileSys::Path& path) {
    auto archive_itr = id_code_map.find(id_code);
    if (archive_itr == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    return archive_itr->second->Format(path, format_info);
}

ResultVal<FileSys::ArchiveFormatInfo> GetArchiveFormatInfo(ArchiveIdCode id_code,
                                                           FileSys::Path& archive_path) {
    auto archive = id_code_map.find(id_code);
    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    return archive->second->GetFormatInfo(archive_path);
}

ResultCode CreateExtSaveData(MediaType media_type, u32 high, u32 low, VAddr icon_buffer,
                             u32 icon_size, const FileSys::ArchiveFormatInfo& format_info) {
    // Construct the binary path to the archive first
    FileSys::Path path =
        FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low);

    auto archive = id_code_map.find(media_type == MediaType::NAND ? ArchiveIdCode::SharedExtSaveData
                                                                  : ArchiveIdCode::ExtSaveData);

    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    auto ext_savedata = static_cast<FileSys::ArchiveFactory_ExtSaveData*>(archive->second.get());

    ResultCode result = ext_savedata->Format(path, format_info);
    if (result.IsError())
        return result;

    if (!Memory::IsValidVirtualAddress(icon_buffer))
        return ResultCode(-1); // TODO(Subv): Find the right error code

    std::vector<u8> smdh_icon(icon_size);
    Memory::ReadBlock(icon_buffer, smdh_icon.data(), smdh_icon.size());
    ext_savedata->WriteIcon(path, smdh_icon.data(), smdh_icon.size());
    return RESULT_SUCCESS;
}

ResultCode DeleteExtSaveData(MediaType media_type, u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path =
        FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low);

    std::string media_type_directory;
    if (media_type == MediaType::NAND) {
        media_type_directory = FileUtil::GetUserPath(D_NAND_IDX);
    } else if (media_type == MediaType::SDMC) {
        media_type_directory = FileUtil::GetUserPath(D_SDMC_IDX);
    } else {
        LOG_ERROR(Service_FS, "Unsupported media type %u", media_type);
        return ResultCode(-1); // TODO(Subv): Find the right error code
    }

    // Delete all directories (/user, /boss) and the icon file.
    std::string base_path =
        FileSys::GetExtDataContainerPath(media_type_directory, media_type == MediaType::NAND);
    std::string extsavedata_path = FileSys::GetExtSaveDataPath(base_path, path);
    if (FileUtil::Exists(extsavedata_path) && !FileUtil::DeleteDirRecursively(extsavedata_path))
        return ResultCode(-1); // TODO(Subv): Find the right error code
    return RESULT_SUCCESS;
}

ResultCode DeleteSystemSaveData(u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path = FileSys::ConstructSystemSaveDataBinaryPath(high, low);

    std::string nand_directory = FileUtil::GetUserPath(D_NAND_IDX);
    std::string base_path = FileSys::GetSystemSaveDataContainerPath(nand_directory);
    std::string systemsavedata_path = FileSys::GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::DeleteDirRecursively(systemsavedata_path))
        return ResultCode(-1); // TODO(Subv): Find the right error code
    return RESULT_SUCCESS;
}

ResultCode CreateSystemSaveData(u32 high, u32 low) {
    // Construct the binary path to the archive first
    FileSys::Path path = FileSys::ConstructSystemSaveDataBinaryPath(high, low);

    std::string nand_directory = FileUtil::GetUserPath(D_NAND_IDX);
    std::string base_path = FileSys::GetSystemSaveDataContainerPath(nand_directory);
    std::string systemsavedata_path = FileSys::GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::CreateFullPath(systemsavedata_path))
        return ResultCode(-1); // TODO(Subv): Find the right error code
    return RESULT_SUCCESS;
}

void RegisterArchiveTypes() {
    // TODO(Subv): Add the other archive types (see here for the known types:
    // http://3dbrew.org/wiki/FS:OpenArchive#Archive_idcodes).

    std::string sdmc_directory = FileUtil::GetUserPath(D_SDMC_IDX);
    std::string nand_directory = FileUtil::GetUserPath(D_NAND_IDX);
    auto sdmc_factory = std::make_unique<FileSys::ArchiveFactory_SDMC>(sdmc_directory);
    if (sdmc_factory->Initialize())
        RegisterArchiveType(std::move(sdmc_factory), ArchiveIdCode::SDMC);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path %s",
                  sdmc_directory.c_str());

    auto sdmcwo_factory = std::make_unique<FileSys::ArchiveFactory_SDMCWriteOnly>(sdmc_directory);
    if (sdmcwo_factory->Initialize())
        RegisterArchiveType(std::move(sdmcwo_factory), ArchiveIdCode::SDMCWriteOnly);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMCWriteOnly archive with path %s",
                  sdmc_directory.c_str());

    // Create the SaveData archive
    auto sd_savedata_source = std::make_shared<FileSys::ArchiveSource_SDSaveData>(sdmc_directory);
    auto savedata_factory = std::make_unique<FileSys::ArchiveFactory_SaveData>(sd_savedata_source);
    RegisterArchiveType(std::move(savedata_factory), ArchiveIdCode::SaveData);
    auto other_savedata_permitted_factory =
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataPermitted>(sd_savedata_source);
    RegisterArchiveType(std::move(other_savedata_permitted_factory),
                        ArchiveIdCode::OtherSaveDataPermitted);
    auto other_savedata_general_factory =
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataGeneral>(sd_savedata_source);
    RegisterArchiveType(std::move(other_savedata_general_factory),
                        ArchiveIdCode::OtherSaveDataGeneral);

    auto extsavedata_factory =
        std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(sdmc_directory, false);
    if (extsavedata_factory->Initialize())
        RegisterArchiveType(std::move(extsavedata_factory), ArchiveIdCode::ExtSaveData);
    else
        LOG_ERROR(Service_FS, "Can't instantiate ExtSaveData archive with path %s",
                  extsavedata_factory->GetMountPoint().c_str());

    auto sharedextsavedata_factory =
        std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(nand_directory, true);
    if (sharedextsavedata_factory->Initialize())
        RegisterArchiveType(std::move(sharedextsavedata_factory), ArchiveIdCode::SharedExtSaveData);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SharedExtSaveData archive with path %s",
                  sharedextsavedata_factory->GetMountPoint().c_str());

    // Create the NCCH archive, basically a small variation of the RomFS archive
    auto savedatacheck_factory = std::make_unique<FileSys::ArchiveFactory_NCCH>(nand_directory);
    RegisterArchiveType(std::move(savedatacheck_factory), ArchiveIdCode::NCCH);

    auto systemsavedata_factory =
        std::make_unique<FileSys::ArchiveFactory_SystemSaveData>(nand_directory);
    RegisterArchiveType(std::move(systemsavedata_factory), ArchiveIdCode::SystemSaveData);

    auto selfncch_factory = std::make_unique<FileSys::ArchiveFactory_SelfNCCH>();
    RegisterArchiveType(std::move(selfncch_factory), ArchiveIdCode::SelfNCCH);
}

void RegisterSelfNCCH(Loader::AppLoader& app_loader) {
    auto itr = id_code_map.find(ArchiveIdCode::SelfNCCH);
    if (itr == id_code_map.end()) {
        LOG_ERROR(Service_FS,
                  "Could not register a new NCCH because the SelfNCCH archive hasn't been created");
        return;
    }

    auto* factory = static_cast<FileSys::ArchiveFactory_SelfNCCH*>(itr->second.get());
    factory->Register(app_loader);
}

void UnregisterArchiveTypes() {
    id_code_map.clear();
}

/// Initialize archives
void ArchiveInit() {
    next_handle = 1;

    AddService(new FS::Interface);

    RegisterArchiveTypes();
}

/// Shutdown archives
void ArchiveShutdown() {
    handle_map.clear();
    UnregisterArchiveTypes();
}

} // namespace FS
} // namespace Service
