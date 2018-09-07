// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/audio_types.h"
#include "common/assert.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/dsp/dsp_dsp.h"

using DspPipe = AudioCore::DspPipe;
using InterruptType = Service::DSP::DSP_DSP::InterruptType;

namespace AudioCore {
enum class DspPipe;
}

namespace Service {
namespace DSP {

void DSP_DSP::RecvData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 0);
    const u32 register_number = rp.Pop<u32>();

    ASSERT_MSG(register_number == 0, "Unknown register_number {}", register_number);

    // Application reads this after requesting DSP shutdown, to verify the DSP has indeed shutdown
    // or slept.

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);

    switch (Core::DSP().GetDspState()) {
    case AudioCore::DspState::On:
        rb.Push<u32>(0);
        break;
    case AudioCore::DspState::Off:
    case AudioCore::DspState::Sleeping:
        rb.Push<u32>(1);
        break;
    default:
        UNREACHABLE();
        break;
    }

    LOG_DEBUG(Service_DSP, "register_number={}", register_number);
}

void DSP_DSP::RecvDataIsReady(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 0);
    const u32 register_number = rp.Pop<u32>();

    ASSERT_MSG(register_number == 0, "Unknown register_number {}", register_number);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(true); /// 0 = not ready, 1 = ready to read

    LOG_DEBUG(Service_DSP, "register_number={}", register_number);
}

void DSP_DSP::SetSemaphore(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 1, 0);
    const u16 semaphore_value = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_DSP, "(STUBBED) called, semaphore_value={:04X}", semaphore_value);
}

void DSP_DSP::ConvertProcessAddressFromDspDram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 1, 0);
    const u32 address = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);

    // TODO(merry): There is a per-region offset missing in this calculation (that seems to be
    // always zero).
    rb.Push<u32>((address << 1) + (Memory::DSP_RAM_VADDR + 0x40000));

    LOG_DEBUG(Service_DSP, "address=0x{:08X}", address);
}

void DSP_DSP::WriteProcessPipe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 2, 2);
    const u32 channel = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto buffer = rp.PopStaticBuffer();

    const DspPipe pipe = static_cast<DspPipe>(channel);

    // This behaviour was confirmed by RE.
    // The likely reason for this is that games tend to pass in garbage at these bytes
    // because they read random bytes off the stack.
    switch (pipe) {
    case DspPipe::Audio:
        ASSERT(buffer.size() >= 4);
        buffer[2] = 0;
        buffer[3] = 0;
        break;
    case DspPipe::Binary:
        ASSERT(buffer.size() >= 8);
        buffer[4] = 1;
        buffer[5] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
        break;
    }

    Core::DSP().PipeWrite(pipe, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_DSP, "channel={}, size=0x{:X}, buffer_size={:X}", channel, size,
              buffer.size());
}

void DSP_DSP::ReadPipe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 3, 0);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();
    const u16 size = rp.Pop<u16>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(Core::DSP().GetPipeReadableSize(pipe));

    std::vector<u8> pipe_buffer;
    if (pipe_readable_size >= size)
        pipe_buffer = Core::DSP().PipeRead(pipe, size);
    else
        UNREACHABLE(); // No more data is in pipe. Hardware hangs in this case; Should never happen.

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(pipe_buffer), 0);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, size=0x{:04X}, pipe_readable_size=0x{:04X}",
              channel, peer, size, pipe_readable_size);
}

void DSP_DSP::GetPipeReadableSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 2, 0);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(Core::DSP().GetPipeReadableSize(pipe));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(pipe_readable_size);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, return pipe_readable_size=0x{:04X}", channel, peer,
              pipe_readable_size);
}

void DSP_DSP::ReadPipeIfPossible(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 3, 0);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();
    const u16 size = rp.Pop<u16>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(Core::DSP().GetPipeReadableSize(pipe));

    std::vector<u8> pipe_buffer;
    if (pipe_readable_size >= size)
        pipe_buffer = Core::DSP().PipeRead(pipe, size);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u16>(pipe_readable_size);
    rb.PushStaticBuffer(pipe_buffer, 0);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, size=0x{:04X}, pipe_readable_size=0x{:04X}",
              channel, peer, size, pipe_readable_size);
}

