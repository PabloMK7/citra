// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/file_util.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FS_User

namespace Service {
namespace FS {

static ArchiveHandle MakeArchiveHandle(u32 low_word, u32 high_word) {
    return (u64)low_word | ((u64)high_word << 32);
}

static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(Link Mauve): check the behavior when cmd_buff[1] isn't 32, as per
    // http://3dbrew.org/wiki/FS:Initialize#Request
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_FS, "called");
}

/**
 * FS_User::OpenFile service function
 *  Inputs:
 *      1 : Transaction
 *      2 : Archive handle lower word
 *      3 : Archive handle upper word
 *      4 : Low path type
 *      5 : Low path size
 *      6 : Open flags
 *      7 : Attributes
 *      8 : (LowPathSize << 14) | 2
 *      9 : Low path data pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : File handle
 */
static void OpenFile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto filename_type    = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 filename_size     = cmd_buff[5];
    FileSys::Mode mode; mode.hex = cmd_buff[6];
    u32 attributes        = cmd_buff[7]; // TODO(Link Mauve): do something with those attributes.
    u32 filename_ptr      = cmd_buff[9];
    FileSys::Path file_path(filename_type, filename_size, filename_ptr);

    LOG_DEBUG(Service_FS, "path=%s, mode=%d attrs=%u", file_path.DebugStr().c_str(), mode.hex, attributes);

    ResultVal<Handle> handle = OpenFileFromArchive(archive_handle, file_path, mode);
    cmd_buff[1] = handle.Code().raw;
    if (handle.Succeeded()) {
        cmd_buff[3] = *handle;
    } else {
        cmd_buff[3] = 0;
        LOG_ERROR(Service_FS, "failed to get a handle for file %s", file_path.DebugStr().c_str());
    }
}

/**
 * FS_User::OpenFileDirectly service function
 *  Inputs:
 *      1 : Transaction
 *      2 : Archive ID
 *      3 : Archive low path type
 *      4 : Archive low path size
 *      5 : File low path type
 *      6 : File low path size
 *      7 : Flags
 *      8 : Attributes
 *      9 : (ArchiveLowPathSize << 14) | 0x802
 *      10 : Archive low path
 *      11 : (FileLowPathSize << 14) | 2
 *      12 : File low path
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : File handle
 */
static void OpenFileDirectly(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    auto archive_id       = static_cast<FS::ArchiveIdCode>(cmd_buff[2]);
    auto archivename_type = static_cast<FileSys::LowPathType>(cmd_buff[3]);
    u32 archivename_size  = cmd_buff[4];
    auto filename_type    = static_cast<FileSys::LowPathType>(cmd_buff[5]);
    u32 filename_size     = cmd_buff[6];
    FileSys::Mode mode; mode.hex = cmd_buff[7];
    u32 attributes        = cmd_buff[8]; // TODO(Link Mauve): do something with those attributes.
    u32 archivename_ptr   = cmd_buff[10];
    u32 filename_ptr      = cmd_buff[12];
    FileSys::Path archive_path(archivename_type, archivename_size, archivename_ptr);
    FileSys::Path file_path(filename_type, filename_size, filename_ptr);

    LOG_DEBUG(Service_FS, "archive_path=%s file_path=%s, mode=%u attributes=%d",
              archive_path.DebugStr().c_str(), file_path.DebugStr().c_str(), mode.hex, attributes);

    if (archive_path.GetType() != FileSys::Empty) {
        LOG_ERROR(Service_FS, "archive LowPath type other than empty is currently unsupported");
        cmd_buff[1] = UnimplementedFunction(ErrorModule::FS).raw;
        cmd_buff[3] = 0;
        return;
    }

    ResultVal<ArchiveHandle> archive_handle = OpenArchive(archive_id);
    if (archive_handle.Failed()) {
        LOG_ERROR(Service_FS, "failed to get a handle for archive");
        cmd_buff[1] = archive_handle.Code().raw;
        cmd_buff[3] = 0;
        return;
    }
    SCOPE_EXIT({ CloseArchive(*archive_handle); });

    ResultVal<Handle> handle = OpenFileFromArchive(*archive_handle, file_path, mode);
    cmd_buff[1] = handle.Code().raw;
    if (handle.Succeeded()) {
        cmd_buff[3] = *handle;
    } else {
        cmd_buff[3] = 0;
        LOG_ERROR(Service_FS, "failed to get a handle for file %s", file_path.DebugStr().c_str());
    }
}

/*
 * FS_User::DeleteFile service function
 *  Inputs:
 *      2 : Archive handle lower word
 *      3 : Archive handle upper word
 *      4 : File path string type
 *      5 : File path string size
 *      7 : File path string data
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void DeleteFile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto filename_type    = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 filename_size     = cmd_buff[5];
    u32 filename_ptr      = cmd_buff[7];

    FileSys::Path file_path(filename_type, filename_size, filename_ptr);

    LOG_DEBUG(Service_FS, "type=%d size=%d data=%s",
              filename_type, filename_size, file_path.DebugStr().c_str());

    cmd_buff[1] = DeleteFileFromArchive(archive_handle, file_path).raw;
}

/*
 * FS_User::RenameFile service function
 *  Inputs:
 *      2 : Source archive handle lower word
 *      3 : Source archive handle upper word
 *      4 : Source file path type
 *      5 : Source file path size
 *      6 : Dest archive handle lower word
 *      7 : Dest archive handle upper word
 *      8 : Dest file path type
 *      9 : Dest file path size
 *      11: Source file path string data
 *      13: Dest file path string
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RenameFile(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle src_archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto src_filename_type     = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 src_filename_size      = cmd_buff[5];
    ArchiveHandle dest_archive_handle = MakeArchiveHandle(cmd_buff[6], cmd_buff[7]);;
    auto dest_filename_type    = static_cast<FileSys::LowPathType>(cmd_buff[8]);
    u32 dest_filename_size     = cmd_buff[9];
    u32 src_filename_ptr       = cmd_buff[11];
    u32 dest_filename_ptr      = cmd_buff[13];

    FileSys::Path src_file_path(src_filename_type, src_filename_size, src_filename_ptr);
    FileSys::Path dest_file_path(dest_filename_type, dest_filename_size, dest_filename_ptr);

    LOG_DEBUG(Service_FS, "src_type=%d src_size=%d src_data=%s dest_type=%d dest_size=%d dest_data=%s",
              src_filename_type, src_filename_size, src_file_path.DebugStr().c_str(),
              dest_filename_type, dest_filename_size, dest_file_path.DebugStr().c_str());

    cmd_buff[1] = RenameFileBetweenArchives(src_archive_handle, src_file_path, dest_archive_handle, dest_file_path).raw;
}

/*
 * FS_User::DeleteDirectory service function
 *  Inputs:
 *      2 : Archive handle lower word
 *      3 : Archive handle upper word
 *      4 : Directory path string type
 *      5 : Directory path string size
 *      7 : Directory path string data
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void DeleteDirectory(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto dirname_type     = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 dirname_size      = cmd_buff[5];
    u32 dirname_ptr       = cmd_buff[7];

    FileSys::Path dir_path(dirname_type, dirname_size, dirname_ptr);

    LOG_DEBUG(Service_FS, "type=%d size=%d data=%s",
              dirname_type, dirname_size, dir_path.DebugStr().c_str());
    
    cmd_buff[1] = DeleteDirectoryFromArchive(archive_handle, dir_path).raw;
}

/*
 * FS_User::CreateDirectory service function
 *  Inputs:
 *      2 : Archive handle lower word
 *      3 : Archive handle upper word
 *      4 : Directory path string type
 *      5 : Directory path string size
 *      8 : Directory path string data
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void CreateDirectory(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto dirname_type = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 dirname_size = cmd_buff[5];
    u32 dirname_ptr = cmd_buff[8];

    FileSys::Path dir_path(dirname_type, dirname_size, dirname_ptr);

    LOG_DEBUG(Service_FS, "type=%d size=%d data=%s", dirname_type, dirname_size, dir_path.DebugStr().c_str());

    cmd_buff[1] = CreateDirectoryFromArchive(archive_handle, dir_path).raw;
}

/*
 * FS_User::RenameDirectory service function
 *  Inputs:
 *      2 : Source archive handle lower word
 *      3 : Source archive handle upper word
 *      4 : Source dir path type
 *      5 : Source dir path size
 *      6 : Dest archive handle lower word
 *      7 : Dest archive handle upper word
 *      8 : Dest dir path type
 *      9 : Dest dir path size
 *      11: Source dir path string data
 *      13: Dest dir path string
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RenameDirectory(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle src_archive_handle = MakeArchiveHandle(cmd_buff[2], cmd_buff[3]);
    auto src_dirname_type      = static_cast<FileSys::LowPathType>(cmd_buff[4]);
    u32 src_dirname_size       = cmd_buff[5];
    ArchiveHandle dest_archive_handle = MakeArchiveHandle(cmd_buff[6], cmd_buff[7]);
    auto dest_dirname_type     = static_cast<FileSys::LowPathType>(cmd_buff[8]);
    u32 dest_dirname_size      = cmd_buff[9];
    u32 src_dirname_ptr        = cmd_buff[11];
    u32 dest_dirname_ptr       = cmd_buff[13];

    FileSys::Path src_dir_path(src_dirname_type, src_dirname_size, src_dirname_ptr);
    FileSys::Path dest_dir_path(dest_dirname_type, dest_dirname_size, dest_dirname_ptr);

    LOG_DEBUG(Service_FS, "src_type=%d src_size=%d src_data=%s dest_type=%d dest_size=%d dest_data=%s",
              src_dirname_type, src_dirname_size, src_dir_path.DebugStr().c_str(),
              dest_dirname_type, dest_dirname_size, dest_dir_path.DebugStr().c_str());

    cmd_buff[1] = RenameDirectoryBetweenArchives(src_archive_handle, src_dir_path, dest_archive_handle, dest_dir_path).raw;
}

/**
 * FS_User::OpenDirectory service function
 *  Inputs:
 *      1 : Archive handle low word
 *      2 : Archive handle high word
 *      3 : Low path type
 *      4 : Low path size
 *      7 : (LowPathSize << 14) | 2
 *      8 : Low path data pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : Directory handle
 */
