// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/alignment.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/csnd_snd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CSND_SND

namespace CSND_SND {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010140, Initialize, "Initialize"},
    {0x00020000, Shutdown, "Shutdown"},
    {0x00030040, ExecuteType0Commands, "ExecuteType0Commands"},
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

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

static Kernel::SharedPtr<Kernel::SharedMemory> shared_memory = nullptr;
static Kernel::SharedPtr<Kernel::Mutex> mutex = nullptr;

void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = Common::AlignUp(cmd_buff[1], Memory::PAGE_SIZE);
    using Kernel::MemoryPermission;
    shared_memory = Kernel::SharedMemory::Create(nullptr, size, MemoryPermission::ReadWrite,
                                                 MemoryPermission::ReadWrite, 0,
                                                 Kernel::MemoryRegion::BASE, "CSND:SharedMemory");

    mutex = Kernel::Mutex::Create(false);

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc(2);
    cmd_buff[3] = Kernel::g_handle_table.Create(mutex).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(shared_memory).MoveFrom();
}

void ExecuteType0Commands(Service::Interface* self) {
    u32* const cmd_buff = Kernel::GetCommandBuffer();
    u8* const ptr = shared_memory->GetPointer(cmd_buff[1]);

    if (shared_memory != nullptr && ptr != nullptr) {
        Type0Command command;
        std::memcpy(&command, ptr, sizeof(Type0Command));

        LOG_WARNING(Service, "(STUBBED) CSND_SND::ExecuteType0Commands");
        command.finished |= 1;
        cmd_buff[1] = 0;

        std::memcpy(ptr, &command, sizeof(Type0Command));
    } else {
        cmd_buff[1] = 1;
    }
}

void AcquireSoundChannels(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = 0;
    cmd_buff[2] = 0xFFFFFF00;
}

void Shutdown(Service::Interface* self) {
    shared_memory = nullptr;
    mutex = nullptr;
}

} // namespace
