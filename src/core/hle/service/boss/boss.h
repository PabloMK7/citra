// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace BOSS {

/**
 * BOSS::InitializeSession service function
 *  Inputs:
 *      0 : Header Code[0x00010082]
 *      1 : u32 lower 64bit value
 *      2 : u32 higher 64bit value
 *      3 : 0x20
 *      4 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void InitializeSession(Service::Interface* self);

/**
 * BOSS::RegisterStorage service function
 *  Inputs:
 *      0 : Header Code[0x00020010]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *      3 : u32 unknown3
 *      4 : u8 unknown_flag
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void RegisterStorage(Service::Interface* self);

/**
 * BOSS::UnregisterStorage service function
 *  Inputs:
 *      0 : Header Code[0x00030000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void UnregisterStorage(Service::Interface* self);

/**
 * BOSS::GetStorageInfo service function
 *  Inputs:
 *      0 : Header Code[0x00040000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 */
void GetStorageInfo(Service::Interface* self);

/**
 * BOSS::RegisterPrivateRootCa service function
 *  Inputs:
 *      0 : Header Code[0x00050042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void RegisterPrivateRootCa(Service::Interface* self);

/**
 * BOSS::RegisterPrivateClientCert service function
 *  Inputs:
 *      0 : Header Code[0x00060084]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : u32 buff_addr1
 *      5 : MappedBufferDesc2(permission = R)
 *      6 : u32 buff_addr2
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff1_size << 4 | 0xA
 *      3 : u32 buff_addr1
 *      4 : buff2_size << 4 | 0xA
 *      5 : u32 buff_addr2
 */
void RegisterPrivateClientCert(Service::Interface* self);

/**
 * BOSS::GetNewArrivalFlag service function
 *  Inputs:
 *      0 : Header Code[0x00070000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 flag
 */
void GetNewArrivalFlag(Service::Interface* self);

/**
 * BOSS::RegisterNewArrivalEvent service function
 *  Inputs:
 *      0 : Header Code[0x00080002]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void RegisterNewArrivalEvent(Service::Interface* self);

/**
 * BOSS::SetOptoutFlag service function
 *  Inputs:
 *      0 : Header Code[0x00090040]
 *      1 : u8 output_flag
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetOptoutFlag(Service::Interface* self);

/**
 * BOSS::GetOptoutFlag service function
 *  Inputs:
 *      0 : Header Code[0x000A0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 output_flag
 */
void GetOptoutFlag(Service::Interface* self);

/**
 * BOSS::RegisterTask service function
 *  Inputs:
 *      0 : Header Code[0x000B00C2]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : u8 unknown value
 *      4 : MappedBufferDesc1(permission = R)
 *      5 : buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void RegisterTask(Service::Interface* self);

/**
 * BOSS::UnregisterTask service function
 *  Inputs:
 *      0 : Header Code[0x000C0082]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void UnregisterTask(Service::Interface* self);

/**
 * BOSS::ReconfigureTask service function
 *  Inputs:
 *      0 : Header Code[0x000D0082]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void ReconfigureTask(Service::Interface* self);

/**
 * BOSS::GetTaskIdList service function
 *  Inputs:
 *      0 : Header Code[0x000E0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetTaskIdList(Service::Interface* self);

/**
 * BOSS::GetStepIdList service function
 *  Inputs:
 *      0 : Header Code[0x000F0042]
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void GetStepIdList(Service::Interface* self);

/**
 * BOSS::GetNsDataIdList service function
 *  Inputs:
 *      0 : Header Code[0x00100102]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *      3 : u32 unknown3
 *      4 : u32 unknown4
 *      5 : MappedBufferDesc(permission = W)
 *      6 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u16 unknown value
 *      3 : u16 unknown value
 *      4 : buff_size << 4 | 0xC
 *      5 : u32 buff_addr
 */
void GetNsDataIdList(Service::Interface* self);

/**
 * BOSS::GetOwnNsDataIdList service function
 *  Inputs:
 *      0 : Header Code[0x00110102]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *      3 : u32 unknown3
 *      4 : u32 unknown4
 *      5 : MappedBufferDesc(permission = W)
 *      6 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u16 unknown value
 *      3 : u16 unknown value
 *      4 : buff_size << 4 | 0xC
 *      5 : u32 buff_addr
 */
void GetOwnNsDataIdList(Service::Interface* self);

/**
 * BOSS::GetNewDataNsDataIdList service function
 *  Inputs:
 *      0 : Header Code[0x00120102]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *      3 : u32 unknown3
 *      4 : u32 unknown4
 *      5 : MappedBufferDesc(permission = W)
 *      6 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u16 unknown value
 *      3 : u16 unknown value
 *      4 : buff_size << 4 | 0xC
 *      5 : u32 buff_addr
 */
void GetNewDataNsDataIdList(Service::Interface* self);

/**
 * BOSS::GetOwnNewDataNsDataIdList service function
 *  Inputs:
 *      0 : Header Code[0x00130102]
 *      1 : u32 unknown1
 *      2 : u32 unknown2
 *      3 : u32 unknown3
 *      4 : u32 unknown4
 *      5 : MappedBufferDesc(permission = W)
 *      6 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u16 unknown value
 *      3 : u16 unknown value

 */
void GetOwnNewDataNsDataIdList(Service::Interface* self);

/**
 * BOSS::SendProperty service function
 *  Inputs:
 *      0 : Header Code[0x00140082]
 *      1 : u16 unknown value
 *      2 : u32 unknown value
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void SendProperty(Service::Interface* self);

/**
 * BOSS::SendPropertyHandle service function
 *  Inputs:
 *      0 : Header Code[0x00150042]
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void SendPropertyHandle(Service::Interface* self);

/**
 * BOSS::ReceiveProperty service function
 *  Inputs:
 *      0 : Header Code[0x00160082]
 *      1 : u16 unknown1
 *      2 : u32 buff_size
 *      3 : MappedBufferDesc(permission = W)
 *      4 : u32 buff addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : u16 unknown value
 *      4 : buff_size << 4 | 0xC
 *      5 : u32 buff_addr
 */
void ReceiveProperty(Service::Interface* self);

/**
 * BOSS::UpdateTaskInterval service function
 *  Inputs:
 *      0 : Header Code[0x00170082]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void UpdateTaskInterval(Service::Interface* self);

/**
 * BOSS::UpdateTaskCount service function
 *  Inputs:
 *      0 : Header Code[0x00180082]
 *      1 : u32 buff_size
 *      2 : u32 unknown2
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void UpdateTaskCount(Service::Interface* self);

/**
 * BOSS::GetTaskInterval service function
 *  Inputs:
 *      0 : Header Code[0x00190042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskInterval(Service::Interface* self);

/**
 * BOSS::GetTaskCount service function
 *  Inputs:
 *      0 : Header Code[0x001A0042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskCount(Service::Interface* self);

/**
 * BOSS::GetTaskServiceStatus service function
 *  Inputs:
 *      0 : Header Code[0x001B0042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskServiceStatus(Service::Interface* self);

/**
 * BOSS::StartTask service function
 *  Inputs:
 *      0 : Header Code[0x001C0042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void StartTask(Service::Interface* self);

/**
 * BOSS::StartTaskImmediate service function
 *  Inputs:
 *      0 : Header Code[0x001D0042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void StartTaskImmediate(Service::Interface* self);

/**
 * BOSS::CancelTask service function
 *  Inputs:
 *      0 : Header Code[0x001E0042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void CancelTask(Service::Interface* self);

/**
 * BOSS::GetTaskFinishHandle service function
 *  Inputs:
 *      0 : Header Code[0x001F0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 0
 *      3 : Task Finish Handle
 */
void GetTaskFinishHandle(Service::Interface* self);

/**
 * BOSS::GetTaskState service function
 *  Inputs:
 *      0 : Header Code[0x00200082]
 *      1 : u32 buff_size
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : u32 unknown value
 *      4 : u8 unknown value
 *      5 : buff_size << 4 | 0xA
 *      6 : u32 buff_addr
 */
void GetTaskState(Service::Interface* self);

/**
 * BOSS::GetTaskResult service function
 *  Inputs:
 *      0 : Header Code[0x00210042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : u32 unknown value
 *      4 : u8 unknown value
 *      5 : buff_size << 4 | 0xA
 *      6 : u32 buff_addr
 */
void GetTaskResult(Service::Interface* self);

/**
 * BOSS::GetTaskCommErrorCode service function
 *  Inputs:
 *      0 : Header Code[0x00220042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : u32 unknown value
 *      4 : u8 unknown value
 *      5 : buff_size << 4 | 0xA
 *      6 : u32 buff_addr
 */
void GetTaskCommErrorCode(Service::Interface* self);

/**
 * BOSS::GetTaskStatus service function
 *  Inputs:
 *      0 : Header Code[0x002300C2]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : u8 unknown value
 *      4 : MappedBufferDesc(permission = R)
 *      5 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskStatus(Service::Interface* self);

/**
 * BOSS::GetTaskError service function
 *  Inputs:
 *      0 : Header Code[0x00240082]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskError(Service::Interface* self);

/**
 * BOSS::GetTaskInfo service function
 *  Inputs:
 *      0 : Header Code[0x00250082]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : MappedBufferDesc(permission = R)
 *      4 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void GetTaskInfo(Service::Interface* self);

/**
 * BOSS::DeleteNsData service function
 *  Inputs:
 *      0 : Header Code[0x00260040]
 *      1 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void DeleteNsData(Service::Interface* self);

/**
 * BOSS::GetNsDataHeaderInfo service function
 *  Inputs:
 *      0 : Header Code[0x002700C2]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : u32 unknown value
 *      4 : MappedBufferDesc(permission = W)
 *      5 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xC
 *      3 : u32 buff_addr
 */
void GetNsDataHeaderInfo(Service::Interface* self);