static void OpenDirectory(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[1], cmd_buff[2]);
    auto dirname_type = static_cast<FileSys::LowPathType>(cmd_buff[3]);
    u32 dirname_size = cmd_buff[4];
    u32 dirname_ptr = cmd_buff[6];

    FileSys::Path dir_path(dirname_type, dirname_size, dirname_ptr);

    LOG_DEBUG(Service_FS, "type=%d size=%d data=%s", dirname_type, dirname_size, dir_path.DebugStr().c_str());

    ResultVal<Handle> handle = OpenDirectoryFromArchive(archive_handle, dir_path);
    cmd_buff[1] = handle.Code().raw;
    if (handle.Succeeded()) {
        cmd_buff[3] = *handle;
    } else {
        LOG_ERROR(Service_FS, "failed to get a handle for directory");
    }
}

/**
 * FS_User::OpenArchive service function
 *  Inputs:
 *      1 : Archive ID
 *      2 : Archive low path type
 *      3 : Archive low path size
 *      4 : (LowPathSize << 14) | 2
 *      5 : Archive low path
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Archive handle lower word (unused)
 *      3 : Archive handle upper word (same as file handle)
 */
static void OpenArchive(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    auto archive_id       = static_cast<FS::ArchiveIdCode>(cmd_buff[1]);
    auto archivename_type = static_cast<FileSys::LowPathType>(cmd_buff[2]);
    u32 archivename_size  = cmd_buff[3];
    u32 archivename_ptr   = cmd_buff[5];
    FileSys::Path archive_path(archivename_type, archivename_size, archivename_ptr);

    LOG_DEBUG(Service_FS, "archive_path=%s", archive_path.DebugStr().c_str());

    if (archive_path.GetType() != FileSys::Empty) {
        LOG_ERROR(Service_FS, "archive LowPath type other than empty is currently unsupported");
        cmd_buff[1] = UnimplementedFunction(ErrorModule::FS).raw;
        return;
    }

    ResultVal<ArchiveHandle> handle = OpenArchive(archive_id);
    cmd_buff[1] = handle.Code().raw;
    if (handle.Succeeded()) {
        cmd_buff[2] = *handle & 0xFFFFFFFF;
        cmd_buff[3] = (*handle >> 32) & 0xFFFFFFFF;
    } else {
        cmd_buff[2] = cmd_buff[3] = 0;
        LOG_ERROR(Service_FS, "failed to get a handle for archive");
    }
}

/**
 * FS_User::CloseArchive service function
 *  Inputs:
 *      0 : 0x080E0080
 *      1 : Archive handle low word
 *      2 : Archive handle high word
 *  Outputs:
 *      0 : ??? TODO(yuriks): Verify return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void CloseArchive(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ArchiveHandle archive_handle = MakeArchiveHandle(cmd_buff[1], cmd_buff[2]);
    cmd_buff[1] = CloseArchive(archive_handle).raw;
}

/*
* FS_User::IsSdmcDetected service function
*  Outputs:
*      1 : Result of function, 0 on success, otherwise error code
*      2 : Whether the Sdmc could be detected
*/
static void IsSdmcDetected(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0;
    cmd_buff[2] = Settings::values.use_virtual_sd ? 1 : 0;

    LOG_DEBUG(Service_FS, "called");
}

/**
 * FS_User::FormatSaveData service function
 *  Inputs:
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void FormatSaveData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    LOG_DEBUG(Service_FS, "(STUBBED)");

    // TODO(Subv): Find out what the inputs and outputs of this function are

    cmd_buff[1] = FormatSaveData().raw;
}

/**
 * FS_User::FormatThisUserSaveData service function
 *  Inputs:
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void FormatThisUserSaveData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    LOG_DEBUG(Service_FS, "(STUBBED)");

    // TODO(Subv): Find out what the inputs and outputs of this function are

    cmd_buff[1] = FormatSaveData().raw;
}

const FSUserInterface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr,               "Dummy1"},
    {0x040100C4, nullptr,               "Control"},
    {0x08010002, Initialize,            "Initialize"},
    {0x080201C2, OpenFile,              "OpenFile"},
    {0x08030204, OpenFileDirectly,      "OpenFileDirectly"},
    {0x08040142, DeleteFile,            "DeleteFile"},
    {0x08050244, RenameFile,            "RenameFile"},
    {0x08060142, DeleteDirectory,       "DeleteDirectory"},
    {0x08070142, nullptr,               "DeleteDirectoryRecursively"},
    {0x08080202, nullptr,               "CreateFile"},
    {0x08090182, CreateDirectory,       "CreateDirectory"},
    {0x080A0244, RenameDirectory,       "RenameDirectory"},
    {0x080B0102, OpenDirectory,         "OpenDirectory"},
    {0x080C00C2, OpenArchive,           "OpenArchive"},
    {0x080D0144, nullptr,               "ControlArchive"},
    {0x080E0080, CloseArchive,          "CloseArchive"},
    {0x080F0180, FormatThisUserSaveData,"FormatThisUserSaveData"},
    {0x08100200, nullptr,               "CreateSystemSaveData"},
    {0x08110040, nullptr,               "DeleteSystemSaveData"},
    {0x08120080, nullptr,               "GetFreeBytes"},
    {0x08130000, nullptr,               "GetCardType"},
    {0x08140000, nullptr,               "GetSdmcArchiveResource"},
    {0x08150000, nullptr,               "GetNandArchiveResource"},
    {0x08160000, nullptr,               "GetSdmcFatfsErro"},
    {0x08170000, IsSdmcDetected,        "IsSdmcDetected"},
    {0x08180000, nullptr,               "IsSdmcWritable"},
    {0x08190042, nullptr,               "GetSdmcCid"},
    {0x081A0042, nullptr,               "GetNandCid"},
    {0x081B0000, nullptr,               "GetSdmcSpeedInfo"},
    {0x081C0000, nullptr,               "GetNandSpeedInfo"},
    {0x081D0042, nullptr,               "GetSdmcLog"},
    {0x081E0042, nullptr,               "GetNandLog"},
    {0x081F0000, nullptr,               "ClearSdmcLog"},
    {0x08200000, nullptr,               "ClearNandLog"},
    {0x08210000, nullptr,               "CardSlotIsInserted"},
    {0x08220000, nullptr,               "CardSlotPowerOn"},
    {0x08230000, nullptr,               "CardSlotPowerOff"},
    {0x08240000, nullptr,               "CardSlotGetCardIFPowerStatus"},
    {0x08250040, nullptr,               "CardNorDirectCommand"},
    {0x08260080, nullptr,               "CardNorDirectCommandWithAddress"},
    {0x08270082, nullptr,               "CardNorDirectRead"},
    {0x082800C2, nullptr,               "CardNorDirectReadWithAddress"},
    {0x08290082, nullptr,               "CardNorDirectWrite"},
    {0x082A00C2, nullptr,               "CardNorDirectWriteWithAddress"},
    {0x082B00C2, nullptr,               "CardNorDirectRead_4xIO"},
    {0x082C0082, nullptr,               "CardNorDirectCpuWriteWithoutVerify"},
    {0x082D0040, nullptr,               "CardNorDirectSectorEraseWithoutVerify"},
    {0x082E0040, nullptr,               "GetProductInfo"},
    {0x082F0040, nullptr,               "GetProgramLaunchInfo"},
    {0x08300182, nullptr,               "CreateExtSaveData"},
    {0x08310180, nullptr,               "CreateSharedExtSaveData"},
    {0x08320102, nullptr,               "ReadExtSaveDataIcon"},
    {0x08330082, nullptr,               "EnumerateExtSaveData"},
    {0x08340082, nullptr,               "EnumerateSharedExtSaveData"},
    {0x08350080, nullptr,               "DeleteExtSaveData"},
    {0x08360080, nullptr,               "DeleteSharedExtSaveData"},
    {0x08370040, nullptr,               "SetCardSpiBaudRate"},
    {0x08380040, nullptr,               "SetCardSpiBusMode"},
    {0x08390000, nullptr,               "SendInitializeInfoTo9"},
    {0x083A0100, nullptr,               "GetSpecialContentIndex"},
    {0x083B00C2, nullptr,               "GetLegacyRomHeader"},
    {0x083C00C2, nullptr,               "GetLegacyBannerData"},
    {0x083D0100, nullptr,               "CheckAuthorityToAccessExtSaveData"},
    {0x083E00C2, nullptr,               "QueryTotalQuotaSize"},
    {0x083F00C0, nullptr,               "GetExtDataBlockSize"},
    {0x08400040, nullptr,               "AbnegateAccessRight"},
    {0x08410000, nullptr,               "DeleteSdmcRoot"},
    {0x08420040, nullptr,               "DeleteAllExtSaveDataOnNand"},
    {0x08430000, nullptr,               "InitializeCtrFileSystem"},
    {0x08440000, nullptr,               "CreateSeed"},
    {0x084500C2, nullptr,               "GetFormatInfo"},
    {0x08460102, nullptr,               "GetLegacyRomHeader2"},
    {0x08470180, nullptr,               "FormatCtrCardUserSaveData"},
    {0x08480042, nullptr,               "GetSdmcCtrRootPath"},
    {0x08490040, nullptr,               "GetArchiveResource"},
    {0x084A0002, nullptr,               "ExportIntegrityVerificationSeed"},
    {0x084B0002, nullptr,               "ImportIntegrityVerificationSeed"},
    {0x084C0242, FormatSaveData,        "FormatSaveData"},
    {0x084D0102, nullptr,               "GetLegacySubBannerData"},
    {0x084E0342, nullptr,               "UpdateSha256Context"},
    {0x084F0102, nullptr,               "ReadSpecialFile"},
    {0x08500040, nullptr,               "GetSpecialFileSize"},
    {0x08580000, nullptr,               "GetMovableSedHashedKeyYRandomData"},
    {0x08610042, nullptr,               "InitializeWithSdkVersion"},
    {0x08620040, nullptr,               "SetPriority"},
    {0x08630000, nullptr,               "GetPriority"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

FSUserInterface::FSUserInterface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

FSUserInterface::~FSUserInterface() {
}

} // namespace FS
} // namespace Service
