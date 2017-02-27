// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/ir/ir.h"
#include "core/hle/service/ir/ir_rst.h"

namespace Service {
namespace IR {

static Kernel::SharedPtr<Kernel::Event> handle_event;
static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory;

/**
 * IR::GetHandles service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Translate header, used by the ARM11-kernel
 *      3 : Shared memory handle
 *      4 : Event handle
 */
static void GetHandles(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0x4000000;
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::IR::shared_memory).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::IR::handle_event).MoveFrom();
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, GetHandles, "GetHandles"},
    {0x00020080, nullptr, "Initialize"},
    {0x00030000, nullptr, "Shutdown"},
    {0x00090000, nullptr, "WriteToTwoFields"},
};

IR_RST_Interface::IR_RST_Interface() {
    Register(FunctionTable);
}

void InitRST() {
    using namespace Kernel;

    shared_memory =
        SharedMemory::Create(nullptr, 0x1000, MemoryPermission::ReadWrite,
                             MemoryPermission::ReadWrite, 0, MemoryRegion::BASE, "IR:SharedMemory");

    handle_event = Event::Create(ResetType::OneShot, "IR:HandleEvent");
}

void ShutdownRST() {
    shared_memory = nullptr;
    handle_event = nullptr;
}

} // namespace IR
} // namespace Service
