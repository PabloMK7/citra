// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_user.h"

namespace Service {
namespace IR {

static Kernel::SharedPtr<Kernel::Event> conn_status_event;
static Kernel::SharedPtr<Kernel::SharedMemory> transfer_shared_memory;

/**
 * IR::InitializeIrNopShared service function
 *  Inputs:
 *      1 : Size of transfer buffer
 *      2 : Recv buffer size
 *      3 : unknown
 *      4 : Send buffer size
 *      5 : unknown
 *      6 : BaudRate (u8)
 *      7 : 0
 *      8 : Handle of transfer shared memory
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void InitializeIrNopShared(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 transfer_buff_size = cmd_buff[1];
    u32 recv_buff_size = cmd_buff[2];
    u32 unk1 = cmd_buff[3];
    u32 send_buff_size = cmd_buff[4];
    u32 unk2 = cmd_buff[5];
    u8 baud_rate = cmd_buff[6] & 0xFF;
    Kernel::Handle handle = cmd_buff[8];

    if (Kernel::g_handle_table.IsValid(handle)) {
        transfer_shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(handle);
        transfer_shared_memory->name = "IR:TransferSharedMemory";
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, transfer_buff_size=%d, recv_buff_size=%d, "
                            "unk1=%d, send_buff_size=%d, unk2=%d, baud_rate=%u, handle=0x%08X",
                transfer_buff_size, recv_buff_size, unk1, send_buff_size, unk2, baud_rate, handle);
}

/**
 * IR::RequireConnection service function
 *  Inputs:
 *      1 : unknown (u8), looks like always 1
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RequireConnection(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conn_status_event->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

/**
 * IR::Disconnect service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Disconnect(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

/**
 * IR::GetConnectionStatusEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Connection Status Event handle
 */
static void GetConnectionStatusEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::conn_status_event).MoveFrom();

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

/**
 * IR::FinalizeIrNop service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void FinalizeIrNop(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010182, nullptr, "InitializeIrNop"},
    {0x00020000, FinalizeIrNop, "FinalizeIrNop"},
    {0x00030000, nullptr, "ClearReceiveBuffer"},
    {0x00040000, nullptr, "ClearSendBuffer"},
    {0x000500C0, nullptr, "WaitConnection"},
    {0x00060040, RequireConnection, "RequireConnection"},
    {0x000702C0, nullptr, "AutoConnection"},
    {0x00080000, nullptr, "AnyConnection"},
    {0x00090000, Disconnect, "Disconnect"},
    {0x000A0000, nullptr, "GetReceiveEvent"},
    {0x000B0000, nullptr, "GetSendEvent"},
    {0x000C0000, GetConnectionStatusEvent, "GetConnectionStatusEvent"},
    {0x000D0042, nullptr, "SendIrNop"},
    {0x000E0042, nullptr, "SendIrNopLarge"},
    {0x000F0040, nullptr, "ReceiveIrnop"},
    {0x00100042, nullptr, "ReceiveIrnopLarge"},
    {0x00110040, nullptr, "GetLatestReceiveErrorResult"},
    {0x00120040, nullptr, "GetLatestSendErrorResult"},
    {0x00130000, nullptr, "GetConnectionStatus"},
    {0x00140000, nullptr, "GetTryingToConnectStatus"},
    {0x00150000, nullptr, "GetReceiveSizeFreeAndUsed"},
    {0x00160000, nullptr, "GetSendSizeFreeAndUsed"},
    {0x00170000, nullptr, "GetConnectionRole"},
    {0x00180182, InitializeIrNopShared, "InitializeIrNopShared"},
    {0x00190040, nullptr, "ReleaseReceivedData"},
    {0x001A0040, nullptr, "SetOwnMachineId"},
};

IR_User_Interface::IR_User_Interface() {
    Register(FunctionTable);
}

void InitUser() {
    using namespace Kernel;

    transfer_shared_memory = nullptr;
    conn_status_event = Event::Create(ResetType::OneShot, "IR:ConnectionStatusEvent");
}

void ShutdownUser() {
    transfer_shared_memory = nullptr;
    conn_status_event = nullptr;
}

} // namespace IR
} // namespace Service
