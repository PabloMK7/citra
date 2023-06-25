// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/seed_db.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"

SERVICE_CONSTRUCT_IMPL(Service::FS::FS_USER)
SERIALIZE_EXPORT_IMPL(Service::FS::FS_USER)
SERIALIZE_EXPORT_IMPL(Service::FS::ClientSlot)

using Kernel::ClientSession;
using Kernel::ServerSession;

namespace Service::FS {

void FS_USER::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0801, 0, 2);
    u32 pid = rp.PopPID();

    ClientSlot* slot = GetSessionData(ctx.Session());
    slot->program_id = system.Kernel().GetProcessById(pid)->codeset->program_id;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void FS_USER::OpenFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0802, 7, 2);
    rp.Skip(1, false); // Transaction.

    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const FileSys::Mode mode{rp.Pop<u32>()};
    const auto attributes = rp.Pop<u32>(); // TODO(Link Mauve): do something with those attributes.
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);
    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "path={}, mode={} attrs={}", file_path.DebugStr(), mode.hex, attributes);

    const auto [file_res, open_timeout_ns] =
        archives.OpenFileFromArchive(archive_handle, file_path, mode);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(file_res.Code());
    if (file_res.Succeeded()) {
        std::shared_ptr<File> file = *file_res;
        rb.PushMoveObjects(file->Connect());
    } else {
        rb.PushMoveObjects<Kernel::Object>(nullptr);
        LOG_DEBUG(Service_FS, "failed to get a handle for file {}", file_path.DebugStr());
    }

    ctx.SleepClientThread("fs_user::open", open_timeout_ns, nullptr);
}

void FS_USER::OpenFileDirectly(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x803, 8, 4);
    rp.Skip(1, false); // Transaction

    const auto archive_id = rp.PopEnum<ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const FileSys::Mode mode{rp.Pop<u32>()};
    const auto attributes = rp.Pop<u32>(); // TODO(Link Mauve): do something with those attributes.
    std::vector<u8> archivename = rp.PopStaticBuffer();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    ASSERT(filename.size() == filename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));
    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "archive_id=0x{:08X} archive_path={} file_path={}, mode={} attributes={}",
              archive_id, archive_path.DebugStr(), file_path.DebugStr(), mode.hex, attributes);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    ClientSlot* slot = GetSessionData(ctx.Session());

    ResultVal<ArchiveHandle> archive_handle =
        archives.OpenArchive(archive_id, archive_path, slot->program_id);
    if (archive_handle.Failed()) {
        LOG_ERROR(Service_FS,
                  "Failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                  archive_id, archive_path.DebugStr());
        rb.Push(archive_handle.Code());
        rb.PushMoveObjects<Kernel::Object>(nullptr);
        return;
    }
    SCOPE_EXIT({ archives.CloseArchive(*archive_handle); });

    const auto [file_res, open_timeout_ns] =
        archives.OpenFileFromArchive(*archive_handle, file_path, mode);
    rb.Push(file_res.Code());
    if (file_res.Succeeded()) {
        std::shared_ptr<File> file = *file_res;
        rb.PushMoveObjects(file->Connect());
    } else {
        rb.PushMoveObjects<Kernel::Object>(nullptr);
        LOG_ERROR(Service_FS, "failed to get a handle for file {} mode={} attributes={}",
                  file_path.DebugStr(), mode.hex, attributes);
    }

    ctx.SleepClientThread("fs_user::open_directly", open_timeout_ns, nullptr);
}

void FS_USER::DeleteFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x804, 5, 2);
    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);

    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", filename_type, filename_size,
              file_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteFileFromArchive(archive_handle, file_path));
}

void FS_USER::RenameFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x805, 9, 4);
    rp.Skip(1, false); // TransactionId

    const auto src_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto src_filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto src_filename_size = rp.Pop<u32>();
    const auto dest_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dest_filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dest_filename_size = rp.Pop<u32>();
    std::vector<u8> src_filename = rp.PopStaticBuffer();
    std::vector<u8> dest_filename = rp.PopStaticBuffer();
    ASSERT(src_filename.size() == src_filename_size);
    ASSERT(dest_filename.size() == dest_filename_size);

    const FileSys::Path src_file_path(src_filename_type, std::move(src_filename));
    const FileSys::Path dest_file_path(dest_filename_type, std::move(dest_filename));

    LOG_DEBUG(Service_FS,
              "src_type={} src_size={} src_data={} dest_type={} dest_size={} dest_data={}",
              src_filename_type, src_filename_size, src_file_path.DebugStr(), dest_filename_type,
              dest_filename_size, dest_file_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.RenameFileBetweenArchives(src_archive_handle, src_file_path,
                                               dest_archive_handle, dest_file_path));
}

void FS_USER::DeleteDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x806, 5, 2);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteDirectoryFromArchive(archive_handle, dir_path));
}

void FS_USER::DeleteDirectoryRecursively(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x807, 5, 2);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteDirectoryRecursivelyFromArchive(archive_handle, dir_path));
}

