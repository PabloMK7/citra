// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
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
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

namespace Service {
namespace FS {

// Command to access directory
enum class DirectoryCommand : u32 {
    Dummy1 = 0x000100C6,
    Control = 0x040100C4,
    Read = 0x08010042,
    Close = 0x08020000,
};

File::File(std::unique_ptr<FileSys::FileBackend>&& backend, const FileSys::Path& path)
    : ServiceFramework("", 1), path(path), backend(std::move(backend)) {
    static const FunctionInfo functions[] = {
        {0x08010100, &File::OpenSubFile, "OpenSubFile"},
        {0x080200C2, &File::Read, "Read"},
        {0x08030102, &File::Write, "Write"},
        {0x08040000, &File::GetSize, "GetSize"},
        {0x08050080, &File::SetSize, "SetSize"},
        {0x08080000, &File::Close, "Close"},
        {0x08090000, &File::Flush, "Flush"},
        {0x080A0040, &File::SetPriority, "SetPriority"},
        {0x080B0000, &File::GetPriority, "GetPriority"},
        {0x080C0000, &File::OpenLinkFile, "OpenLinkFile"},
    };
    RegisterHandlers(functions);
}

void File::Read(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0802, 3, 2);
    u64 offset = rp.Pop<u64>();
    u32 length = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();
    LOG_TRACE(Service_FS, "Read {}: offset=0x{:x} length=0x{:08X}", GetName(), offset, length);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    if (file->subfile && length > file->size) {
        LOG_WARNING(Service_FS, "Trying to read beyond the subfile size, truncating");
        length = static_cast<u32>(file->size);
    }

    // This file session might have a specific offset from where to start reading, apply it.
    offset += file->offset;

    if (offset + length > backend->GetSize()) {
        LOG_ERROR(Service_FS,
                  "Reading from out of bounds offset=0x{:x} length=0x{:08X} file_size=0x{:x}",
                  offset, length, backend->GetSize());
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    std::vector<u8> data(length);
    ResultVal<std::size_t> read = backend->Read(offset, data.size(), data.data());
    if (read.Failed()) {
        rb.Push(read.Code());
        rb.Push<u32>(0);
    } else {
        buffer.Write(data.data(), 0, *read);
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(static_cast<u32>(*read));
    }
    rb.PushMappedBuffer(buffer);

    std::chrono::nanoseconds read_timeout_ns{backend->GetReadDelayNs(length)};
    ctx.SleepClientThread(Kernel::GetCurrentThread(), "file::read", read_timeout_ns,
                          [](Kernel::SharedPtr<Kernel::Thread> thread,
                             Kernel::HLERequestContext& ctx, ThreadWakeupReason reason) {
                              // Nothing to do here
                          });
}

void File::Write(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0803, 4, 2);
    u64 offset = rp.Pop<u64>();
    u32 length = rp.Pop<u32>();
    u32 flush = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();
    LOG_TRACE(Service_FS, "Write {}: offset=0x{:x} length={}, flush=0x{:x}", GetName(), offset,
              length, flush);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    // Subfiles can not be written to
    if (file->subfile) {
        rb.Push(FileSys::ERROR_UNSUPPORTED_OPEN_FLAGS);
        rb.Push<u32>(0);
        rb.PushMappedBuffer(buffer);
        return;
    }

    std::vector<u8> data(length);
    buffer.Read(data.data(), 0, data.size());
    ResultVal<std::size_t> written = backend->Write(offset, data.size(), flush != 0, data.data());
    if (written.Failed()) {
        rb.Push(written.Code());
        rb.Push<u32>(0);
    } else {
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(static_cast<u32>(*written));
    }
    rb.PushMappedBuffer(buffer);
}

void File::GetSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0804, 0, 0);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(file->size);
}

void File::SetSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0805, 2, 0);
    u64 size = rp.Pop<u64>();

    FileSessionSlot* file = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    // SetSize can not be called on subfiles.
    if (file->subfile) {
        rb.Push(FileSys::ERROR_UNSUPPORTED_OPEN_FLAGS);
        return;
    }

    file->size = size;
    backend->SetSize(size);
    rb.Push(RESULT_SUCCESS);
}

void File::Close(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0808, 0, 0);

    // TODO(Subv): Only close the backend if this client is the only one left.
    if (connected_sessions.size() > 1)
        LOG_WARNING(Service_FS, "Closing File backend but {} clients still connected",
                    connected_sessions.size());

    backend->Close();
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void File::Flush(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0809, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    const FileSessionSlot* file = GetSessionData(ctx.Session());

    // Subfiles can not be flushed.
    if (file->subfile) {
        rb.Push(FileSys::ERROR_UNSUPPORTED_OPEN_FLAGS);
        return;
    }

    backend->Flush();
    rb.Push(RESULT_SUCCESS);
}

void File::SetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x080A, 1, 0);

    FileSessionSlot* file = GetSessionData(ctx.Session());
    file->priority = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void File::GetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x080B, 0, 0);
    const FileSessionSlot* file = GetSessionData(ctx.Session());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(file->priority);
}

