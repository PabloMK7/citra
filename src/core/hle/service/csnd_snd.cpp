// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/csnd_snd.h"

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

void CSND_SND::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 5, 0);
    u32 size = Common::AlignUp(rp.Pop<u32>(), Memory::PAGE_SIZE);
    VAddr offset0 = rp.Pop<u32>();
    VAddr offset1 = rp.Pop<u32>();
    VAddr offset2 = rp.Pop<u32>();
    VAddr offset3 = rp.Pop<u32>();

    using Kernel::MemoryPermission;
    mutex = Kernel::Mutex::Create(false, "CSND:mutex");
    shared_memory = Kernel::SharedMemory::Create(nullptr, size, MemoryPermission::ReadWrite,
                                                 MemoryPermission::ReadWrite, 0,
                                                 Kernel::MemoryRegion::BASE, "CSND:SharedMemory");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(mutex, shared_memory);

    LOG_WARNING(Service_CSND, "(STUBBED) called, size=0x{:08X} "
                              "offset0=0x{:08X} offset1=0x{:08X} offset2=0x{:08X} offset3=0x{:08X}",
                              size, offset0, offset1, offset2, offset3);
}

void CSND_SND::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 0, 0);

    if (mutex)
        mutex = nullptr;
    if (shared_memory)
        shared_memory = nullptr;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::ExecuteCommands(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 1, 0);
    VAddr addr = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (!shared_memory) {
        rb.Push<u32>(1);
        rb.Skip(1, false);
        LOG_ERROR(Service_CSND, "called, shared memory not allocated");
    } else {
        u8* ptr = shared_memory->GetPointer(addr);
        Type0Command command;

        std::memcpy(&command, ptr, sizeof(Type0Command));
        command.finished |= 1;
        std::memcpy(ptr, &command, sizeof(Type0Command));

        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0xFFFFFF00);
    }

    LOG_WARNING(Service_CSND, "(STUBBED) called, addr=0x{:08X}", addr);
}

void CSND_SND::AcquireSoundChannels(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0xFFFFFF00);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::ReleaseSoundChannels(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 2, 2);
    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void CSND_SND::StoreDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xA, 2, 2);
    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void CSND_SND::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 2, 2);
    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

CSND_SND::CSND_SND() : ServiceFramework("csnd:SND", 4) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x00010140, &CSND_SND::Initialize, "Initialize"},
        {0x00020000, &CSND_SND::Shutdown, "Shutdown"},
        {0x00030040, &CSND_SND::ExecuteCommands, "ExecuteCommands"},
        {0x00040080, nullptr, "ExecuteType1Commands"},
        {0x00050000, &CSND_SND::AcquireSoundChannels, "AcquireSoundChannels"},
        {0x00060000, &CSND_SND::ReleaseSoundChannels, "ReleaseSoundChannels"},
        {0x00070000, nullptr, "AcquireCaptureDevice"},
        {0x00080040, nullptr, "ReleaseCaptureDevice"},
        {0x00090082, &CSND_SND::FlushDataCache, "FlushDataCache"},
        {0x000A0082, &CSND_SND::StoreDataCache, "StoreDataCache"},
        {0x000B0082, &CSND_SND::InvalidateDataCache, "InvalidateDataCache"},
        {0x000C0000, nullptr, "Reset"},
        // clang-format on
    };

    RegisterHandlers(functions);
};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<CSND_SND>()->InstallAsService(service_manager);
}

} // namespace CSND
} // namespace Service