void FS_USER::CreateFile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x808, 8, 2);

    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto filename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto filename_size = rp.Pop<u32>();
    const auto attributes = rp.Pop<u32>();
    const auto file_size = rp.Pop<u64>();
    std::vector<u8> filename = rp.PopStaticBuffer();
    ASSERT(filename.size() == filename_size);

    const FileSys::Path file_path(filename_type, std::move(filename));

    LOG_DEBUG(Service_FS, "type={} attributes={} size={:x} data={}", filename_type, attributes,
              file_size, file_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.CreateFileInArchive(archive_handle, file_path, file_size));
}

void FS_USER::CreateDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x809, 6, 2);
    rp.Skip(1, false); // TransactionId
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    [[maybe_unused]] const auto attributes = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);
    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.CreateDirectoryFromArchive(archive_handle, dir_path));
}

void FS_USER::RenameDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80A, 9, 4);
    rp.Skip(1, false); // TransactionId
    const auto src_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto src_dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto src_dirname_size = rp.Pop<u32>();
    const auto dest_archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dest_dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dest_dirname_size = rp.Pop<u32>();
    std::vector<u8> src_dirname = rp.PopStaticBuffer();
    std::vector<u8> dest_dirname = rp.PopStaticBuffer();
    ASSERT(src_dirname.size() == src_dirname_size);
    ASSERT(dest_dirname.size() == dest_dirname_size);

    const FileSys::Path src_dir_path(src_dirname_type, std::move(src_dirname));
    const FileSys::Path dest_dir_path(dest_dirname_type, std::move(dest_dirname));

    LOG_DEBUG(Service_FS,
              "src_type={} src_size={} src_data={} dest_type={} dest_size={} dest_data={}",
              src_dirname_type, src_dirname_size, src_dir_path.DebugStr(), dest_dirname_type,
              dest_dirname_size, dest_dir_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.RenameDirectoryBetweenArchives(src_archive_handle, src_dir_path,
                                                    dest_archive_handle, dest_dir_path));
}

void FS_USER::OpenDirectory(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80B, 4, 2);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    const auto dirname_type = rp.PopEnum<FileSys::LowPathType>();
    const auto dirname_size = rp.Pop<u32>();
    std::vector<u8> dirname = rp.PopStaticBuffer();
    ASSERT(dirname.size() == dirname_size);

    const FileSys::Path dir_path(dirname_type, std::move(dirname));

    LOG_DEBUG(Service_FS, "type={} size={} data={}", dirname_type, dirname_size,
              dir_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    ResultVal<std::shared_ptr<Directory>> dir_res =
        archives.OpenDirectoryFromArchive(archive_handle, dir_path);
    rb.Push(dir_res.Code());
    if (dir_res.Succeeded()) {
        std::shared_ptr<Directory> directory = *dir_res;
        auto [server, client] = system.Kernel().CreateSessionPair(directory->GetName());
        directory->ClientConnected(server);
        rb.PushMoveObjects(client);
    } else {
        LOG_ERROR(Service_FS, "failed to get a handle for directory type={} size={} data={}",
                  dirname_type, dirname_size, dir_path.DebugStr());
        rb.PushMoveObjects<Kernel::Object>(nullptr);
    }
}

void FS_USER::OpenArchive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80C, 3, 2);
    const auto archive_id = rp.PopEnum<FS::ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_id=0x{:08X} archive_path={}", archive_id,
              archive_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    ClientSlot* slot = GetSessionData(ctx.Session());
    const ResultVal<ArchiveHandle> handle =
        archives.OpenArchive(archive_id, archive_path, slot->program_id);
    rb.Push(handle.Code());
    if (handle.Succeeded()) {
        rb.PushRaw(*handle);
    } else {
        rb.Push<u64>(0);
        LOG_ERROR(Service_FS,
                  "failed to get a handle for archive archive_id=0x{:08X} archive_path={}",
                  archive_id, archive_path.DebugStr());
    }
}

void FS_USER::CloseArchive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80E, 2, 0);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.CloseArchive(archive_handle));
}

void FS_USER::IsSdmcDetected(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x817, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(Settings::values.use_virtual_sd.GetValue());
}

void FS_USER::IsSdmcWriteable(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x818, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    // If the SD isn't enabled, it can't be writeable...else, stubbed true
    rb.Push(Settings::values.use_virtual_sd.GetValue());
    LOG_DEBUG(Service_FS, " (STUBBED)");
}

void FS_USER::FormatSaveData(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FS, "(STUBBED)");

    IPC::RequestParser rp(ctx, 0x84C, 9, 2);
    const auto archive_id = rp.PopEnum<ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    const auto block_size = rp.Pop<u32>();
    const auto number_directories = rp.Pop<u32>();
    const auto number_files = rp.Pop<u32>();
    [[maybe_unused]] const auto directory_buckets = rp.Pop<u32>();
    [[maybe_unused]] const auto file_buckets = rp.Pop<u32>();
    const bool duplicate_data = rp.Pop<bool>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);
    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_path={}", archive_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (archive_id != FS::ArchiveIdCode::SaveData) {
        LOG_ERROR(Service_FS, "tried to format an archive different than SaveData, {}", archive_id);
        rb.Push(FileSys::ERROR_INVALID_PATH);
        return;
    }

    if (archive_path.GetType() != FileSys::LowPathType::Empty) {
        // TODO(Subv): Implement formatting the SaveData of other games
        LOG_ERROR(Service_FS, "archive LowPath type other than empty is currently unsupported");
        rb.Push(UnimplementedFunction(ErrorModule::FS));
        return;
    }

    FileSys::ArchiveFormatInfo format_info;
    format_info.duplicate_data = duplicate_data;
    format_info.number_directories = number_directories;
    format_info.number_files = number_files;
    format_info.total_size = block_size * 512;

    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.FormatArchive(ArchiveIdCode::SaveData, format_info, archive_path,
                                   slot->program_id));
}

void FS_USER::FormatThisUserSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x80F, 6, 0);
    const auto block_size = rp.Pop<u32>();
    const auto number_directories = rp.Pop<u32>();
    const auto number_files = rp.Pop<u32>();
    [[maybe_unused]] const auto directory_buckets = rp.Pop<u32>();
    [[maybe_unused]] const auto file_buckets = rp.Pop<u32>();
    const auto duplicate_data = rp.Pop<bool>();

    FileSys::ArchiveFormatInfo format_info;
    format_info.duplicate_data = duplicate_data;
    format_info.number_directories = number_directories;
    format_info.number_files = number_files;
    format_info.total_size = block_size * 512;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.FormatArchive(ArchiveIdCode::SaveData, format_info, FileSys::Path(),
                                   slot->program_id));

    LOG_TRACE(Service_FS, "called");
}

void FS_USER::GetFreeBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x812, 2, 0);
    const auto archive_handle = rp.PopRaw<ArchiveHandle>();
    ResultVal<u64> bytes_res = archives.GetFreeBytesInArchive(archive_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(bytes_res.Code());
    if (bytes_res.Succeeded()) {
        rb.Push<u64>(bytes_res.Unwrap());
    } else {
        rb.Push<u64>(0);
    }
}

void FS_USER::GetSdmcArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x814, 0, 0);

    LOG_WARNING(Service_FS, "(STUBBED) called");

    auto resource = archives.GetArchiveResource(MediaType::SDMC);

    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(*resource);
}

void FS_USER::GetNandArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x815, 0, 0);

    LOG_WARNING(Service_FS, "(STUBBED) called");

    auto resource = archives.GetArchiveResource(MediaType::NAND);
    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(*resource);
}

void FS_USER::CreateExtSaveData(Kernel::HLERequestContext& ctx) {
    // TODO(Subv): Figure out the other parameters.
    IPC::RequestParser rp(ctx, 0x0851, 9, 2);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u32>()); // the other bytes are unknown
    u32 save_low = rp.Pop<u32>();
    u32 save_high = rp.Pop<u32>();
    u32 unknown = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u64 size_limit = rp.Pop<u64>();
    u32 icon_size = rp.Pop<u32>();
    auto icon_buffer = rp.PopMappedBuffer();

    std::vector<u8> icon(icon_size);
    icon_buffer.Read(icon.data(), 0, icon_size);

    FileSys::ArchiveFormatInfo format_info;
    format_info.number_directories = directories;
    format_info.number_files = files;
    format_info.duplicate_data = false;
    format_info.total_size = 0;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.CreateExtSaveData(media_type, save_high, save_low, icon, format_info,
                                       slot->program_id));
    rb.PushMappedBuffer(icon_buffer);

    LOG_DEBUG(Service_FS,
              "called, savedata_high={:08X} savedata_low={:08X} unknown={:08X} "
              "files={:08X} directories={:08X} size_limit={:016x} icon_size={:08X}",
              save_high, save_low, unknown, directories, files, size_limit, icon_size);
}

void FS_USER::DeleteExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x852, 4, 0);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u32>()); // the other bytes are unknown
    u32 save_low = rp.Pop<u32>();
    u32 save_high = rp.Pop<u32>();
    u32 unknown = rp.Pop<u32>(); // TODO(Subv): Figure out what this is

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteExtSaveData(media_type, save_high, save_low));

    LOG_DEBUG(Service_FS,
              "called, save_low={:08X} save_high={:08X} media_type={:08X} unknown={:08X}", save_low,
              save_high, media_type, unknown);
}

void FS_USER::CardSlotIsInserted(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x821, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false);
    LOG_WARNING(Service_FS, "(STUBBED) called");
}

void FS_USER::DeleteSystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x857, 2, 0);
    u32 savedata_high = rp.Pop<u32>();
    u32 savedata_low = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteSystemSaveData(savedata_high, savedata_low));
}