void File::OpenLinkFile(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED) File command OpenLinkFile {}", GetName());
    using Kernel::ClientSession;
    using Kernel::ServerSession;
    using Kernel::SharedPtr;
    IPC::RequestParser rp(ctx, 0x080C, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    auto sessions = ServerSession::CreateSessionPair(GetName());
    auto server = std::get<SharedPtr<ServerSession>>(sessions);
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(server);
    const FileSessionSlot* original_file = GetSessionData(ctx.Session());

    slot->priority = original_file->priority;
    slot->offset = 0;
    slot->size = backend->GetSize();
    slot->subfile = false;

    rb.Push(RESULT_SUCCESS);
    rb.PushMoveObjects(std::get<SharedPtr<ClientSession>>(sessions));
}

void File::OpenSubFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0801, 4, 0);
    s64 offset = rp.PopRaw<s64>();
    s64 size = rp.PopRaw<s64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    const FileSessionSlot* original_file = GetSessionData(ctx.Session());

    if (original_file->subfile) {
        // OpenSubFile can not be called on a file which is already as subfile
        rb.Push(FileSys::ERROR_UNSUPPORTED_OPEN_FLAGS);
        return;
    }

    if (offset < 0 || size < 0) {
        rb.Push(FileSys::ERR_WRITE_BEYOND_END);
        return;
    }

    std::size_t end = offset + size;

    // TODO(Subv): Check for overflow and return ERR_WRITE_BEYOND_END

    if (end > original_file->size) {
        rb.Push(FileSys::ERR_WRITE_BEYOND_END);
        return;
    }

    using Kernel::ClientSession;
    using Kernel::ServerSession;
    using Kernel::SharedPtr;
    auto sessions = ServerSession::CreateSessionPair(GetName());
    auto server = std::get<SharedPtr<ServerSession>>(sessions);
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(server);
    slot->priority = original_file->priority;
    slot->offset = offset;
    slot->size = size;
    slot->subfile = true;

    rb.Push(RESULT_SUCCESS);
    rb.PushMoveObjects(std::get<SharedPtr<ClientSession>>(sessions));
}

Kernel::SharedPtr<Kernel::ClientSession> File::Connect() {
    auto sessions = Kernel::ServerSession::CreateSessionPair(GetName());
    auto server = std::get<Kernel::SharedPtr<Kernel::ServerSession>>(sessions);
    ClientConnected(server);

    FileSessionSlot* slot = GetSessionData(server);
    slot->priority = 0;
    slot->offset = 0;
    slot->size = backend->GetSize();
    slot->subfile = false;

    return std::get<Kernel::SharedPtr<Kernel::ClientSession>>(sessions);
}

Directory::Directory(std::unique_ptr<FileSys::DirectoryBackend>&& backend,
                     const FileSys::Path& path)
    : ServiceFramework("", 1), path(path), backend(std::move(backend)) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x08010042, &Directory::Read, "Read"},
        {0x08020000, &Directory::Close, "Close"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

Directory::~Directory() {}

void Directory::Read(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0801, 1, 2);
    u32 count = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();
    std::vector<FileSys::Entry> entries(count);
    LOG_TRACE(Service_FS, "Read {}: count={}", GetName(), count);
    // Number of entries actually read
    u32 read = backend->Read(static_cast<u32>(entries.size()), entries.data());
    buffer.Write(entries.data(), 0, read * sizeof(FileSys::Entry));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(read);
    rb.PushMappedBuffer(buffer);
}

void Directory::Close(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0802, 0, 0);
    LOG_TRACE(Service_FS, "Close {}", GetName());
    backend->Close();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
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
    LOG_TRACE(Service_FS, "Opening archive with id code 0x{:08X}", static_cast<u32>(id_code));

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
    LOG_DEBUG(Service_FS, "Registered archive {} with id code 0x{:08X}", archive->GetName(),
              static_cast<u32>(id_code));
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

ResultCode CreateExtSaveData(MediaType media_type, u32 high, u32 low,
                             const std::vector<u8>& smdh_icon,
                             const FileSys::ArchiveFormatInfo& format_info) {
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
        LOG_ERROR(Service_FS, "Unsupported media type {}", static_cast<u32>(media_type));
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
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path {}", sdmc_directory);

    auto sdmcwo_factory = std::make_unique<FileSys::ArchiveFactory_SDMCWriteOnly>(sdmc_directory);
    if (sdmcwo_factory->Initialize())
        RegisterArchiveType(std::move(sdmcwo_factory), ArchiveIdCode::SDMCWriteOnly);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMCWriteOnly archive with path {}",
                  sdmc_directory);

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
        LOG_ERROR(Service_FS, "Can't instantiate ExtSaveData archive with path {}",
                  extsavedata_factory->GetMountPoint());

    auto sharedextsavedata_factory =
        std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(nand_directory, true);
    if (sharedextsavedata_factory->Initialize())
        RegisterArchiveType(std::move(sharedextsavedata_factory), ArchiveIdCode::SharedExtSaveData);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SharedExtSaveData archive with path {}",
                  sharedextsavedata_factory->GetMountPoint());

    // Create the NCCH archive, basically a small variation of the RomFS archive
    auto savedatacheck_factory = std::make_unique<FileSys::ArchiveFactory_NCCH>();
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
    RegisterArchiveTypes();
}

/// Shutdown archives
void ArchiveShutdown() {
    handle_map.clear();
    UnregisterArchiveTypes();
}

} // namespace FS
} // namespace Service
