// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/boss/boss_u.h"

namespace Service::BOSS {

BOSS_U::BOSS_U(std::shared_ptr<Module> boss)
    : Module::Interface(std::move(boss), "boss:U", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 2, 2), &BOSS_U::InitializeSession, "InitializeSession"},
        {IPC::MakeHeader(0x0002, 4, 0), &BOSS_U::SetStorageInfo, "SetStorageInfo"},
        {IPC::MakeHeader(0x0003, 0, 0), &BOSS_U::UnregisterStorage, "UnregisterStorage"},
        {IPC::MakeHeader(0x0004, 0, 0), &BOSS_U::GetStorageInfo, "GetStorageInfo"},
        {IPC::MakeHeader(0x0005, 1, 2), &BOSS_U::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {IPC::MakeHeader(0x0006, 2, 4), &BOSS_U::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {IPC::MakeHeader(0x0007, 0, 0), &BOSS_U::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {IPC::MakeHeader(0x0008, 0, 2), &BOSS_U::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {IPC::MakeHeader(0x0009, 1, 0), &BOSS_U::SetOptoutFlag, "SetOptoutFlag"},
        {IPC::MakeHeader(0x000A, 0, 0), &BOSS_U::GetOptoutFlag, "GetOptoutFlag"},
        {IPC::MakeHeader(0x000B, 3, 2), &BOSS_U::RegisterTask, "RegisterTask"},
        {IPC::MakeHeader(0x000C, 2, 2), &BOSS_U::UnregisterTask, "UnregisterTask"},
        {IPC::MakeHeader(0x000D, 2, 2), &BOSS_U::ReconfigureTask, "ReconfigureTask"},
        {IPC::MakeHeader(0x000E, 0, 0), &BOSS_U::GetTaskIdList, "GetTaskIdList"},
        {IPC::MakeHeader(0x000F, 1, 2), &BOSS_U::GetStepIdList, "GetStepIdList"},
        {IPC::MakeHeader(0x0010, 4, 2), &BOSS_U::GetNsDataIdList, "GetNsDataIdList"},
        {IPC::MakeHeader(0x0011, 4, 2), &BOSS_U::GetNsDataIdList1, "GetNsDataIdList1"},
        {IPC::MakeHeader(0x0012, 4, 2), &BOSS_U::GetNsDataIdList2, "GetNsDataIdList2"},
        {IPC::MakeHeader(0x0013, 4, 2), &BOSS_U::GetNsDataIdList3, "GetNsDataIdList3"},
        {IPC::MakeHeader(0x0014, 2, 2), &BOSS_U::SendProperty, "SendProperty"},
        {IPC::MakeHeader(0x0015, 1, 2), &BOSS_U::SendPropertyHandle, "SendPropertyHandle"},
        {IPC::MakeHeader(0x0016, 2, 2), &BOSS_U::ReceiveProperty, "ReceiveProperty"},
        {IPC::MakeHeader(0x0017, 2, 2), &BOSS_U::UpdateTaskInterval, "UpdateTaskInterval"},
        {IPC::MakeHeader(0x0018, 2, 2), &BOSS_U::UpdateTaskCount, "UpdateTaskCount"},
        {IPC::MakeHeader(0x0019, 1, 2), &BOSS_U::GetTaskInterval, "GetTaskInterval"},
        {IPC::MakeHeader(0x001A, 1, 2), &BOSS_U::GetTaskCount, "GetTaskCount"},
        {IPC::MakeHeader(0x001B, 1, 2), &BOSS_U::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {IPC::MakeHeader(0x001C, 1, 2), &BOSS_U::StartTask, "StartTask"},
        {IPC::MakeHeader(0x001D, 1, 2), &BOSS_U::StartTaskImmediate, "StartTaskImmediate"},
        {IPC::MakeHeader(0x001E, 1, 2), &BOSS_U::CancelTask, "CancelTask"},
        {IPC::MakeHeader(0x001F, 0, 0), &BOSS_U::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {IPC::MakeHeader(0x0020, 2, 2), &BOSS_U::GetTaskState, "GetTaskState"},
        {IPC::MakeHeader(0x0021, 1, 2), &BOSS_U::GetTaskResult, "GetTaskResult"},
        {IPC::MakeHeader(0x0022, 1, 2), &BOSS_U::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {IPC::MakeHeader(0x0023, 3, 2), &BOSS_U::GetTaskStatus, "GetTaskStatus"},
        {IPC::MakeHeader(0x0024, 2, 2), &BOSS_U::GetTaskError, "GetTaskError"},
        {IPC::MakeHeader(0x0025, 2, 2), &BOSS_U::GetTaskInfo, "GetTaskInfo"},
        {IPC::MakeHeader(0x0026, 1, 0), &BOSS_U::DeleteNsData, "DeleteNsData"},
        {IPC::MakeHeader(0x0027, 3, 2), &BOSS_U::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {IPC::MakeHeader(0x0028, 4, 2), &BOSS_U::ReadNsData, "ReadNsData"},
        {IPC::MakeHeader(0x0029, 2, 0), &BOSS_U::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {IPC::MakeHeader(0x002A, 1, 0), &BOSS_U::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {IPC::MakeHeader(0x002B, 2, 0), &BOSS_U::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {IPC::MakeHeader(0x002C, 1, 0), &BOSS_U::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {IPC::MakeHeader(0x002D, 1, 0), &BOSS_U::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {IPC::MakeHeader(0x002E, 1, 0), &BOSS_U::GetErrorCode, "GetErrorCode"},
        {IPC::MakeHeader(0x002F, 5, 0), &BOSS_U::RegisterStorageEntry, "RegisterStorageEntry"},
        {IPC::MakeHeader(0x0030, 0, 0), &BOSS_U::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {IPC::MakeHeader(0x0031, 4, 0), &BOSS_U::SetStorageOption, "SetStorageOption"},
        {IPC::MakeHeader(0x0032, 0, 0), &BOSS_U::GetStorageOption, "GetStorageOption"},
        {IPC::MakeHeader(0x0033, 1, 2), &BOSS_U::StartBgImmediate, "StartBgImmediate"},
        {IPC::MakeHeader(0x0034, 1, 2), &BOSS_U::GetTaskProperty0, "GetTaskProperty0"},
        {IPC::MakeHeader(0x0035, 3, 2), &BOSS_U::RegisterImmediateTask, "RegisterImmediateTask"},
        {IPC::MakeHeader(0x0036, 2, 4), &BOSS_U::SetTaskQuery, "SetTaskQuery"},
        {IPC::MakeHeader(0x0037, 2, 4), &BOSS_U::GetTaskQuery, "GetTaskQuery"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS

SERIALIZE_EXPORT_IMPL(Service::BOSS::BOSS_U)