void FS_USER::CreateSystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x856, 9, 0);
    u32 savedata_high = rp.Pop<u32>();
    u32 savedata_low = rp.Pop<u32>();
    u32 total_size = rp.Pop<u32>();
    u32 block_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u32 directory_buckets = rp.Pop<u32>();
    u32 file_buckets = rp.Pop<u32>();
    bool duplicate = rp.Pop<bool>();

    LOG_WARNING(
        Service_FS,
        "(STUBBED) savedata_high={:08X} savedata_low={:08X} total_size={:08X}  block_size={:08X} "
        "directories={} files={} directory_buckets={} file_buckets={} duplicate={}",
        savedata_high, savedata_low, total_size, block_size, directories, files, directory_buckets,
        file_buckets, duplicate);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.CreateSystemSaveData(savedata_high, savedata_low));
}

void FS_USER::CreateLegacySystemSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x810, 8, 0);
    u32 savedata_id = rp.Pop<u32>();
    u32 total_size = rp.Pop<u32>();
    u32 block_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    u32 directory_buckets = rp.Pop<u32>();
    u32 file_buckets = rp.Pop<u32>();
    bool duplicate = rp.Pop<bool>();

    LOG_WARNING(Service_FS,
                "(STUBBED) savedata_id={:08X} total_size={:08X} block_size={:08X} directories={} "
                "files={} directory_buckets={} file_buckets={} duplicate={}",
                savedata_id, total_size, block_size, directories, files, directory_buckets,
                file_buckets, duplicate);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // With this command, the SystemSaveData always has save_high = 0 (Always created in the NAND)
    rb.Push(archives.CreateSystemSaveData(0, savedata_id));
}

void FS_USER::InitializeWithSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x861, 1, 2);
    const u32 version = rp.Pop<u32>();
    u32 pid = rp.PopPID();

    ClientSlot* slot = GetSessionData(ctx.Session());
    slot->program_id = system.Kernel().GetProcessById(pid)->codeset->program_id;

    LOG_WARNING(Service_FS, "(STUBBED) called, version: 0x{:08X}", version);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void FS_USER::SetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x862, 1, 0);

    priority = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_FS, "called priority=0x{:X}", priority);
}

void FS_USER::GetPriority(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x863, 0, 0);

    if (priority == UINT32_MAX) {
        LOG_INFO(Service_FS, "priority was not set, priority=0x{:X}", priority);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(priority);

    LOG_DEBUG(Service_FS, "called priority=0x{:X}", priority);
}

void FS_USER::GetArchiveResource(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x849, 1, 0);
    auto media_type = rp.PopEnum<MediaType>();

    LOG_WARNING(Service_FS, "(STUBBED) called Media type=0x{:08X}", media_type);

    auto resource = archives.GetArchiveResource(media_type);
    if (resource.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(resource.Code());
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(*resource);
}

void FS_USER::GetFormatInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x845, 3, 2);
    const auto archive_id = rp.PopEnum<FS::ArchiveIdCode>();
    const auto archivename_type = rp.PopEnum<FileSys::LowPathType>();
    const auto archivename_size = rp.Pop<u32>();
    std::vector<u8> archivename = rp.PopStaticBuffer();
    ASSERT(archivename.size() == archivename_size);

    const FileSys::Path archive_path(archivename_type, std::move(archivename));

    LOG_DEBUG(Service_FS, "archive_path={}", archive_path.DebugStr());

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    ClientSlot* slot = GetSessionData(ctx.Session());
    auto format_info = archives.GetArchiveFormatInfo(archive_id, archive_path, slot->program_id);
    rb.Push(format_info.Code());
    if (format_info.Failed()) {
        LOG_ERROR(Service_FS, "Failed to retrieve the format info");
        rb.Skip(4, true);
        return;
    }

    rb.Push<u32>(format_info->total_size);
    rb.Push<u32>(format_info->number_directories);
    rb.Push<u32>(format_info->number_files);
    rb.Push<bool>(format_info->duplicate_data != 0);
}

void FS_USER::GetProgramLaunchInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x82F, 1, 0);

    u32 process_id = rp.Pop<u32>();

    LOG_DEBUG(Service_FS, "process_id={}", process_id);

    auto program_info = program_info_map.find(process_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);

    if (program_info == program_info_map.end()) {
        // Note: In this case, the rest of the parameters are not changed but the command header
        // remains the same.
        rb.Push(ResultCode(FileSys::ErrCodes::ArchiveNotMounted, ErrorModule::FS,
                           ErrorSummary::NotFound, ErrorLevel::Status));
        rb.Skip(4, false);
        return;
    }

    rb.Push(RESULT_SUCCESS);
    rb.Push(program_info->second.program_id);
    rb.Push(static_cast<u8>(program_info->second.media_type));

    // TODO(Subv): Find out what this value means.
    rb.Push<u32>(0);
}

