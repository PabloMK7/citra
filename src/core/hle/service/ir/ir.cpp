// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_u.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

static Kernel::SharedPtr<Kernel::Event> handle_event;
static Kernel::SharedPtr<Kernel::Event> conn_status_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;
static Kernel::SharedPtr<Kernel::SharedMemory> transfer_shared_memory;

void GetHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0x4000000;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::shared_memory).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::IR::handle_event).MoveFrom();
}

void InitializeIrNopShared(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 transfer_buff_size = cmd_buff[1];
    u32 recv_buff_size = cmd_buff[2];
    u32 unk1 = cmd_buff[3];
    u32 send_buff_size = cmd_buff[4];
    u32 unk2 = cmd_buff[5];
    u8 baud_rate = cmd_buff[6] & 0xFF;
    Handle handle = cmd_buff[8];

    if (Kernel::g_handle_table.IsValid(handle)) {
        transfer_shared_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(handle);
        transfer_shared_memory->name = "IR:TransferSharedMemory";
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called, transfer_buff_size=%d, recv_buff_size=%d, "
                            "unk1=%d, send_buff_size=%d, unk2=%d, baud_rate=%u, handle=0x%08X",
                transfer_buff_size, recv_buff_size, unk1, send_buff_size, unk2, baud_rate, handle);
}

void RequireConnection(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conn_status_event->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void Disconnect(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void GetConnectionStatusEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::conn_status_event).MoveFrom();

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void FinalizeIrNop(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_IR, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new IR_RST_Interface);
    AddService(new IR_U_Interface);
    AddService(new IR_User_Interface);

    using Kernel::MemoryPermission;
    shared_memory = SharedMemory::Create(nullptr, 0x1000, Kernel::MemoryPermission::ReadWrite,
                                         Kernel::MemoryPermission::ReadWrite, 0,
                                         Kernel::MemoryRegion::BASE, "IR:SharedMemory");
    transfer_shared_memory = nullptr;

    // Create event handle(s)
    handle_event = Event::Create(ResetType::OneShot, "IR:HandleEvent");
    conn_status_event = Event::Create(ResetType::OneShot, "IR:ConnectionStatusEvent");
}

void Shutdown() {
    transfer_shared_memory = nullptr;
    shared_memory = nullptr;
    handle_event = nullptr;
    conn_status_event = nullptr;
}

} // namespace IR

} // namespace Service
