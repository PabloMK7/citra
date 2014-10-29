// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"

#include "fs_user.h"
#include "core/settings.h"
#include "core/hle/kernel/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FS_User

namespace FS_User {

// Command to access archive file
enum class LowPathType : u32 {
    Invalid = 0,
    Empty   = 1,
    Binary  = 2,
    Char    = 3,
    Wchar   = 4
};

std::string GetStringFromCmdBuff(const u32 pointer, const u32 size) {
    auto data = reinterpret_cast<const char*>(Memory::GetPointer(pointer));
    return std::string(data, size - 1);
}

// We currently return 0 for success and -1 for failure in cmd_buff[1].  -1 was chosen because it
// puts all the sections of the http://3dbrew.org/wiki/Error_codes to something non-zero, to make
// sure we don't mislead the application into thinking something worked.

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    // TODO(Link Mauve): check the behavior when cmd_buff[1] isn't 32, as per
    // http://3dbrew.org/wiki/FS:Initialize#Request
    cmd_buff[1] = 0;

    DEBUG_LOG(KERNEL, "called");
}

void OpenFile(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    u32 transaction = cmd_buff[1];
    // TODO(Link Mauve): cmd_buff[2], aka archive handle lower word, isn't used according to
    // 3dmoo's or ctrulib's implementations.  Triple check if it's really the case.
    Handle archive_handle = static_cast<Handle>(cmd_buff[3]);
    LowPathType type = static_cast<LowPathType>(cmd_buff[4]);
    u32 size = cmd_buff[5];
    FileSys::Mode mode; mode.hex = cmd_buff[6];
    u32 attributes = cmd_buff[7]; // TODO(Link Mauve): do something with those attributes.
    u32 pointer = cmd_buff[9];

    if (type != LowPathType::Char) {
        ERROR_LOG(KERNEL, "file LowPath type other than char is currently unsupported");
        cmd_buff[1] = -1;
        return;
    }

    std::string file_name = GetStringFromCmdBuff(pointer, size);

    DEBUG_LOG(KERNEL, "type=%d size=%d mode=%d attrs=%d data=%s", type, size, mode, attributes, file_name.c_str());

    Handle handle = Kernel::OpenFileFromArchive(archive_handle, file_name, mode);
    if (handle) {
        cmd_buff[1] = 0;
        cmd_buff[3] = handle;
    } else {
        ERROR_LOG(KERNEL, "failed to get a handle for file %s", file_name.c_str());
        // TODO(Link Mauve): check for the actual error values, this one was just chosen arbitrarily.
        cmd_buff[1] = -1;
    }

    DEBUG_LOG(KERNEL, "called");
}

