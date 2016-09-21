// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include "audio_core/hle/pipe.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/dsp_dsp.h"

using DspPipe = DSP::HLE::DspPipe;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace DSP_DSP

namespace DSP_DSP {

static Kernel::SharedPtr<Kernel::Event> semaphore_event;

/// There are three types of interrupts
enum class InterruptType { Zero, One, Pipe };
constexpr size_t NUM_INTERRUPT_TYPE = 3;

class InterruptEvents final {
public:
    void Signal(InterruptType type, DspPipe pipe) {
        Kernel::SharedPtr<Kernel::Event>& event = Get(type, pipe);
        if (event) {
            event->Signal();
        }
    }

    Kernel::SharedPtr<Kernel::Event>& Get(InterruptType type, DspPipe dsp_pipe) {
        switch (type) {
        case InterruptType::Zero:
            return zero;
        case InterruptType::One:
            return one;
        case InterruptType::Pipe: {
            const size_t pipe_index = static_cast<size_t>(dsp_pipe);
            ASSERT(pipe_index < DSP::HLE::NUM_DSP_PIPE);
            return pipe[pipe_index];
        }
        }

        UNREACHABLE_MSG("Invalid interrupt type = %zu", static_cast<size_t>(type));
    }

    bool HasTooManyEventsRegistered() const {
        // Actual service implementation only has 6 'slots' for interrupts.
        constexpr size_t max_number_of_interrupt_events = 6;

        size_t number =
            std::count_if(pipe.begin(), pipe.end(), [](const auto& evt) { return evt != nullptr; });

        if (zero != nullptr)
            number++;
        if (one != nullptr)
            number++;

        return number >= max_number_of_interrupt_events;
    }

private:
    /// Currently unknown purpose
    Kernel::SharedPtr<Kernel::Event> zero = nullptr;
    /// Currently unknown purpose
    Kernel::SharedPtr<Kernel::Event> one = nullptr;
    /// Each DSP pipe has an associated interrupt
    std::array<Kernel::SharedPtr<Kernel::Event>, DSP::HLE::NUM_DSP_PIPE> pipe = {{}};
};

static InterruptEvents interrupt_events;

// DSP Interrupts:
// The audio-pipe interrupt occurs every frame tick. Userland programs normally have a thread
// that's waiting for an interrupt event. Immediately after this interrupt event, userland
// normally updates the state in the next region and increments the relevant frame counter by
// two.
void SignalPipeInterrupt(DspPipe pipe) {
    interrupt_events.Signal(InterruptType::Pipe, pipe);
}

/**
 * DSP_DSP::ConvertProcessAddressFromDspDram service function
 *  Inputs:
 *      1 : Address
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : (inaddr << 1) + 0x1FF40000 (where 0x1FF00000 is the DSP RAM address)
 */
static void ConvertProcessAddressFromDspDram(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 addr = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0xC, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    // TODO(merry): There is a per-region offset missing in this calculation (that seems to be
    // always zero).
    cmd_buff[2] = (addr << 1) + (Memory::DSP_RAM_VADDR + 0x40000);

    LOG_DEBUG(Service_DSP, "addr=0x%08X", addr);
}

/**
 * DSP_DSP::LoadComponent service function
 *  Inputs:
 *      1 : Size
 *      2 : Program mask (observed only half word used)
 *      3 : Data mask (observed only half word used)
 *      4 : (size << 4) | 0xA
 *      5 : Buffer address
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Component loaded, 0 on not loaded, 1 on loaded
 */
static void LoadComponent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    u32 prog_mask = cmd_buff[2];
    u32 data_mask = cmd_buff[3];
    u32 desc = cmd_buff[4];
    u32 buffer = cmd_buff[5];

    cmd_buff[0] = IPC::MakeHeader(0x11, 2, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 1;                  // Pretend that we actually loaded the DSP firmware
    cmd_buff[3] = desc;
    cmd_buff[4] = buffer;

    // TODO(bunnei): Implement real DSP firmware loading

    ASSERT(Memory::IsValidVirtualAddress(buffer));

    std::vector<u8> component_data(size);
    Memory::ReadBlock(buffer, component_data.data(), component_data.size());

    LOG_INFO(Service_DSP, "Firmware hash: %#" PRIx64,
             Common::ComputeHash64(component_data.data(), component_data.size()));
    // Some versions of the firmware have the location of DSP structures listed here.
    ASSERT(size > 0x37C);
    LOG_INFO(Service_DSP, "Structures hash: %#" PRIx64,
             Common::ComputeHash64(component_data.data() + 0x340, 60));

    LOG_WARNING(Service_DSP,
                "(STUBBED) called size=0x%X, prog_mask=0x%08X, data_mask=0x%08X, buffer=0x%08X",
                size, prog_mask, data_mask, buffer);
}

/**
 * DSP_DSP::GetSemaphoreEventHandle service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : Semaphore event handle
 */
static void GetSemaphoreEventHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x16, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    // cmd_buff[2] not set
    cmd_buff[3] = Kernel::g_handle_table.Create(semaphore_event).MoveFrom(); // Event handle

    LOG_WARNING(Service_DSP, "(STUBBED) called");
}

/**
 * DSP_DSP::FlushDataCache service function
 *
 * This Function is a no-op, We aren't emulating the CPU cache any time soon.
 *
 *  Inputs:
 *      1 : Address
 *      2 : Size
 *      3 : Value 0, some descriptor for the KProcess Handle
 *      4 : KProcess handle
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void FlushDataCache(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 address = cmd_buff[1];
    u32 size = cmd_buff[2];
    u32 process = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x13, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_TRACE(Service_DSP, "called address=0x%08X, size=0x%X, process=0x%08X", address, size,
              process);
}

/**
 * DSP_DSP::RegisterInterruptEvents service function
 *  Inputs:
 *      1 : Interrupt Type
 *      2 : Pipe Number
 *      4 : Interrupt event handle
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RegisterInterruptEvents(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 type_index = cmd_buff[1];
    u32 pipe_index = cmd_buff[2];
    u32 event_handle = cmd_buff[4];

    ASSERT_MSG(type_index < NUM_INTERRUPT_TYPE && pipe_index < DSP::HLE::NUM_DSP_PIPE,
               "Invalid type or pipe: type = %u, pipe = %u", type_index, pipe_index);

    InterruptType type = static_cast<InterruptType>(cmd_buff[1]);
    DspPipe pipe = static_cast<DspPipe>(cmd_buff[2]);

    cmd_buff[0] = IPC::MakeHeader(0x15, 1, 0);

    if (event_handle) {
        auto evt = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);

        if (!evt) {
            LOG_INFO(Service_DSP, "Invalid event handle! type=%u, pipe=%u, event_handle=0x%08X",
                     type_index, pipe_index, event_handle);
            ASSERT(false); // TODO: This should really be handled at an IPC translation layer.
        }

        if (interrupt_events.HasTooManyEventsRegistered()) {
            LOG_INFO(Service_DSP, "Ran out of space to register interrupts (Attempted to register "
                                  "type=%u, pipe=%u, event_handle=0x%08X)",
                     type_index, pipe_index, event_handle);
            cmd_buff[1] = ResultCode(ErrorDescription::InvalidResultValue, ErrorModule::DSP,
                                     ErrorSummary::OutOfResource, ErrorLevel::Status)
                              .raw;
            return;
        }

        interrupt_events.Get(type, pipe) = evt;
        LOG_INFO(Service_DSP, "Registered type=%u, pipe=%u, event_handle=0x%08X", type_index,
                 pipe_index, event_handle);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        interrupt_events.Get(type, pipe) = nullptr;
        LOG_INFO(Service_DSP, "Unregistered interrupt=%u, channel=%u, event_handle=0x%08X",
                 type_index, pipe_index, event_handle);
        cmd_buff[1] = RESULT_SUCCESS.raw;
    }
}

/**
 * DSP_DSP::SetSemaphore service function
 *  Inputs:
 *      1 : Unknown (observed only half word used)
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetSemaphore(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_DSP, "(STUBBED) called");
}

/**
 * DSP_DSP::WriteProcessPipe service function
 *  Inputs:
 *      1 : Pipe Number
 *      2 : Size
 *      3 : (size << 14) | 0x402
 *      4 : Buffer
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void WriteProcessPipe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 pipe_index = cmd_buff[1];
    u32 size = cmd_buff[2];
    u32 buffer = cmd_buff[4];

    DSP::HLE::DspPipe pipe = static_cast<DSP::HLE::DspPipe>(pipe_index);

    if (IPC::StaticBufferDesc(size, 1) != cmd_buff[3]) {
        LOG_ERROR(Service_DSP, "IPC static buffer descriptor failed validation (0x%X). pipe=%u, "
                               "size=0x%X, buffer=0x%08X",
                  cmd_buff[3], pipe_index, size, buffer);
        cmd_buff[0] = IPC::MakeHeader(0, 1, 0);
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent)
                          .raw;
        return;
    }

    ASSERT_MSG(Memory::IsValidVirtualAddress(buffer),
               "Invalid Buffer: pipe=%u, size=0x%X, buffer=0x%08X", pipe, size, buffer);

    std::vector<u8> message(size);
    for (u32 i = 0; i < size; i++) {
        message[i] = Memory::Read8(buffer + i);
    }

    DSP::HLE::PipeWrite(pipe, message);

    cmd_buff[0] = IPC::MakeHeader(0xD, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_DEBUG(Service_DSP, "pipe=%u, size=0x%X, buffer=0x%08X", pipe_index, size, buffer);
}

/**
 * DSP_DSP::ReadPipeIfPossible service function
 *      A pipe is a means of communication between the ARM11 and DSP that occurs on
 *      hardware by writing to/reading from the DSP registers at 0x10203000.
 *      Pipes are used for initialisation. See also DSP::HLE::PipeRead.
 *  Inputs:
 *      1 : Pipe Number
 *      2 : Unknown
 *      3 : Size in bytes of read (observed only lower half word used)
 *      0x41 : Virtual address of memory buffer to write pipe contents to
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Number of bytes read from pipe
 */