void DSP_DSP::LoadComponent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 3, 2);
    const u32 size = rp.Pop<u32>();
    const u32 prog_mask = rp.Pop<u32>();
    const u32 data_mask = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push(true); /// Pretend that we actually loaded the DSP firmware
    rb.PushMappedBuffer(buffer);

    // TODO(bunnei): Implement real DSP firmware loading

    std::vector<u8> component_data(size);
    buffer.Read(component_data.data(), 0, size);

    LOG_INFO(Service_DSP, "Firmware hash: {:#018x}",
             Common::ComputeHash64(component_data.data(), component_data.size()));
    // Some versions of the firmware have the location of DSP structures listed here.
    if (size > 0x37C) {
        LOG_INFO(Service_DSP, "Structures hash: {:#018x}",
                 Common::ComputeHash64(component_data.data() + 0x340, 60));
    }
    LOG_WARNING(Service_DSP, "(STUBBED) called size=0x{:X}, prog_mask=0x{:08X}, data_mask=0x{:08X}",
                size, prog_mask, data_mask);
}

void DSP_DSP::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 2, 2);
    const VAddr address = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_TRACE(Service_DSP, "called address=0x{:08X}, size=0x{:X}, process={}", address, size,
              process->process_id);
}

void DSP_DSP::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 2, 2);
    const VAddr address = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_TRACE(Service_DSP, "called address=0x{:08X}, size=0x{:X}, process={}", address, size,
              process->process_id);
}

void DSP_DSP::RegisterInterruptEvents(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 2, 2);
    const u32 interrupt = rp.Pop<u32>();
    const u32 channel = rp.Pop<u32>();
    auto event = rp.PopObject<Kernel::Event>();

    ASSERT_MSG(interrupt < NUM_INTERRUPT_TYPE && channel < AudioCore::num_dsp_pipe,
               "Invalid type or pipe: interrupt = {}, channel = {}", interrupt, channel);

    const InterruptType type = static_cast<InterruptType>(interrupt);
    const DspPipe pipe = static_cast<DspPipe>(channel);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (event) { /// Register interrupt event
        if (HasTooManyEventsRegistered()) {
            LOG_INFO(Service_DSP,
                     "Ran out of space to register interrupts (Attempted to register "
                     "interrupt={}, channel={}, event={})",
                     interrupt, channel, event->GetName());
            rb.Push(ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::DSP,
                               ErrorSummary::OutOfResource, ErrorLevel::Status));
            return;
        } else {
            GetInterruptEvent(type, pipe) = event;
            LOG_INFO(Service_DSP, "Registered interrupt={}, channel={}, event={}", interrupt,
                     channel, event->GetName());
        }
    } else { /// Otherwise unregister event
        GetInterruptEvent(type, pipe) = nullptr;
        LOG_INFO(Service_DSP, "Unregistered interrupt={}, channel={}", interrupt, channel);
    }
    rb.Push(RESULT_SUCCESS);
}

void DSP_DSP::GetSemaphoreEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(semaphore_event);

    LOG_WARNING(Service_DSP, "(STUBBED) called");
}

void DSP_DSP::SetSemaphoreMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 1, 0);
    const u32 mask = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_DSP, "(STUBBED) called mask=0x{:08X}", mask);
}

void DSP_DSP::GetHeadphoneStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false); /// u8, 0 = not inserted, 1 = inserted

    LOG_DEBUG(Service_DSP, "called");
}

void DSP_DSP::ForceHeadphoneOut(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x20, 1, 0);
    const u8 force = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_DSP, "(STUBBED) called, force={}", force);
}

// DSP Interrupts:
// The audio-pipe interrupt occurs every frame tick. Userland programs normally have a thread
// that's waiting for an interrupt event. Immediately after this interrupt event, userland
// normally updates the state in the next region and increments the relevant frame counter by two.
void DSP_DSP::SignalInterrupt(InterruptType type, DspPipe pipe) {
    LOG_DEBUG(Service_DSP, "called, type={}, pipe={}", static_cast<u32>(type),
              static_cast<u32>(pipe));
    const auto& event = GetInterruptEvent(type, pipe);
    if (event)
        event->Signal();
}

Kernel::SharedPtr<Kernel::Event>& DSP_DSP::GetInterruptEvent(InterruptType type, DspPipe pipe) {
    switch (type) {
    case InterruptType::Zero:
        return interrupt_zero;
    case InterruptType::One:
        return interrupt_one;
    case InterruptType::Pipe: {
        const std::size_t pipe_index = static_cast<std::size_t>(pipe);
        ASSERT(pipe_index < AudioCore::num_dsp_pipe);
        return pipes[pipe_index];
    }
    }
    UNREACHABLE_MSG("Invalid interrupt type = {}", static_cast<std::size_t>(type));
}

bool DSP_DSP::HasTooManyEventsRegistered() const {
    std::size_t number =
        std::count_if(pipes.begin(), pipes.end(), [](const auto& evt) { return evt != nullptr; });

    if (interrupt_zero != nullptr)
        number++;
    if (interrupt_one != nullptr)
        number++;

    LOG_DEBUG(Service_DSP, "Number of events registered = {}", number);
    return number >= max_number_of_interrupt_events;
}

DSP_DSP::DSP_DSP() : ServiceFramework("dsp::DSP", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x00010040, &DSP_DSP::RecvData, "RecvData"},
        {0x00020040, &DSP_DSP::RecvDataIsReady, "RecvDataIsReady"},
        {0x00030080, nullptr, "SendData"},
        {0x00040040, nullptr, "SendDataIsEmpty"},
        {0x000500C2, nullptr, "SendFifoEx"},
        {0x000600C0, nullptr, "RecvFifoEx"},
        {0x00070040, &DSP_DSP::SetSemaphore, "SetSemaphore"},
        {0x00080000, nullptr, "GetSemaphore"},
        {0x00090040, nullptr, "ClearSemaphore"},
        {0x000A0040, nullptr, "MaskSemaphore"},
        {0x000B0000, nullptr, "CheckSemaphoreRequest"},
        {0x000C0040, &DSP_DSP::ConvertProcessAddressFromDspDram, "ConvertProcessAddressFromDspDram"},
        {0x000D0082, &DSP_DSP::WriteProcessPipe, "WriteProcessPipe"},
        {0x000E00C0, &DSP_DSP::ReadPipe, "ReadPipe"},
        {0x000F0080, &DSP_DSP::GetPipeReadableSize, "GetPipeReadableSize"},
        {0x001000C0, &DSP_DSP::ReadPipeIfPossible, "ReadPipeIfPossible"},
        {0x001100C2, &DSP_DSP::LoadComponent, "LoadComponent"},
        {0x00120000, nullptr, "UnloadComponent"},
        {0x00130082, &DSP_DSP::FlushDataCache, "FlushDataCache"},
        {0x00140082, &DSP_DSP::InvalidateDataCache, "InvalidateDCache"},
        {0x00150082, &DSP_DSP::RegisterInterruptEvents, "RegisterInterruptEvents"},
        {0x00160000, &DSP_DSP::GetSemaphoreEventHandle, "GetSemaphoreEventHandle"},
        {0x00170040, &DSP_DSP::SetSemaphoreMask, "SetSemaphoreMask"},
        {0x00180040, nullptr, "GetPhysicalAddress"},
        {0x00190040, nullptr, "GetVirtualAddress"},
        {0x001A0042, nullptr, "SetIirFilterI2S1_cmd1"},
        {0x001B0042, nullptr, "SetIirFilterI2S1_cmd2"},
        {0x001C0082, nullptr, "SetIirFilterEQ"},
        {0x001D00C0, nullptr, "ReadMultiEx_SPI2"},
        {0x001E00C2, nullptr, "WriteMultiEx_SPI2"},
        {0x001F0000, &DSP_DSP::GetHeadphoneStatus, "GetHeadphoneStatus"},
        {0x00200040, &DSP_DSP::ForceHeadphoneOut, "ForceHeadphoneOut"},
        {0x00210000, nullptr, "GetIsDspOccupied"},
        // clang-format on
    };

    RegisterHandlers(functions);

    semaphore_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "DSP_DSP::semaphore_event");
}

DSP_DSP::~DSP_DSP() {
    semaphore_event = nullptr;
    pipes = {};
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto dsp = std::make_shared<DSP_DSP>();
    dsp->InstallAsService(service_manager);
    Core::DSP().SetServiceToInterrupt(std::move(dsp));
}

} // namespace DSP
} // namespace Service
