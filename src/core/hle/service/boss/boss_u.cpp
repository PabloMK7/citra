// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_u.h"

namespace Service {
namespace BOSS {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, InitializeSession, "InitializeSession"},
    {0x00020100, RegisterStorage, "RegisterStorage"},
    {0x00030000, UnregisterStorage, "UnregisterStorage"},
    {0x00040000, GetStorageInfo, "GetStorageInfo"},
    {0x00050042, RegisterPrivateRootCa, "RegisterPrivateRootCa"},
    {0x00060084, RegisterPrivateClientCert, "RegisterPrivateClientCert"},
    {0x00070000, GetNewArrivalFlag, "GetNewArrivalFlag"},
    {0x00080002, RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
    {0x00090040, SetOptoutFlag, "SetOptoutFlag"},
    {0x000A0000, GetOptoutFlag, "GetOptoutFlag"},
    {0x000B00C2, RegisterTask, "RegisterTask"},
    {0x000C0082, UnregisterTask, "UnregisterTask"},
    {0x000D0082, ReconfigureTask, "ReconfigureTask"},
    {0x000E0000, GetTaskIdList, "GetTaskIdList"},
    {0x000F0042, GetStepIdList, "GetStepIdList"},
    {0x00100102, GetNsDataIdList, "GetNsDataIdList"},
    {0x00110102, GetOwnNsDataIdList, "GetOwnNsDataIdList"},
    {0x00120102, GetNewDataNsDataIdList, "GetNewDataNsDataIdList"},
    {0x00130102, GetOwnNewDataNsDataIdList, "GetOwnNewDataNsDataIdList"},
    {0x00140082, SendProperty, "SendProperty"},
    {0x00150042, SendPropertyHandle, "SendPropertyHandle"},
    {0x00160082, ReceiveProperty, "ReceiveProperty"},
    {0x00170082, UpdateTaskInterval, "UpdateTaskInterval"},
    {0x00180082, UpdateTaskCount, "UpdateTaskCount"},
    {0x00190042, GetTaskInterval, "GetTaskInterval"},
    {0x001A0042, GetTaskCount, "GetTaskCount"},
    {0x001B0042, GetTaskServiceStatus, "GetTaskServiceStatus"},
    {0x001C0042, StartTask, "StartTask"},
    {0x001D0042, StartTaskImmediate, "StartTaskImmediate"},
    {0x001E0042, CancelTask, "CancelTask"},
    {0x001F0000, GetTaskFinishHandle, "GetTaskFinishHandle"},
    {0x00200082, GetTaskState, "GetTaskState"},
    {0x00210042, GetTaskResult, "GetTaskResult"},
    {0x00220042, GetTaskCommErrorCode, "GetTaskCommErrorCode"},
    {0x002300C2, GetTaskStatus, "GetTaskStatus"},
    {0x00240082, GetTaskError, "GetTaskError"},
    {0x00250082, GetTaskInfo, "GetTaskInfo"},
    {0x00260040, DeleteNsData, "DeleteNsData"},
    {0x002700C2, GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
    {0x00280102, ReadNsData, "ReadNsData"},
    {0x00290080, SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
    {0x002A0040, GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
    {0x002B0080, SetNsDataNewFlag, "SetNsDataNewFlag"},
    {0x002C0040, GetNsDataNewFlag, "GetNsDataNewFlag"},
    {0x002D0040, GetNsDataLastUpdate, "GetNsDataLastUpdate"},
    {0x002E0040, GetErrorCode, "GetErrorCode"},
    {0x002F0140, RegisterStorageEntry, "RegisterStorageEntry"},
    {0x00300000, GetStorageEntryInfo, "GetStorageEntryInfo"},
    {0x00310100, SetStorageOption, "SetStorageOption"},
    {0x00320000, GetStorageOption, "GetStorageOption"},
    {0x00330042, StartBgImmediate, "StartBgImmediate"},
    {0x00340042, GetTaskActivePriority, "GetTaskActivePriority"},
    {0x003500C2, RegisterImmediateTask, "RegisterImmediateTask"},
    {0x00360084, SetTaskQuery, "SetTaskQuery"},
    {0x00370084, GetTaskQuery, "GetTaskQuery"},
};

BOSS_U_Interface::BOSS_U_Interface() {
    Register(FunctionTable);
}

} // namespace BOSS
} // namespace Service