static void ReadPipeIfPossible(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 pipe_index = cmd_buff[1];
    u32 unknown = cmd_buff[2];
    u32 size = cmd_buff[3] & 0xFFFF; // Lower 16 bits are size
    VAddr addr = cmd_buff[0x41];

    DSP::HLE::DspPipe pipe = static_cast<DSP::HLE::DspPipe>(pipe_index);

    ASSERT_MSG(Memory::IsValidVirtualAddress(addr),
               "Invalid addr: pipe=0x%08X, unknown=0x%08X, size=0x%X, buffer=0x%08X", pipe, unknown,
               size, addr);

    cmd_buff[0] = IPC::MakeHeader(0x10, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    if (DSP::HLE::GetPipeReadableSize(pipe) >= size) {
        std::vector<u8> response = DSP::HLE::PipeRead(pipe, size);

        Memory::WriteBlock(addr, response.data(), response.size());

        cmd_buff[2] = static_cast<u32>(response.size());
    } else {
        cmd_buff[2] = 0; // Return no data
    }
    cmd_buff[3] = IPC::StaticBufferDesc(size, 0);
    cmd_buff[4] = addr;

    LOG_DEBUG(Service_DSP,
              "pipe=%u, unknown=0x%08X, size=0x%X, buffer=0x%08X, return cmd_buff[2]=0x%08X",
              pipe_index, unknown, size, addr, cmd_buff[2]);
}

/**
 * DSP_DSP::ReadPipe service function
 *  Inputs:
 *      1 : Pipe Number
 *      2 : Unknown
 *      3 : Size in bytes of read (observed only lower half word used)
 *      0x41 : Virtual address of memory buffer to write pipe contents to
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Number of bytes read from pipe
 */
static void ReadPipe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 pipe_index = cmd_buff[1];
    u32 unknown = cmd_buff[2];
    u32 size = cmd_buff[3] & 0xFFFF; // Lower 16 bits are size
    VAddr addr = cmd_buff[0x41];

    DSP::HLE::DspPipe pipe = static_cast<DSP::HLE::DspPipe>(pipe_index);

    ASSERT_MSG(Memory::IsValidVirtualAddress(addr),
               "Invalid addr: pipe=0x%08X, unknown=0x%08X, size=0x%X, buffer=0x%08X", pipe, unknown,
               size, addr);

    if (DSP::HLE::GetPipeReadableSize(pipe) >= size) {
        std::vector<u8> response = DSP::HLE::PipeRead(pipe, size);

        Memory::WriteBlock(addr, response.data(), response.size());

        cmd_buff[0] = IPC::MakeHeader(0xE, 2, 2);
        cmd_buff[1] = RESULT_SUCCESS.raw; // No error
        cmd_buff[2] = static_cast<u32>(response.size());
        cmd_buff[3] = IPC::StaticBufferDesc(size, 0);
        cmd_buff[4] = addr;
    } else {
        // No more data is in pipe. Hardware hangs in this case; this should never happen.
        UNREACHABLE();
    }

    LOG_DEBUG(Service_DSP,
              "pipe=%u, unknown=0x%08X, size=0x%X, buffer=0x%08X, return cmd_buff[2]=0x%08X",
              pipe_index, unknown, size, addr, cmd_buff[2]);
}

/**
 * DSP_DSP::GetPipeReadableSize service function
 *  Inputs:
 *      1 : Pipe Number
 *      2 : Unknown
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Number of bytes readable from pipe
 */
static void GetPipeReadableSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 pipe_index = cmd_buff[1];
    u32 unknown = cmd_buff[2];

    DSP::HLE::DspPipe pipe = static_cast<DSP::HLE::DspPipe>(pipe_index);

    cmd_buff[0] = IPC::MakeHeader(0xF, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(DSP::HLE::GetPipeReadableSize(pipe));

    LOG_DEBUG(Service_DSP, "pipe=%u, unknown=0x%08X, return cmd_buff[2]=0x%08X", pipe_index,
              unknown, cmd_buff[2]);
}

/**
 * DSP_DSP::SetSemaphoreMask service function
 *  Inputs:
 *      1 : Mask
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetSemaphoreMask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 mask = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x17, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_DSP, "(STUBBED) called mask=0x%08X", mask);
}

/**
 * DSP_DSP::GetHeadphoneStatus service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : The headphone status response, 0 = Not using headphones?,
 *          1 = using headphones?
 */
static void GetHeadphoneStatus(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1F, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Not using headphones

    LOG_DEBUG(Service_DSP, "called");
}

/**
 * DSP_DSP::RecvData service function
 *      This function reads a value out of a DSP register.
 *  Inputs:
 *      1 : Register Number
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Value in the register
 *  Notes:
 *      This function has only been observed being called with a register number of 0.
 */
static void RecvData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 register_number = cmd_buff[1];

    ASSERT_MSG(register_number == 0, "Unknown register_number %u", register_number);

    // Application reads this after requesting DSP shutdown, to verify the DSP has indeed shutdown
    // or slept.

    cmd_buff[0] = IPC::MakeHeader(0x1, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    switch (DSP::HLE::GetDspState()) {
    case DSP::HLE::DspState::On:
        cmd_buff[2] = 0;
        break;
    case DSP::HLE::DspState::Off:
    case DSP::HLE::DspState::Sleeping:
        cmd_buff[2] = 1;
        break;
    default:
        UNREACHABLE();
        break;
    }

    LOG_DEBUG(Service_DSP, "register_number=%u", register_number);
}

/**
 * DSP_DSP::RecvDataIsReady service function
 *      This function checks whether a DSP register is ready to be read.
 *  Inputs:
 *      1 : Register Number
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : non-zero == ready
 *  Note:
 *      This function has only been observed being called with a register number of 0.
 */
static void RecvDataIsReady(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 register_number = cmd_buff[1];

    ASSERT_MSG(register_number == 0, "Unknown register_number %u", register_number);

    cmd_buff[0] = IPC::MakeHeader(0x2, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1; // Ready to read

    LOG_DEBUG(Service_DSP, "register_number=%u", register_number);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, RecvData, "RecvData"},
    {0x00020040, RecvDataIsReady, "RecvDataIsReady"},
    {0x00030080, nullptr, "SendData"},
    {0x00040040, nullptr, "SendDataIsEmpty"},
    {0x000500C2, nullptr, "SendFifoEx"},
    {0x000600C0, nullptr, "RecvFifoEx"},
    {0x00070040, SetSemaphore, "SetSemaphore"},
    {0x00080000, nullptr, "GetSemaphore"},
    {0x00090040, nullptr, "ClearSemaphore"},
    {0x000A0040, nullptr, "MaskSemaphore"},
    {0x000B0000, nullptr, "CheckSemaphoreRequest"},
    {0x000C0040, ConvertProcessAddressFromDspDram, "ConvertProcessAddressFromDspDram"},
    {0x000D0082, WriteProcessPipe, "WriteProcessPipe"},
    {0x000E00C0, ReadPipe, "ReadPipe"},
    {0x000F0080, GetPipeReadableSize, "GetPipeReadableSize"},
    {0x001000C0, ReadPipeIfPossible, "ReadPipeIfPossible"},
    {0x001100C2, LoadComponent, "LoadComponent"},
    {0x00120000, nullptr, "UnloadComponent"},
    {0x00130082, FlushDataCache, "FlushDataCache"},
    {0x00140082, nullptr, "InvalidateDCache"},
    {0x00150082, RegisterInterruptEvents, "RegisterInterruptEvents"},
    {0x00160000, GetSemaphoreEventHandle, "GetSemaphoreEventHandle"},
    {0x00170040, SetSemaphoreMask, "SetSemaphoreMask"},
    {0x00180040, nullptr, "GetPhysicalAddress"},
    {0x00190040, nullptr, "GetVirtualAddress"},
    {0x001A0042, nullptr, "SetIirFilterI2S1_cmd1"},
    {0x001B0042, nullptr, "SetIirFilterI2S1_cmd2"},
    {0x001C0082, nullptr, "SetIirFilterEQ"},
    {0x001D00C0, nullptr, "ReadMultiEx_SPI2"},
    {0x001E00C2, nullptr, "WriteMultiEx_SPI2"},
    {0x001F0000, GetHeadphoneStatus, "GetHeadphoneStatus"},
    {0x00200040, nullptr, "ForceHeadphoneOut"},
    {0x00210000, nullptr, "GetIsDspOccupied"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    semaphore_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "DSP_DSP::semaphore_event");
    interrupt_events = {};

    Register(FunctionTable);
}

Interface::~Interface() {
    semaphore_event = nullptr;
    interrupt_events = {};
}

} // namespace
