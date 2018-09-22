// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/am/am_sys.h"

namespace Service::AM {

AM_SYS::AM_SYS(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:sys", 5) {
    static const FunctionInfo functions[] = {
        {0x00010040, &AM_SYS::GetNumPrograms, "GetNumPrograms"},
        {0x00020082, &AM_SYS::GetProgramList, "GetProgramList"},
        {0x00030084, &AM_SYS::GetProgramInfos, "GetProgramInfos"},
        {0x000400C0, &AM_SYS::DeleteUserProgram, "DeleteUserProgram"},
        {0x000500C0, &AM_SYS::GetProductCode, "GetProductCode"},
        {0x000600C0, nullptr, "GetStorageId"},
        {0x00070080, &AM_SYS::DeleteTicket, "DeleteTicket"},
        {0x00080000, &AM_SYS::GetNumTickets, "GetNumTickets"},
        {0x00090082, &AM_SYS::GetTicketList, "GetTicketList"},
        {0x000A0000, nullptr, "GetDeviceID"},
        {0x000B0040, nullptr, "GetNumImportTitleContexts"},
        {0x000C0082, nullptr, "GetImportTitleContextList"},
        {0x000D0084, nullptr, "GetImportTitleContexts"},
        {0x000E00C0, nullptr, "DeleteImportTitleContext"},
        {0x000F00C0, nullptr, "GetNumImportContentContexts"},
        {0x00100102, nullptr, "GetImportContentContextList"},
        {0x00110104, nullptr, "GetImportContentContexts"},
        {0x00120102, nullptr, "DeleteImportContentContexts"},
        {0x00130040, nullptr, "NeedsCleanup"},
        {0x00140040, nullptr, "DoCleanup"},
        {0x00150040, nullptr, "DeleteAllImportContexts"},
        {0x00160000, nullptr, "DeleteAllTemporaryPrograms"},
        {0x00170044, nullptr, "ImportTwlBackupLegacy"},
        {0x00180080, nullptr, "InitializeTitleDatabase"},
        {0x00190040, &AM_SYS::QueryAvailableTitleDatabase, "QueryAvailableTitleDatabase"},
        {0x001A00C0, nullptr, "CalcTwlBackupSize"},
        {0x001B0144, nullptr, "ExportTwlBackup"},
        {0x001C0084, nullptr, "ImportTwlBackup"},
        {0x001D0000, nullptr, "DeleteAllTwlUserPrograms"},
        {0x001E00C8, nullptr, "ReadTwlBackupInfo"},
        {0x001F0040, nullptr, "DeleteAllExpiredUserPrograms"},
        {0x00200000, nullptr, "GetTwlArchiveResourceInfo"},
        {0x00210042, nullptr, "GetPersonalizedTicketInfoList"},
        {0x00220080, nullptr, "DeleteAllImportContextsFiltered"},
        {0x00230080, nullptr, "GetNumImportTitleContextsFiltered"},
        {0x002400C2, nullptr, "GetImportTitleContextListFiltered"},
        {0x002500C0, &AM_SYS::CheckContentRights, "CheckContentRights"},
        {0x00260044, nullptr, "GetTicketLimitInfos"},
        {0x00270044, nullptr, "GetDemoLaunchInfos"},
        {0x00280108, nullptr, "ReadTwlBackupInfoEx"},
        {0x00290082, nullptr, "DeleteUserProgramsAtomically"},
        {0x002A00C0, nullptr, "GetNumExistingContentInfosSystem"},
        {0x002B0142, nullptr, "ListExistingContentInfosSystem"},
        {0x002C0084, nullptr, "GetProgramInfosIgnorePlatform"},
        {0x002D00C0, &AM_SYS::CheckContentRightsIgnorePlatform, "CheckContentRightsIgnorePlatform"},
        {0x100100C0, &AM_SYS::GetDLCContentInfoCount, "GetDLCContentInfoCount"},
        {0x10020104, &AM_SYS::FindDLCContentInfos, "FindDLCContentInfos"},
        {0x10030142, &AM_SYS::ListDLCContentInfos, "ListDLCContentInfos"},
        {0x10040102, &AM_SYS::DeleteContents, "DeleteContents"},
        {0x10050084, &AM_SYS::GetDLCTitleInfos, "GetDLCTitleInfos"},
        {0x10060080, nullptr, "GetNumDataTitleTickets"},
        {0x10070102, &AM_SYS::ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
        {0x100801C2, nullptr, "GetItemRights"},
        {0x100900C0, nullptr, "IsDataTitleInUse"},
        {0x100A0000, nullptr, "IsExternalTitleDatabaseInitialized"},
        {0x100B00C0, nullptr, "GetNumExistingContentInfos"},
        {0x100C0142, nullptr, "ListExistingContentInfos"},
        {0x100D0084, &AM_SYS::GetPatchTitleInfos, "GetPatchTitleInfos"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM
