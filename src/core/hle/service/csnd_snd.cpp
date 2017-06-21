// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/alignment.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/csnd_snd.h"
#include "core/memory.h"

namespace Service {
namespace CSND {

struct Type0Command {
    // command id and next command offset
    u32 command_id;
    u32 finished;
    u32 flags;
    u8 parameters[20];
};
static_assert(sizeof(Type0Command) == 0x20, "Type0Command structure size is wrong");

static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory = nullptr;
static Kernel::SharedPtr<Kernel::Mutex> mutex = nullptr;

/**
 * CSND_SND::Initialize service function
 *  Inputs:
 *      0 : Header Code[0x00010140]
 *      1 : Shared memory block size, for mem-block creation
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Handle-list header
 *      3 : Mutex handle
 *      4 : Shared memory block handle
 */
static void Initialize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = Common::AlignUp(cmd_buff[1], Memory::PAGE_SIZE);

    using Kernel::MemoryPermission;
    shared_memory = Kernel::SharedMemory::Create(nullptr, size, MemoryPermission::ReadWrite,
                                                 MemoryPermission::ReadWrite, 0,
                                                 Kernel::MemoryRegion::BASE, "CSND:SharedMemory");

    mutex = Kernel::Mutex::Create(false, "CSND:mutex");

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc(2);
    cmd_buff[3] = Kernel::g_handle_table.Create(mutex).Unwrap();
    cmd_buff[4] = Kernel::g_handle_table.Create(shared_memory).Unwrap();

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

/**
 * CSND_SND::Shutdown service function
 *  Inputs:
 *      0 : Header Code[0x00020000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Shutdown(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    shared_memory = nullptr;
    mutex = nullptr;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

/**
 * CSND_SND::ExecuteCommands service function
 *  Inputs:
 *      0 : Header Code[0x00030040]
 *      1 : Command offset in shared memory.
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Available channel bit mask
 */
static void ExecuteCommands(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (shared_memory == nullptr) {
        cmd_buff[1] = 1;
        LOG_ERROR(Service_CSND, "called, shared memory not allocated");
        return;
    }

    VAddr addr = cmd_buff[1];
    u8* ptr = shared_memory->GetPointer(addr);

    Type0Command command;
    std::memcpy(&command, ptr, sizeof(Type0Command));
    command.finished |= 1;
    std::memcpy(ptr, &command, sizeof(Type0Command));

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CSND, "(STUBBED) called, addr=0x%08X", addr);
}

/**
 * CSND_SND::AcquireSoundChannels service function
 *  Inputs:
 *      0 : Header Code[0x00050000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Available channel bit mask
 */
static void AcquireSoundChannels(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0xFFFFFF00;
    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010140, Initialize, "Initialize"},
    {0x00020000, Shutdown, "Shutdown"},
    {0x00030040, ExecuteCommands, "ExecuteCommands"},
    {0x00040080, nullptr, "ExecuteType1Commands"},
    {0x00050000, AcquireSoundChannels, "AcquireSoundChannels"},
    {0x00060000, nullptr, "ReleaseSoundChannels"},
    {0x00070000, nullptr, "AcquireCaptureDevice"},
    {0x00080040, nullptr, "ReleaseCaptureDevice"},
    {0x00090082, nullptr, "FlushDataCache"},
    {0x000A0082, nullptr, "StoreDataCache"},
    {0x000B0082, nullptr, "InvalidateDataCache"},
    {0x000C0000, nullptr, "Reset"},
};

CSND_SND::CSND_SND() {
    Register(FunctionTable);
}

} // namespace CSND
} // namespace Service
