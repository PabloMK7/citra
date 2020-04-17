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
        {0x00010082, &BOSS_U::InitializeSession, "InitializeSession"},
        {0x00020100, &BOSS_U::SetStorageInfo, "SetStorageInfo"},
        {0x00030000, &BOSS_U::UnregisterStorage, "UnregisterStorage"},
        {0x00040000, &BOSS_U::GetStorageInfo, "GetStorageInfo"},
        {0x00050042, &BOSS_U::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {0x00060084, &BOSS_U::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {0x00070000, &BOSS_U::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {0x00080002, &BOSS_U::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {0x00090040, &BOSS_U::SetOptoutFlag, "SetOptoutFlag"},
        {0x000A0000, &BOSS_U::GetOptoutFlag, "GetOptoutFlag"},
        {0x000B00C2, &BOSS_U::RegisterTask, "RegisterTask"},
        {0x000C0082, &BOSS_U::UnregisterTask, "UnregisterTask"},
        {0x000D0082, &BOSS_U::ReconfigureTask, "ReconfigureTask"},
        {0x000E0000, &BOSS_U::GetTaskIdList, "GetTaskIdList"},
        {0x000F0042, &BOSS_U::GetStepIdList, "GetStepIdList"},
        {0x00100102, &BOSS_U::GetNsDataIdList, "GetNsDataIdList"},
        {0x00110102, &BOSS_U::GetNsDataIdList1, "GetNsDataIdList1"},
        {0x00120102, &BOSS_U::GetNsDataIdList2, "GetNsDataIdList2"},
        {0x00130102, &BOSS_U::GetNsDataIdList3, "GetNsDataIdList3"},
        {0x00140082, &BOSS_U::SendProperty, "SendProperty"},
        {0x00150042, &BOSS_U::SendPropertyHandle, "SendPropertyHandle"},
        {0x00160082, &BOSS_U::ReceiveProperty, "ReceiveProperty"},
        {0x00170082, &BOSS_U::UpdateTaskInterval, "UpdateTaskInterval"},
        {0x00180082, &BOSS_U::UpdateTaskCount, "UpdateTaskCount"},
        {0x00190042, &BOSS_U::GetTaskInterval, "GetTaskInterval"},
        {0x001A0042, &BOSS_U::GetTaskCount, "GetTaskCount"},
        {0x001B0042, &BOSS_U::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {0x001C0042, &BOSS_U::StartTask, "StartTask"},
        {0x001D0042, &BOSS_U::StartTaskImmediate, "StartTaskImmediate"},
        {0x001E0042, &BOSS_U::CancelTask, "CancelTask"},
        {0x001F0000, &BOSS_U::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {0x00200082, &BOSS_U::GetTaskState, "GetTaskState"},
        {0x00210042, &BOSS_U::GetTaskResult, "GetTaskResult"},
        {0x00220042, &BOSS_U::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {0x002300C2, &BOSS_U::GetTaskStatus, "GetTaskStatus"},
        {0x00240082, &BOSS_U::GetTaskError, "GetTaskError"},
        {0x00250082, &BOSS_U::GetTaskInfo, "GetTaskInfo"},
        {0x00260040, &BOSS_U::DeleteNsData, "DeleteNsData"},
        {0x002700C2, &BOSS_U::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {0x00280102, &BOSS_U::ReadNsData, "ReadNsData"},
        {0x00290080, &BOSS_U::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {0x002A0040, &BOSS_U::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {0x002B0080, &BOSS_U::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {0x002C0040, &BOSS_U::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {0x002D0040, &BOSS_U::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {0x002E0040, &BOSS_U::GetErrorCode, "GetErrorCode"},
        {0x002F0140, &BOSS_U::RegisterStorageEntry, "RegisterStorageEntry"},
        {0x00300000, &BOSS_U::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {0x00310100, &BOSS_U::SetStorageOption, "SetStorageOption"},
        {0x00320000, &BOSS_U::GetStorageOption, "GetStorageOption"},
        {0x00330042, &BOSS_U::StartBgImmediate, "StartBgImmediate"},
        {0x00340042, &BOSS_U::GetTaskProperty0, "GetTaskProperty0"},
        {0x003500C2, &BOSS_U::RegisterImmediateTask, "RegisterImmediateTask"},
        {0x00360084, &BOSS_U::SetTaskQuery, "SetTaskQuery"},
        {0x00370084, &BOSS_U::GetTaskQuery, "GetTaskQuery"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS

SERIALIZE_EXPORT_IMPL(Service::BOSS::BOSS_U)