/**
 * BOSS::ReadNsData service function
 *  Inputs:
 *      0 : Header Code[0x00280102]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *      3 : u32 unknown value
 *      4 : u32 unknown value
 *      5 : MappedBufferDesc(permission = W)
 *      6 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : u32 unknown value
 *      4 : buff_size << 4 | 0xC
 *      5 : u32 buff_addr
 */
void ReadNsData(Service::Interface* self);

/**
 * BOSS::SetNsDataAdditionalInfo service function
 *  Inputs:
 *      0 : Header Code[0x00290080]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetNsDataAdditionalInfo(Service::Interface* self);

/**
 * BOSS::GetNsDataAdditionalInfo service function
 *  Inputs:
 *      0 : Header Code[0x002A0040]
 *      1 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 */
void GetNsDataAdditionalInfo(Service::Interface* self);

/**
 * BOSS::SetNsDataNewFlag service function
 *  Inputs:
 *      0 : Header Code[0x002B0080]
 *      1 : u32 unknown value
 *      2 : u8 flag
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetNsDataNewFlag(Service::Interface* self);

/**
 * BOSS::GetNsDataNewFlag service function
 *  Inputs:
 *      0 : Header Code[0x002C0040]
 *      1 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 flag
 */
void GetNsDataNewFlag(Service::Interface* self);

/**
 * BOSS::GetNsDataLastUpdate service function
 *  Inputs:
 *      0 : Header Code[0x002D0040]
 *      1 : u32 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : u32 unknown value
 */
void GetNsDataLastUpdate(Service::Interface* self);

/**
 * BOSS::GetErrorCode service function
 *  Inputs:
 *      0 : Header Code[0x002E0040]
 *      1 : u8 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 */
void GetErrorCode(Service::Interface* self);

/**
 * BOSS::RegisterStorageEntry service function
 *  Inputs:
 *      0 : Header Code[0x002F0140]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *      3 : u32 unknown value
 *      4 : u16 unknown value
 *      5 : u8 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void RegisterStorageEntry(Service::Interface* self);

/**
 * BOSS::GetStorageEntryInfo service function
 *  Inputs:
 *      0 : Header Code[0x00300000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u32 unknown value
 *      3 : u16 unknown value
 */
void GetStorageEntryInfo(Service::Interface* self);

/**
 * BOSS::SetStorageOption service function
 *  Inputs:
 *      0 : Header Code[0x00310100]
 *      1 : u8 unknown value
 *      2 : u32 unknown value
 *      3 : u16 unknown value
 *      4 : u16 unknown value
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void SetStorageOption(Service::Interface* self);

/**
 * BOSS::GetStorageOption service function
 *  Inputs:
 *      0 : Header Code[0x00320000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : u32 unknown value
 *      4 : u16 unknown value
 *      5 : u16 unknown value
 */
void GetStorageOption(Service::Interface* self);

/**
 * BOSS::StartBgImmediate service function
 *  Inputs:
 *      0 : Header Code[0x00330042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void StartBgImmediate(Service::Interface* self);

/**
 * BOSS::GetTaskActivePriority service function
 *  Inputs:
 *      0 : Header Code[0x00340042]
 *      1 : u32 unknown value
 *      2 : MappedBufferDesc(permission = R)
 *      3 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 unknown value
 *      3 : buff_size << 4 | 0xA
 *      4 : u32 buff_addr
 */
void GetTaskActivePriority(Service::Interface* self);

/**
 * BOSS::RegisterImmediateTask service function
 *  Inputs:
 *      0 : Header Code[0x003500C2]
 *      1 : u32 unknown value
 *      2 : u8 unknown value
 *      3 : u8 unknown value
 *      4 : MappedBufferDesc(permission = R)
 *      5 : u32 buff_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff_size << 4 | 0xA
 *      3 : u32 buff_addr
 */
void RegisterImmediateTask(Service::Interface* self);

/**
 * BOSS::SetTaskQuery service function
 *  Inputs:
 *      0 : Header Code[0x00360084]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : u32 buff1_addr
 *      5 : MappedBufferDesc2(permission = R)
 *      6 : u32 buff2_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff1_size << 4 | 0xA
 *      3 : u32 buff1_addr
 *      4 : buff2_size << 4 | 0xA
 *      5 : u32 buff2_addr
 */
void SetTaskQuery(Service::Interface* self);

/**
 * BOSS::GetTaskQuery service function
 *  Inputs:
 *      0 : Header Code[0x00370084]
 *      1 : u32 unknown value
 *      2 : u32 unknown value
 *      3 : MappedBufferDesc1(permission = R)
 *      4 : u32 buff1_addr
 *      5 : MappedBufferDesc2(permission = W)
 *      6 : u32 buff2_addr
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : buff1_size << 4 | 0xA
 *      3 : u32 buff1_addr
 *      4 : buff2_size << 4 | 0xC
 *      5 : u32 buff2_addr
 */
void GetTaskQuery(Service::Interface* self);

/// Initialize BOSS service(s)
void Init();

/// Shutdown BOSS service(s)
void Shutdown();

} // namespace BOSS
} // namespace Service