void FS_USER::ObsoletedCreateExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x830, 6, 2);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    u32 save_low = rp.Pop<u32>();
    u32 save_high = rp.Pop<u32>();
    u32 icon_size = rp.Pop<u32>();
    u32 directories = rp.Pop<u32>();
    u32 files = rp.Pop<u32>();
    auto icon_buffer = rp.PopMappedBuffer();

    std::vector<u8> icon(icon_size);
    icon_buffer.Read(icon.data(), 0, icon_size);

    FileSys::ArchiveFormatInfo format_info;
    format_info.number_directories = directories;
    format_info.number_files = files;
    format_info.duplicate_data = false;
    format_info.total_size = 0;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    ClientSlot* slot = GetSessionData(ctx.Session());
    rb.Push(archives.CreateExtSaveData(media_type, save_high, save_low, icon, format_info,
                                       slot->program_id));
    rb.PushMappedBuffer(icon_buffer);

    LOG_DEBUG(Service_FS,
              "called, savedata_high={:08X} savedata_low={:08X} "
              "icon_size={:08X} files={:08X} directories={:08X}",
              save_high, save_low, icon_size, directories, files);
}

void FS_USER::ObsoletedDeleteExtSaveData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x835, 2, 0);
    MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    u32 save_low = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(archives.DeleteExtSaveData(media_type, 0, save_low));

    LOG_DEBUG(Service_FS, "called, save_low={:08X} media_type={:08X}", save_low, media_type);
}

void FS_USER::GetSpecialContentIndex(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x83A, 4, 0);
    const MediaType media_type = static_cast<MediaType>(rp.Pop<u8>());
    const u64 title_id = rp.Pop<u64>();
    const auto type = rp.PopEnum<SpecialContentType>();

    LOG_DEBUG(Service_FS, "called, media_type={:08X} type={:08X}, title_id={:016X}", media_type,
              type, title_id);

    ResultVal<u16> index;
    if (media_type == MediaType::GameCard) {
        index = GetSpecialContentIndexFromGameCard(title_id, type);
    } else {
        index = GetSpecialContentIndexFromTMD(media_type, title_id, type);
    }

    if (index.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        rb.Push(index.Unwrap());
    } else {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(index.Code());
    }
}

void FS_USER::GetNumSeeds(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb{ctx, 0x87D, 2, 0};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(FileSys::GetSeedCount());
}

void FS_USER::AddSeed(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx, 0x87A, 6, 0};
    u64 title_id{rp.Pop<u64>()};
    FileSys::Seed::Data seed{rp.PopRaw<FileSys::Seed::Data>()};
    FileSys::AddSeed({title_id, seed, {}});
    IPC::RequestBuilder rb{rp.MakeBuilder(1, 0)};
    rb.Push(RESULT_SUCCESS);
}

void FS_USER::SetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x865, 5, 0);
    u64 value = rp.Pop<u64>();
    u32 secure_value_slot = rp.Pop<u32>();
    u32 unique_id = rp.Pop<u32>();
    u8 title_variation = rp.Pop<u8>();

    // TODO: Generate and Save the Secure Value

    LOG_WARNING(Service_FS,
                "(STUBBED) called, value=0x{:016x} secure_value_slot=0x{:08X} "
                "unqiue_id=0x{:08X} title_variation=0x{:02X}",
                value, secure_value_slot, unique_id, title_variation);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    rb.Push(RESULT_SUCCESS);
}

void FS_USER::GetSaveDataSecureValue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x866, 3, 0);

    u32 secure_value_slot = rp.Pop<u32>();
    u32 unique_id = rp.Pop<u32>();
    u8 title_variation = rp.Pop<u8>();

    LOG_WARNING(
        Service_FS,
        "(STUBBED) called secure_value_slot=0x{:08X} unqiue_id=0x{:08X} title_variation=0x{:02X}",
        secure_value_slot, unique_id, title_variation);

    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);

    rb.Push(RESULT_SUCCESS);

    // TODO: Implement Secure Value Lookup & Generation

    rb.Push<bool>(false); // indicates that the secure value doesn't exist
    rb.Push<u64>(0);      // the secure value
}

void FS_USER::Register(u32 process_id, u64 program_id, const std::string& filepath) {
    const MediaType media_type = GetMediaTypeFromPath(filepath);
    program_info_map.insert_or_assign(process_id, ProgramInfo{program_id, media_type});
    if (media_type == MediaType::GameCard) {
        current_gamecard_path = filepath;
    }
}

std::string FS_USER::GetCurrentGamecardPath() const {
    return current_gamecard_path;
}

ResultVal<u16> FS_USER::GetSpecialContentIndexFromGameCard(u64 title_id, SpecialContentType type) {
    // TODO(B3N30) check if on real 3DS NCSD is checked if partition exists

    if (type > SpecialContentType::DLPChild) {
        // Maybe type 4 is New 3DS update/partition 6 but this needs more research
        // TODO(B3N30): Find correct result code
        return ResultCode(-1);
    }

    switch (type) {
    case SpecialContentType::Update:
        return MakeResult(static_cast<u16>(NCSDContentIndex::Update));
    case SpecialContentType::Manual:
        return MakeResult(static_cast<u16>(NCSDContentIndex::Manual));
    case SpecialContentType::DLPChild:
        return MakeResult(static_cast<u16>(NCSDContentIndex::DLP));
    default:
        UNREACHABLE();
    }
}

