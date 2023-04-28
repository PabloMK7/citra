// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/am/am_sys.h"

namespace Service::AM {

AM_SYS::AM_SYS(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:sys", 5) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 1, 0), &AM_SYS::GetNumPrograms, "GetNumPrograms"},
        {IPC::MakeHeader(0x0002, 2, 2), &AM_SYS::GetProgramList, "GetProgramList"},
        {IPC::MakeHeader(0x0003, 2, 4), &AM_SYS::GetProgramInfos, "GetProgramInfos"},
        {IPC::MakeHeader(0x0004, 3, 0), &AM_SYS::DeleteUserProgram, "DeleteUserProgram"},
        {IPC::MakeHeader(0x0005, 3, 0), &AM_SYS::GetProductCode, "GetProductCode"},
        {IPC::MakeHeader(0x0006, 3, 0), nullptr, "GetStorageId"},
        {IPC::MakeHeader(0x0007, 2, 0), &AM_SYS::DeleteTicket, "DeleteTicket"},
        {IPC::MakeHeader(0x0008, 0, 0), &AM_SYS::GetNumTickets, "GetNumTickets"},
        {IPC::MakeHeader(0x0009, 2, 2), &AM_SYS::GetTicketList, "GetTicketList"},
        {IPC::MakeHeader(0x000A, 0, 0), nullptr, "GetDeviceID"},
        {IPC::MakeHeader(0x000B, 1, 0), nullptr, "GetNumImportTitleContexts"},
        {IPC::MakeHeader(0x000C, 2, 2), nullptr, "GetImportTitleContextList"},
        {IPC::MakeHeader(0x000D, 2, 4), nullptr, "GetImportTitleContexts"},
        {IPC::MakeHeader(0x000E, 3, 0), nullptr, "DeleteImportTitleContext"},
        {IPC::MakeHeader(0x000F, 3, 0), nullptr, "GetNumImportContentContexts"},
        {IPC::MakeHeader(0x0010, 4, 2), nullptr, "GetImportContentContextList"},
        {IPC::MakeHeader(0x0011, 4, 4), nullptr, "GetImportContentContexts"},
        {IPC::MakeHeader(0x0012, 4, 2), nullptr, "DeleteImportContentContexts"},
        {IPC::MakeHeader(0x0013, 1, 0), nullptr, "NeedsCleanup"},
        {IPC::MakeHeader(0x0014, 1, 0), nullptr, "DoCleanup"},
        {IPC::MakeHeader(0x0015, 1, 0), nullptr, "DeleteAllImportContexts"},
        {IPC::MakeHeader(0x0016, 0, 0), nullptr, "DeleteAllTemporaryPrograms"},
        {IPC::MakeHeader(0x0017, 1, 4), nullptr, "ImportTwlBackupLegacy"},
        {IPC::MakeHeader(0x0018, 2, 0), nullptr, "InitializeTitleDatabase"},
        {IPC::MakeHeader(0x0019, 1, 0), &AM_SYS::QueryAvailableTitleDatabase, "QueryAvailableTitleDatabase"},
        {IPC::MakeHeader(0x001A, 3, 0), nullptr, "CalcTwlBackupSize"},
        {IPC::MakeHeader(0x001B, 5, 4), nullptr, "ExportTwlBackup"},
        {IPC::MakeHeader(0x001C, 2, 4), nullptr, "ImportTwlBackup"},
        {IPC::MakeHeader(0x001D, 0, 0), nullptr, "DeleteAllTwlUserPrograms"},
        {IPC::MakeHeader(0x001E, 3, 8), nullptr, "ReadTwlBackupInfo"},
        {IPC::MakeHeader(0x001F, 1, 0), nullptr, "DeleteAllExpiredUserPrograms"},
        {IPC::MakeHeader(0x0020, 0, 0), nullptr, "GetTwlArchiveResourceInfo"},
        {IPC::MakeHeader(0x0021, 1, 2), nullptr, "GetPersonalizedTicketInfoList"},
        {IPC::MakeHeader(0x0022, 2, 0), nullptr, "DeleteAllImportContextsFiltered"},
        {IPC::MakeHeader(0x0023, 2, 0), nullptr, "GetNumImportTitleContextsFiltered"},
        {IPC::MakeHeader(0x0024, 3, 2), nullptr, "GetImportTitleContextListFiltered"},
        {IPC::MakeHeader(0x0025, 3, 0), &AM_SYS::CheckContentRights, "CheckContentRights"},
        {IPC::MakeHeader(0x0026, 1, 4), nullptr, "GetTicketLimitInfos"},
        {IPC::MakeHeader(0x0027, 1, 4), nullptr, "GetDemoLaunchInfos"},
        {IPC::MakeHeader(0x0028, 4, 8), nullptr, "ReadTwlBackupInfoEx"},
        {IPC::MakeHeader(0x0029, 2, 2), nullptr, "DeleteUserProgramsAtomically"},
        {IPC::MakeHeader(0x002A, 3, 0), nullptr, "GetNumExistingContentInfosSystem"},
        {IPC::MakeHeader(0x002B, 5, 2), nullptr, "ListExistingContentInfosSystem"},
        {IPC::MakeHeader(0x002C, 2, 4), nullptr, "GetProgramInfosIgnorePlatform"},
        {IPC::MakeHeader(0x002D, 3, 0), &AM_SYS::CheckContentRightsIgnorePlatform, "CheckContentRightsIgnorePlatform"},
        {IPC::MakeHeader(0x1001, 3, 0), &AM_SYS::GetDLCContentInfoCount, "GetDLCContentInfoCount"},
        {IPC::MakeHeader(0x1002, 4, 4), &AM_SYS::FindDLCContentInfos, "FindDLCContentInfos"},
        {IPC::MakeHeader(0x1003, 5, 2), &AM_SYS::ListDLCContentInfos, "ListDLCContentInfos"},
        {IPC::MakeHeader(0x1004, 4, 2), &AM_SYS::DeleteContents, "DeleteContents"},
        {IPC::MakeHeader(0x1005, 2, 4), &AM_SYS::GetDLCTitleInfos, "GetDLCTitleInfos"},
        {IPC::MakeHeader(0x1006, 2, 0), nullptr, "GetNumDataTitleTickets"},
        {IPC::MakeHeader(0x1007, 4, 2), &AM_SYS::ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
        {IPC::MakeHeader(0x1008, 7, 2), nullptr, "GetItemRights"},
        {IPC::MakeHeader(0x1009, 3, 0), nullptr, "IsDataTitleInUse"},
        {IPC::MakeHeader(0x100A, 0, 0), nullptr, "IsExternalTitleDatabaseInitialized"},
        {IPC::MakeHeader(0x100B, 3, 0), nullptr, "GetNumExistingContentInfos"},
        {IPC::MakeHeader(0x100C, 5, 2), nullptr, "ListExistingContentInfos"},
        {IPC::MakeHeader(0x100D, 2, 4), &AM_SYS::GetPatchTitleInfos, "GetPatchTitleInfos"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM

SERIALIZE_EXPORT_IMPL(Service::AM::AM_SYS)
