// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/boss/boss_p.h"

namespace Service::BOSS {

BOSS_P::BOSS_P(std::shared_ptr<Module> boss)
    : Module::Interface(std::move(boss), "boss:P", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // boss:u shared commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), &BOSS_P::InitializeSession, "InitializeSession"},
        {IPC::MakeHeader(0x0002, 4, 0), &BOSS_P::SetStorageInfo, "RegisterStorage"},
        {IPC::MakeHeader(0x0003, 0, 0), &BOSS_P::UnregisterStorage, "UnregisterStorage"},
        {IPC::MakeHeader(0x0004, 0, 0), &BOSS_P::GetStorageInfo, "GetStorageInfo"},
        {IPC::MakeHeader(0x0005, 1, 2), &BOSS_P::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {IPC::MakeHeader(0x0006, 2, 4), &BOSS_P::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {IPC::MakeHeader(0x0007, 0, 0), &BOSS_P::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {IPC::MakeHeader(0x0008, 0, 2), &BOSS_P::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {IPC::MakeHeader(0x0009, 1, 0), &BOSS_P::SetOptoutFlag, "SetOptoutFlag"},
        {IPC::MakeHeader(0x000A, 0, 0), &BOSS_P::GetOptoutFlag, "GetOptoutFlag"},
        {IPC::MakeHeader(0x000B, 3, 2), &BOSS_P::RegisterTask, "RegisterTask"},
        {IPC::MakeHeader(0x000C, 2, 2), &BOSS_P::UnregisterTask, "UnregisterTask"},
        {IPC::MakeHeader(0x000D, 2, 2), &BOSS_P::ReconfigureTask, "ReconfigureTask"},
        {IPC::MakeHeader(0x000E, 0, 0), &BOSS_P::GetTaskIdList, "GetTaskIdList"},
        {IPC::MakeHeader(0x000F, 1, 2), &BOSS_P::GetStepIdList, "GetStepIdList"},
        {IPC::MakeHeader(0x0010, 4, 2), &BOSS_P::GetNsDataIdList, "GetNsDataIdList"},
        {IPC::MakeHeader(0x0011, 4, 2), &BOSS_P::GetNsDataIdList1, "GetNsDataIdList1"},
        {IPC::MakeHeader(0x0012, 4, 2), &BOSS_P::GetNsDataIdList2, "GetNsDataIdList2"},
        {IPC::MakeHeader(0x0013, 4, 2), &BOSS_P::GetNsDataIdList3, "GetNsDataIdList3"},
        {IPC::MakeHeader(0x0014, 2, 2), &BOSS_P::SendProperty, "SendProperty"},
        {IPC::MakeHeader(0x0015, 1, 2), &BOSS_P::SendPropertyHandle, "SendPropertyHandle"},
        {IPC::MakeHeader(0x0016, 2, 2), &BOSS_P::ReceiveProperty, "ReceiveProperty"},
        {IPC::MakeHeader(0x0017, 2, 2), &BOSS_P::UpdateTaskInterval, "UpdateTaskInterval"},
        {IPC::MakeHeader(0x0018, 2, 2), &BOSS_P::UpdateTaskCount, "UpdateTaskCount"},
        {IPC::MakeHeader(0x0019, 1, 2), &BOSS_P::GetTaskInterval, "GetTaskInterval"},
        {IPC::MakeHeader(0x001A, 1, 2), &BOSS_P::GetTaskCount, "GetTaskCount"},
        {IPC::MakeHeader(0x001B, 1, 2), &BOSS_P::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {IPC::MakeHeader(0x001C, 1, 2), &BOSS_P::StartTask, "StartTask"},
        {IPC::MakeHeader(0x001D, 1, 2), &BOSS_P::StartTaskImmediate, "StartTaskImmediate"},
        {IPC::MakeHeader(0x001E, 1, 2), &BOSS_P::CancelTask, "CancelTask"},
        {IPC::MakeHeader(0x001F, 0, 0), &BOSS_P::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {IPC::MakeHeader(0x0020, 2, 2), &BOSS_P::GetTaskState, "GetTaskState"},
        {IPC::MakeHeader(0x0021, 1, 2), &BOSS_P::GetTaskResult, "GetTaskResult"},
        {IPC::MakeHeader(0x0022, 1, 2), &BOSS_P::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {IPC::MakeHeader(0x0023, 3, 2), &BOSS_P::GetTaskStatus, "GetTaskStatus"},
        {IPC::MakeHeader(0x0024, 2, 2), &BOSS_P::GetTaskError, "GetTaskError"},
        {IPC::MakeHeader(0x0025, 2, 2), &BOSS_P::GetTaskInfo, "GetTaskInfo"},
        {IPC::MakeHeader(0x0026, 1, 0), &BOSS_P::DeleteNsData, "DeleteNsData"},
        {IPC::MakeHeader(0x0027, 3, 2), &BOSS_P::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {IPC::MakeHeader(0x0028, 4, 2), &BOSS_P::ReadNsData, "ReadNsData"},
        {IPC::MakeHeader(0x0029, 2, 0), &BOSS_P::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {IPC::MakeHeader(0x002A, 1, 0), &BOSS_P::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {IPC::MakeHeader(0x002B, 2, 0), &BOSS_P::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {IPC::MakeHeader(0x002C, 1, 0), &BOSS_P::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {IPC::MakeHeader(0x002D, 1, 0), &BOSS_P::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {IPC::MakeHeader(0x002E, 1, 0), &BOSS_P::GetErrorCode, "GetErrorCode"},
        {IPC::MakeHeader(0x002F, 5, 0), &BOSS_P::RegisterStorageEntry, "RegisterStorageEntry"},
        {IPC::MakeHeader(0x0030, 0, 0), &BOSS_P::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {IPC::MakeHeader(0x0031, 4, 0), &BOSS_P::SetStorageOption, "SetStorageOption"},
        {IPC::MakeHeader(0x0032, 0, 0), &BOSS_P::GetStorageOption, "GetStorageOption"},
        {IPC::MakeHeader(0x0033, 1, 2), &BOSS_P::StartBgImmediate, "StartBgImmediate"},
        {IPC::MakeHeader(0x0034, 1, 2), &BOSS_P::GetTaskProperty0, "GetTaskProperty0"},
        {IPC::MakeHeader(0x0035, 3, 2), &BOSS_P::RegisterImmediateTask, "RegisterImmediateTask"},
        {IPC::MakeHeader(0x0036, 2, 4), &BOSS_P::SetTaskQuery, "SetTaskQuery"},
        {IPC::MakeHeader(0x0037, 2, 4), &BOSS_P::GetTaskQuery, "GetTaskQuery"},
        // boss:p
        {IPC::MakeHeader(0x0401, 2, 2), &BOSS_P::InitializeSessionPrivileged, "InitializeSessionPrivileged"},
        {IPC::MakeHeader(0x0404, 2, 0), &BOSS_P::GetAppNewFlag, "GetAppNewFlag"},
        {IPC::MakeHeader(0x040D, 6, 2), &BOSS_P::GetNsDataIdListPrivileged, "GetNsDataIdListPrivileged"},
        {IPC::MakeHeader(0x040E, 6, 2), &BOSS_P::GetNsDataIdListPrivileged1, "GetNsDataIdListPrivileged1"},
        {IPC::MakeHeader(0x0413, 2, 2), &BOSS_P::SendPropertyPrivileged, "SendPropertyPrivileged"},
        {IPC::MakeHeader(0x0415, 3, 0), &BOSS_P::DeleteNsDataPrivileged, "DeleteNsDataPrivileged"},
        {IPC::MakeHeader(0x0416, 5, 2), &BOSS_P::GetNsDataHeaderInfoPrivileged, "GetNsDataHeaderInfoPrivileged"},
        {IPC::MakeHeader(0x0417, 6, 2), &BOSS_P::ReadNsDataPrivileged, "ReadNsDataPrivileged"},
        {IPC::MakeHeader(0x041A, 4, 0), &BOSS_P::SetNsDataNewFlagPrivileged, "SetNsDataNewFlagPrivileged"},
        {IPC::MakeHeader(0x041B, 3, 0), &BOSS_P::GetNsDataNewFlagPrivileged, "GetNsDataNewFlagPrivileged"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS

SERIALIZE_EXPORT_IMPL(Service::BOSS::BOSS_P)