void OpenFileDirectly(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    u32 transaction = cmd_buff[1];
    auto archive_id = static_cast<FileSys::Archive::IdCode>(cmd_buff[2]);
    LowPathType archive_type = static_cast<LowPathType>(cmd_buff[3]);
    u32 archive_size = cmd_buff[4];
    LowPathType file_type = static_cast<LowPathType>(cmd_buff[5]);
    u32 size = cmd_buff[6];
    FileSys::Mode mode; mode.hex = cmd_buff[7];
    u32 attributes = cmd_buff[8]; // TODO(Link Mauve): do something with those attributes.
    u32 archive_pointer = cmd_buff[10];
    u32 pointer = cmd_buff[12];

    if (archive_type != LowPathType::Empty) {
        ERROR_LOG(KERNEL, "archive LowPath type other than empty is currently unsupported");
        cmd_buff[1] = -1;
        return;
    }

    std::string archive_name = GetStringFromCmdBuff(archive_pointer, archive_size);
    std::string file_name = GetStringFromCmdBuff(pointer, size);

    DEBUG_LOG(KERNEL, "archive_type=%d archive_size=%d archive_data=%s "
                      "file_type=%d file_size=%d file_mode=%d file_attrs=%d file_data=%s",
              archive_type, archive_size, archive_name.c_str(),
              file_type, size, mode, attributes, file_name.c_str());

    // TODO(Link Mauve): check if we should even get a handle for the archive, and don't leak it.
    Handle archive_handle = Kernel::OpenArchive(archive_id);
    if (archive_handle) {
        cmd_buff[1] = 0;
        // cmd_buff[2] isn't used according to 3dmoo's implementation.
        cmd_buff[3] = archive_handle;
    } else {
        ERROR_LOG(KERNEL, "failed to get a handle for archive %s", archive_name.c_str());
        // TODO(Link Mauve): check for the actual error values, this one was just chosen arbitrarily.
        cmd_buff[1] = -1;
        return;
    }

    if (file_type != LowPathType::Char) {
        WARN_LOG(KERNEL, "file LowPath type other than char is currently unsupported; returning archive handle instead");
        return;
    }

    Handle handle = Kernel::OpenFileFromArchive(archive_handle, file_name, mode);
    if (handle) {
        cmd_buff[1] = 0;
        cmd_buff[3] = handle;
    } else {
        ERROR_LOG(KERNEL, "failed to get a handle for file %s", file_name.c_str());
        // TODO(Link Mauve): check for the actual error values, this one was just chosen arbitrarily.
        cmd_buff[1] = -1;
    }

    DEBUG_LOG(KERNEL, "called");
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
void CreateDirectory(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    // TODO: cmd_buff[2], aka archive handle lower word, isn't used according to
    // 3dmoo's or ctrulib's implementations.  Triple check if it's really the case.
    Handle archive_handle = static_cast<Handle>(cmd_buff[3]);
    LowPathType type = static_cast<LowPathType>(cmd_buff[4]);
    u32 name_size = cmd_buff[5];
    u32 name_offset = cmd_buff[8];

    if (type != LowPathType::Char) {
        ERROR_LOG(KERNEL, "directory LowPath type other than char is currently unsupported");
        cmd_buff[1] = -1;
        return;
    }

    std::string dir_name = GetStringFromCmdBuff(name_offset, name_size);

    DEBUG_LOG(KERNEL, "type=%d size=%d data=%s", type, name_size, dir_name.c_str());

    cmd_buff[1] = Kernel::CreateDirectoryFromArchive(archive_handle, dir_name);

    DEBUG_LOG(KERNEL, "called");
}

void OpenDirectory(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    // TODO(Link Mauve): cmd_buff[2], aka archive handle lower word, isn't used according to
    // 3dmoo's or ctrulib's implementations.  Triple check if it's really the case.
    Handle archive_handle = static_cast<Handle>(cmd_buff[2]);
    LowPathType type = static_cast<LowPathType>(cmd_buff[3]);
    u32 size = cmd_buff[4];
    u32 pointer = cmd_buff[6];

    if (type != LowPathType::Char) {
        ERROR_LOG(KERNEL, "directory LowPath type other than char is currently unsupported");
        cmd_buff[1] = -1;
        return;
    }

    std::string dir_name = GetStringFromCmdBuff(pointer, size);

    DEBUG_LOG(KERNEL, "type=%d size=%d data=%s", type, size, dir_name.c_str());

    Handle handle = Kernel::OpenDirectoryFromArchive(archive_handle, dir_name);
    if (handle) {
        cmd_buff[1] = 0;
        cmd_buff[3] = handle;
    } else {
        ERROR_LOG(KERNEL, "failed to get a handle for directory %s", dir_name.c_str());
        // TODO(Link Mauve): check for the actual error values, this one was just chosen arbitrarily.
        cmd_buff[1] = -1;
    }

    DEBUG_LOG(KERNEL, "called");
}

void OpenArchive(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    auto arch_id = static_cast<FileSys::Archive::IdCode>(cmd_buff[1]);
    LowPathType type = static_cast<LowPathType>(cmd_buff[2]);
    u32 size = cmd_buff[3];
    u32 pointer = cmd_buff[5];

    if (type != LowPathType::Empty) {
        ERROR_LOG(KERNEL, "archive LowPath type other than empty is currently unsupported");
        cmd_buff[1] = -1;
        return;
    }

    std::string archive_name = GetStringFromCmdBuff(pointer, size);

    DEBUG_LOG(KERNEL, "type=%d size=%d data=%s", type, size, archive_name.c_str());

    Handle handle = Kernel::OpenArchive(arch_id);
    if (handle) {
        cmd_buff[1] = 0;
        // cmd_buff[2] isn't used according to 3dmoo's implementation.
        cmd_buff[3] = handle;
    } else {
        ERROR_LOG(KERNEL, "failed to get a handle for archive %s", archive_name.c_str());
        // TODO(Link Mauve): check for the actual error values, this one was just chosen arbitrarily.
        cmd_buff[1] = -1;
    }

    DEBUG_LOG(KERNEL, "called");
}

/*
* FS_User::IsSdmcDetected service function
*  Outputs:
*      1 : Result of function, 0 on success, otherwise error code
*      2 : Whether the Sdmc could be detected
*/
void IsSdmcDetected(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    cmd_buff[1] = 0;
    cmd_buff[2] = Settings::values.use_virtual_sd ? 1 : 0;
    
    DEBUG_LOG(KERNEL, "called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr,               "Dummy1"},
    {0x040100C4, nullptr,               "Control"},
    {0x08010002, Initialize,            "Initialize"},
    {0x080201C2, OpenFile,              "OpenFile"},
    {0x08030204, OpenFileDirectly,      "OpenFileDirectly"},
    {0x08040142, nullptr,               "DeleteFile"},
    {0x08050244, nullptr,               "RenameFile"},
    {0x08060142, nullptr,               "DeleteDirectory"},
    {0x08070142, nullptr,               "DeleteDirectoryRecursively"},
    {0x08080202, nullptr,               "CreateFile"},
    {0x08090182, CreateDirectory,       "CreateDirectory"},
    {0x080A0244, nullptr,               "RenameDirectory"},
    {0x080B0102, OpenDirectory,         "OpenDirectory"},
    {0x080C00C2, OpenArchive,           "OpenArchive"},
    {0x080D0144, nullptr,               "ControlArchive"},
    {0x080E0080, nullptr,               "CloseArchive"},
    {0x080F0180, nullptr,               "FormatThisUserSaveData"},
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
    {0x084C0242, nullptr,               "FormatSaveData"},
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

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