ResultVal<u16> FS_USER::GetSpecialContentIndexFromTMD(MediaType media_type, u64 title_id,
                                                      SpecialContentType type) {
    if (type > SpecialContentType::DLPChild) {
        // TODO(B3N30): Find correct result code
        return ResultCode(-1);
    }

    std::string tmd_path = AM::GetTitleMetadataPath(media_type, title_id);

    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) != Loader::ResultStatus::Success || type == SpecialContentType::Update) {
        // TODO(B3N30): Find correct result code
        return ResultCode(-1);
    }

    // TODO(B3N30): Does real 3DS check if content exists in TMD?

    switch (type) {
    case SpecialContentType::Manual:
        return MakeResult(static_cast<u16>(FileSys::TMDContentIndex::Manual));
    case SpecialContentType::DLPChild:
        return MakeResult(static_cast<u16>(FileSys::TMDContentIndex::DLP));
    default:
        ASSERT(false);
    }

    return ResultCode(-1);
}

FS_USER::FS_USER(Core::System& system)
    : ServiceFramework("fs:USER", 30), system(system), archives(system.ArchiveManager()) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 3, 6), nullptr, "Dummy1"},
        {IPC::MakeHeader(0x0401, 3, 4), nullptr, "Control"},
        {IPC::MakeHeader(0x0801, 0, 2), &FS_USER::Initialize, "Initialize"},
        {IPC::MakeHeader(0x0802, 7, 2), &FS_USER::OpenFile, "OpenFile"},
        {IPC::MakeHeader(0x0803, 8, 4), &FS_USER::OpenFileDirectly, "OpenFileDirectly"},
        {IPC::MakeHeader(0x0804, 5, 2), &FS_USER::DeleteFile, "DeleteFile"},
        {IPC::MakeHeader(0x0805, 9, 4), &FS_USER::RenameFile, "RenameFile"},
        {IPC::MakeHeader(0x0806, 5, 2), &FS_USER::DeleteDirectory, "DeleteDirectory"},
        {IPC::MakeHeader(0x0807, 5, 2), &FS_USER::DeleteDirectoryRecursively, "DeleteDirectoryRecursively"},
        {IPC::MakeHeader(0x0808, 8, 2), &FS_USER::CreateFile, "CreateFile"},
        {IPC::MakeHeader(0x0809, 6, 2), &FS_USER::CreateDirectory, "CreateDirectory"},
        {IPC::MakeHeader(0x080A, 9, 4), &FS_USER::RenameDirectory, "RenameDirectory"},
        {IPC::MakeHeader(0x080B, 4, 2), &FS_USER::OpenDirectory, "OpenDirectory"},
        {IPC::MakeHeader(0x080C, 3, 2), &FS_USER::OpenArchive, "OpenArchive"},
        {IPC::MakeHeader(0x080D, 5, 4), nullptr, "ControlArchive"},
        {IPC::MakeHeader(0x080E, 2, 0), &FS_USER::CloseArchive, "CloseArchive"},
        {IPC::MakeHeader(0x080F, 6, 0), &FS_USER::FormatThisUserSaveData, "FormatThisUserSaveData"},
        {IPC::MakeHeader(0x0810, 8, 0), &FS_USER::CreateLegacySystemSaveData, "CreateLegacySystemSaveData"},
        {IPC::MakeHeader(0x0811, 1, 0), nullptr, "DeleteSystemSaveData"},
        {IPC::MakeHeader(0x0812, 2, 0), &FS_USER::GetFreeBytes, "GetFreeBytes"},
        {IPC::MakeHeader(0x0813, 0, 0), nullptr, "GetCardType"},
        {IPC::MakeHeader(0x0814, 0, 0), &FS_USER::GetSdmcArchiveResource, "GetSdmcArchiveResource"},
        {IPC::MakeHeader(0x0815, 0, 0), &FS_USER::GetNandArchiveResource, "GetNandArchiveResource"},
        {IPC::MakeHeader(0x0816, 0, 0), nullptr, "GetSdmcFatfsError"},
        {IPC::MakeHeader(0x0817, 0, 0), &FS_USER::IsSdmcDetected, "IsSdmcDetected"},
        {IPC::MakeHeader(0x0818, 0, 0), &FS_USER::IsSdmcWriteable, "IsSdmcWritable"},
        {IPC::MakeHeader(0x0819, 1, 2), nullptr, "GetSdmcCid"},
        {IPC::MakeHeader(0x081A, 1, 2), nullptr, "GetNandCid"},
        {IPC::MakeHeader(0x081B, 0, 0), nullptr, "GetSdmcSpeedInfo"},
        {IPC::MakeHeader(0x081C, 0, 0), nullptr, "GetNandSpeedInfo"},
        {IPC::MakeHeader(0x081D, 1, 2), nullptr, "GetSdmcLog"},
        {IPC::MakeHeader(0x081E, 1, 2), nullptr, "GetNandLog"},
        {IPC::MakeHeader(0x081F, 0, 0), nullptr, "ClearSdmcLog"},
        {IPC::MakeHeader(0x0820, 0, 0), nullptr, "ClearNandLog"},
        {IPC::MakeHeader(0x0821, 0, 0), &FS_USER::CardSlotIsInserted, "CardSlotIsInserted"},
        {IPC::MakeHeader(0x0822, 0, 0), nullptr, "CardSlotPowerOn"},
        {IPC::MakeHeader(0x0823, 0, 0), nullptr, "CardSlotPowerOff"},
        {IPC::MakeHeader(0x0824, 0, 0), nullptr, "CardSlotGetCardIFPowerStatus"},
        {IPC::MakeHeader(0x0825, 1, 0), nullptr, "CardNorDirectCommand"},
        {IPC::MakeHeader(0x0826, 2, 0), nullptr, "CardNorDirectCommandWithAddress"},
        {IPC::MakeHeader(0x0827, 2, 2), nullptr, "CardNorDirectRead"},
        {IPC::MakeHeader(0x0828, 3, 2), nullptr, "CardNorDirectReadWithAddress"},
        {IPC::MakeHeader(0x0829, 2, 2), nullptr, "CardNorDirectWrite"},
        {IPC::MakeHeader(0x082A, 3, 2), nullptr, "CardNorDirectWriteWithAddress"},
        {IPC::MakeHeader(0x082B, 3, 2), nullptr, "CardNorDirectRead_4xIO"},
        {IPC::MakeHeader(0x082C, 2, 2), nullptr, "CardNorDirectCpuWriteWithoutVerify"},
        {IPC::MakeHeader(0x082D, 1, 0), nullptr, "CardNorDirectSectorEraseWithoutVerify"},
        {IPC::MakeHeader(0x082E, 1, 0), nullptr, "GetProductInfo"},
        {IPC::MakeHeader(0x082F, 1, 0), &FS_USER::GetProgramLaunchInfo, "GetProgramLaunchInfo"},
        {IPC::MakeHeader(0x0830, 6, 2), &FS_USER::ObsoletedCreateExtSaveData, "Obsoleted_3_0_CreateExtSaveData"},
        {IPC::MakeHeader(0x0831, 6, 0), nullptr, "CreateSharedExtSaveData"},
        {IPC::MakeHeader(0x0832, 4, 2), nullptr, "ReadExtSaveDataIcon"},
        {IPC::MakeHeader(0x0833, 2, 2), nullptr, "EnumerateExtSaveData"},
        {IPC::MakeHeader(0x0834, 2, 2), nullptr, "EnumerateSharedExtSaveData"},
        {IPC::MakeHeader(0x0835, 2, 0), &FS_USER::ObsoletedDeleteExtSaveData, "Obsoleted_3_0_DeleteExtSaveData"},
        {IPC::MakeHeader(0x0836, 2, 0), nullptr, "DeleteSharedExtSaveData"},
        {IPC::MakeHeader(0x0837, 1, 0), nullptr, "SetCardSpiBaudRate"},
        {IPC::MakeHeader(0x0838, 1, 0), nullptr, "SetCardSpiBusMode"},
        {IPC::MakeHeader(0x0839, 0, 0), nullptr, "SendInitializeInfoTo9"},
        {IPC::MakeHeader(0x083A, 4, 0), &FS_USER::GetSpecialContentIndex, "GetSpecialContentIndex"},
        {IPC::MakeHeader(0x083B, 3, 2), nullptr, "GetLegacyRomHeader"},
        {IPC::MakeHeader(0x083C, 3, 2), nullptr, "GetLegacyBannerData"},
        {IPC::MakeHeader(0x083D, 4, 0), nullptr, "CheckAuthorityToAccessExtSaveData"},
        {IPC::MakeHeader(0x083E, 3, 2), nullptr, "QueryTotalQuotaSize"},
        {IPC::MakeHeader(0x083F, 3, 0), nullptr, "GetExtDataBlockSize"},
        {IPC::MakeHeader(0x0840, 1, 0), nullptr, "AbnegateAccessRight"},
        {IPC::MakeHeader(0x0841, 0, 0), nullptr, "DeleteSdmcRoot"},
        {IPC::MakeHeader(0x0842, 1, 0), nullptr, "DeleteAllExtSaveDataOnNand"},
        {IPC::MakeHeader(0x0843, 0, 0), nullptr, "InitializeCtrFileSystem"},
        {IPC::MakeHeader(0x0844, 0, 0), nullptr, "CreateSeed"},
        {IPC::MakeHeader(0x0845, 3, 2), &FS_USER::GetFormatInfo, "GetFormatInfo"},
        {IPC::MakeHeader(0x0846, 4, 2), nullptr, "GetLegacyRomHeader2"},
        {IPC::MakeHeader(0x0847, 6, 0), nullptr, "FormatCtrCardUserSaveData"},
        {IPC::MakeHeader(0x0848, 1, 2), nullptr, "GetSdmcCtrRootPath"},
        {IPC::MakeHeader(0x0849, 1, 0), &FS_USER::GetArchiveResource, "GetArchiveResource"},
        {IPC::MakeHeader(0x084A, 0, 2), nullptr, "ExportIntegrityVerificationSeed"},
        {IPC::MakeHeader(0x084B, 0, 2), nullptr, "ImportIntegrityVerificationSeed"},
        {IPC::MakeHeader(0x084C, 9, 2), &FS_USER::FormatSaveData, "FormatSaveData"},
        {IPC::MakeHeader(0x084D, 4, 2), nullptr, "GetLegacySubBannerData"},
        {IPC::MakeHeader(0x084E, 13, 2), nullptr, "UpdateSha256Context"},
        {IPC::MakeHeader(0x084F, 4, 2), nullptr, "ReadSpecialFile"},
        {IPC::MakeHeader(0x0850, 1, 0), nullptr, "GetSpecialFileSize"},
        {IPC::MakeHeader(0x0851, 9, 2), &FS_USER::CreateExtSaveData, "CreateExtSaveData"},
        {IPC::MakeHeader(0x0852, 4, 0), &FS_USER::DeleteExtSaveData, "DeleteExtSaveData"},
        {IPC::MakeHeader(0x0853, 5, 2), nullptr, "ReadExtSaveDataIcon"},
        {IPC::MakeHeader(0x0854, 3, 0), nullptr, "GetExtDataBlockSize"},
        {IPC::MakeHeader(0x0855, 4, 2), nullptr, "EnumerateExtSaveData"},
        {IPC::MakeHeader(0x0856, 9, 0), &FS_USER::CreateSystemSaveData, "CreateSystemSaveData"},
        {IPC::MakeHeader(0x0857, 2, 0), &FS_USER::DeleteSystemSaveData, "DeleteSystemSaveData"},
        {IPC::MakeHeader(0x0858, 0, 0), nullptr, "StartDeviceMoveAsSource"},
        {IPC::MakeHeader(0x0859, 8, 0), nullptr, "StartDeviceMoveAsDestination"},
        {IPC::MakeHeader(0x085A, 3, 0), nullptr, "SetArchivePriority"},
        {IPC::MakeHeader(0x085B, 2, 0), nullptr, "GetArchivePriority"},
        {IPC::MakeHeader(0x085C, 3, 0), nullptr, "SetCtrCardLatencyParameter"},
        {IPC::MakeHeader(0x085D, 7, 0), nullptr, "SetFsCompatibilityInfo"},
        {IPC::MakeHeader(0x085E, 1, 0), nullptr, "ResetCardCompatibilityParameter"},
        {IPC::MakeHeader(0x085F, 1, 0), nullptr, "SwitchCleanupInvalidSaveData"},
        {IPC::MakeHeader(0x0860, 1, 2), nullptr, "EnumerateSystemSaveData"},
        {IPC::MakeHeader(0x0861, 1, 2), &FS_USER::InitializeWithSdkVersion, "InitializeWithSdkVersion"},
        {IPC::MakeHeader(0x0862, 1, 0), &FS_USER::SetPriority, "SetPriority"},
        {IPC::MakeHeader(0x0863, 0, 0), &FS_USER::GetPriority, "GetPriority"},
        {IPC::MakeHeader(0x0864, 0, 0), nullptr, "GetNandInfo"},
        {IPC::MakeHeader(0x0865, 5, 0), &FS_USER::SetSaveDataSecureValue, "SetSaveDataSecureValue"},
        {IPC::MakeHeader(0x0866, 3, 0), &FS_USER::GetSaveDataSecureValue, "GetSaveDataSecureValue"},
        {IPC::MakeHeader(0x0867, 3, 4), nullptr, "ControlSecureSave"},
        {IPC::MakeHeader(0x0868, 0, 0), nullptr, "GetMediaType"},
        {IPC::MakeHeader(0x0869, 0, 0), nullptr, "GetNandEraseCount"},
        {IPC::MakeHeader(0x086A, 2, 2), nullptr, "ReadNandReport"},
        {IPC::MakeHeader(0x087A, 6, 0), &FS_USER::AddSeed, "AddSeed"},
        {IPC::MakeHeader(0x087D, 0, 0), &FS_USER::GetNumSeeds, "GetNumSeeds"},
        {IPC::MakeHeader(0x0886, 3, 0), nullptr, "CheckUpdatedDat"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<FS_USER>(system)->InstallAsService(service_manager);
}
} // namespace Service::FS
