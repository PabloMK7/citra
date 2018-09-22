// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/boss/boss_p.h"

namespace Service::BOSS {

BOSS_P::BOSS_P(std::shared_ptr<Module> boss)
    : Module::Interface(std::move(boss), "boss:P", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // boss:u shared commands
        // clang-format off
        {0x00010082, &BOSS_P::InitializeSession, "InitializeSession"},
        {0x00020100, &BOSS_P::SetStorageInfo, "RegisterStorage"},
        {0x00030000, &BOSS_P::UnregisterStorage, "UnregisterStorage"},
        {0x00040000, &BOSS_P::GetStorageInfo, "GetStorageInfo"},
        {0x00050042, &BOSS_P::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {0x00060084, &BOSS_P::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {0x00070000, &BOSS_P::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {0x00080002, &BOSS_P::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {0x00090040, &BOSS_P::SetOptoutFlag, "SetOptoutFlag"},
        {0x000A0000, &BOSS_P::GetOptoutFlag, "GetOptoutFlag"},
        {0x000B00C2, &BOSS_P::RegisterTask, "RegisterTask"},
        {0x000C0082, &BOSS_P::UnregisterTask, "UnregisterTask"},
        {0x000D0082, &BOSS_P::ReconfigureTask, "ReconfigureTask"},
        {0x000E0000, &BOSS_P::GetTaskIdList, "GetTaskIdList"},
        {0x000F0042, &BOSS_P::GetStepIdList, "GetStepIdList"},
        {0x00100102, &BOSS_P::GetNsDataIdList, "GetNsDataIdList"},
        {0x00110102, &BOSS_P::GetNsDataIdList1, "GetNsDataIdList1"},
        {0x00120102, &BOSS_P::GetNsDataIdList2, "GetNsDataIdList2"},
        {0x00130102, &BOSS_P::GetNsDataIdList3, "GetNsDataIdList3"},
        {0x00140082, &BOSS_P::SendProperty, "SendProperty"},
        {0x00150042, &BOSS_P::SendPropertyHandle, "SendPropertyHandle"},
        {0x00160082, &BOSS_P::ReceiveProperty, "ReceiveProperty"},
        {0x00170082, &BOSS_P::UpdateTaskInterval, "UpdateTaskInterval"},
        {0x00180082, &BOSS_P::UpdateTaskCount, "UpdateTaskCount"},
        {0x00190042, &BOSS_P::GetTaskInterval, "GetTaskInterval"},
        {0x001A0042, &BOSS_P::GetTaskCount, "GetTaskCount"},
        {0x001B0042, &BOSS_P::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {0x001C0042, &BOSS_P::StartTask, "StartTask"},
        {0x001D0042, &BOSS_P::StartTaskImmediate, "StartTaskImmediate"},
        {0x001E0042, &BOSS_P::CancelTask, "CancelTask"},
        {0x001F0000, &BOSS_P::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {0x00200082, &BOSS_P::GetTaskState, "GetTaskState"},
        {0x00210042, &BOSS_P::GetTaskResult, "GetTaskResult"},
        {0x00220042, &BOSS_P::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {0x002300C2, &BOSS_P::GetTaskStatus, "GetTaskStatus"},
        {0x00240082, &BOSS_P::GetTaskError, "GetTaskError"},
        {0x00250082, &BOSS_P::GetTaskInfo, "GetTaskInfo"},
        {0x00260040, &BOSS_P::DeleteNsData, "DeleteNsData"},
        {0x002700C2, &BOSS_P::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {0x00280102, &BOSS_P::ReadNsData, "ReadNsData"},
        {0x00290080, &BOSS_P::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {0x002A0040, &BOSS_P::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {0x002B0080, &BOSS_P::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {0x002C0040, &BOSS_P::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {0x002D0040, &BOSS_P::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {0x002E0040, &BOSS_P::GetErrorCode, "GetErrorCode"},
        {0x002F0140, &BOSS_P::RegisterStorageEntry, "RegisterStorageEntry"},
        {0x00300000, &BOSS_P::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {0x00310100, &BOSS_P::SetStorageOption, "SetStorageOption"},
        {0x00320000, &BOSS_P::GetStorageOption, "GetStorageOption"},
        {0x00330042, &BOSS_P::StartBgImmediate, "StartBgImmediate"},
        {0x00340042, &BOSS_P::GetTaskProperty0, "GetTaskProperty0"},
        {0x003500C2, &BOSS_P::RegisterImmediateTask, "RegisterImmediateTask"},
        {0x00360084, &BOSS_P::SetTaskQuery, "SetTaskQuery"},
        {0x00370084, &BOSS_P::GetTaskQuery, "GetTaskQuery"},
        // boss:p
        {0x04010082, &BOSS_P::InitializeSessionPrivileged, "InitializeSessionPrivileged"},
        {0x04040080, &BOSS_P::GetAppNewFlag, "GetAppNewFlag"},
        {0x040D0182, &BOSS_P::GetNsDataIdListPrivileged, "GetNsDataIdListPrivileged"},
        {0x040E0182, &BOSS_P::GetNsDataIdListPrivileged1, "GetNsDataIdListPrivileged1"},
        {0x04130082, &BOSS_P::SendPropertyPrivileged, "SendPropertyPrivileged"},
        {0x041500C0, &BOSS_P::DeleteNsDataPrivileged, "DeleteNsDataPrivileged"},
        {0x04160142, &BOSS_P::GetNsDataHeaderInfoPrivileged, "GetNsDataHeaderInfoPrivileged"},
        {0x04170182, &BOSS_P::ReadNsDataPrivileged, "ReadNsDataPrivileged"},
        {0x041A0100, &BOSS_P::SetNsDataNewFlagPrivileged, "SetNsDataNewFlagPrivileged"},
        {0x041B00C0, &BOSS_P::GetNsDataNewFlagPrivileged, "GetNsDataNewFlagPrivileged"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS
