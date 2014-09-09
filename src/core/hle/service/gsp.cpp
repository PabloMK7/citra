// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"
#include "common/bit_field.h"

#include "core/mem_map.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/gsp.h"
#include "core/hw/gpu.h"

#include "video_core/gpu_debugger.h"

// Main graphics debugger object - TODO: Here is probably not the best place for this
GraphicsDebugger g_debugger;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

Handle g_interrupt_event = 0;   ///< Handle to event triggered when GSP interrupt has been signalled
Handle g_shared_memory = 0;     ///< Handle to GSP shared memorys
u32 g_thread_id = 1;            ///< Thread index into interrupt relay queue, 1 is arbitrary

/// Gets a pointer to a thread command buffer in GSP shared memory
static inline u8* GetCommandBuffer(u32 thread_id) {
    if (0 == g_shared_memory)
        return nullptr;

    return Kernel::GetSharedMemoryPointer(g_shared_memory,
        0x800 + (thread_id * sizeof(CommandBuffer)));
}

static inline FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index) {
    if (0 == g_shared_memory)
        return nullptr;

    _dbg_assert_msg_(GSP, screen_index < 2, "Invalid screen index");

    // For each thread there are two FrameBufferUpdate fields
    u32 offset = 0x200 + (2 * thread_id + screen_index) * sizeof(FrameBufferUpdate);
    return (FrameBufferUpdate*)Kernel::GetSharedMemoryPointer(g_shared_memory, offset);
}

/// Gets a pointer to the interrupt relay queue for a given thread index
static inline InterruptRelayQueue* GetInterruptRelayQueue(u32 thread_id) {
    return (InterruptRelayQueue*)Kernel::GetSharedMemoryPointer(g_shared_memory,
        sizeof(InterruptRelayQueue) * thread_id);
}

void WriteHWRegs(u32 base_address, u32 size_in_bytes, const u32* data) {
    // TODO: Return proper error codes
    if (base_address + size_in_bytes >= 0x420000) {
        ERROR_LOG(GPU, "Write address out of range! (address=0x%08x, size=0x%08x)",
                  base_address, size_in_bytes);
        return;
    }

    // size should be word-aligned
    if ((size_in_bytes % 4) != 0) {
        ERROR_LOG(GPU, "Invalid size 0x%08x", size_in_bytes);
        return;
    }

    while (size_in_bytes > 0) {
        GPU::Write<u32>(base_address + 0x1EB00000, *data);

        size_in_bytes -= 4;
        ++data;
        base_address += 4;
    }
}

/// Write a GSP GPU hardware register
void WriteHWRegs(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    u32* src = (u32*)Memory::GetPointer(cmd_buff[0x4]);

    WriteHWRegs(reg_addr, size, src);
}

/// Read a GSP GPU hardware register
void ReadHWRegs(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    // TODO: Return proper error codes
    if (reg_addr + size >= 0x420000) {
        ERROR_LOG(GPU, "Read address out of range! (address=0x%08x, size=0x%08x)", reg_addr, size);
        return;
    }

    // size should be word-aligned
    if ((size % 4) != 0) {
        ERROR_LOG(GPU, "Invalid size 0x%08x", size);
        return;
    }

    u32* dst = (u32*)Memory::GetPointer(cmd_buff[0x41]);

    while (size > 0) {
        GPU::Read<u32>(*dst, reg_addr + 0x1EB00000);

        size -= 4;
        ++dst;
        reg_addr += 4;
    }
}

void SetBufferSwap(u32 screen_id, const FrameBufferInfo& info) {
    u32 base_address = 0x400000;
    if (info.active_fb == 0) {
        WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].address_left1), 4, &info.address_left);
        WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].address_right1), 4, &info.address_right);
    } else {
        WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].address_left2), 4, &info.address_left);
        WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].address_right2), 4, &info.address_right);
    }
    WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].stride), 4, &info.stride);
    WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].color_format), 4, &info.format);
    WriteHWRegs(base_address + 4 * GPU_REG_INDEX(framebuffer_config[screen_id].active_fb), 4, &info.shown_fb);
}

/**
 * GSP_GPU::SetBufferSwap service function
 *
 * Updates GPU display framebuffer configuration using the specified parameters.
 *
 *  Inputs:
 *      1 : Screen ID (0 = top screen, 1 = bottom screen)
 *      2-7 : FrameBufferInfo structure
 *  Outputs:
 *      1: Result code
 */
void SetBufferSwap(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 screen_id = cmd_buff[1];
    FrameBufferInfo* fb_info = (FrameBufferInfo*)&cmd_buff[2];
    SetBufferSwap(screen_id, *fb_info);

    cmd_buff[1] = 0; // No error
}

/**
 * GSP_GPU::RegisterInterruptRelayQueue service function
 *  Inputs:
 *      1 : "Flags" field, purpose is unknown
 *      3 : Handle to GSP synchronization event
 *  Outputs:
 *      0 : Result of function, 0 on success, otherwise error code
 *      2 : Thread index into GSP command buffer
 *      4 : Handle to GSP shared memory
 */
void RegisterInterruptRelayQueue(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();
    u32 flags = cmd_buff[1];
    g_interrupt_event = cmd_buff[3];
    g_shared_memory = Kernel::CreateSharedMemory("GSPSharedMem");

    _assert_msg_(GSP, (g_interrupt_event != 0), "handle is not valid!");

    cmd_buff[2] = g_thread_id++; // ThreadID
    cmd_buff[4] = g_shared_memory; // GSP shared memory

    Kernel::SignalEvent(g_interrupt_event); // TODO(bunnei): Is this correct?
}

/**
 * Signals that the specified interrupt type has occurred to userland code
 * @param interrupt_id ID of interrupt that is being signalled
 * @todo This should probably take a thread_id parameter and only signal this thread?
 */
void SignalInterrupt(InterruptId interrupt_id) {
    if (0 == g_interrupt_event) {
        WARN_LOG(GSP, "cannot synchronize until GSP event has been created!");
        return;
    }
    if (0 == g_shared_memory) {
        WARN_LOG(GSP, "cannot synchronize until GSP shared memory has been created!");
        return;
    }
    for (int thread_id = 0; thread_id < 0x4; ++thread_id) {
        InterruptRelayQueue* interrupt_relay_queue = GetInterruptRelayQueue(thread_id);
        interrupt_relay_queue->number_interrupts = interrupt_relay_queue->number_interrupts + 1;

        u8 next = interrupt_relay_queue->index;
        next += interrupt_relay_queue->number_interrupts;
        next = next % 0x34; // 0x34 is the number of interrupt slots

        interrupt_relay_queue->slot[next] = interrupt_id;
        interrupt_relay_queue->error_code = 0x0; // No error
    }
    Kernel::SignalEvent(g_interrupt_event);
}

/// Executes the next GSP command
void ExecuteCommand(const Command& command, u32 thread_id) {
    // Utility function to convert register ID to address
    auto WriteGPURegister = [](u32 id, u32 data) {
        GPU::Write<u32>(0x1EF00000 + 4 * id, data);
    };

    switch (command.id) {

    // GX request DMA - typically used for copying memory from GSP heap to VRAM
    case CommandId::REQUEST_DMA:
        memcpy(Memory::GetPointer(command.dma_request.dest_address),
               Memory::GetPointer(command.dma_request.source_address),
               command.dma_request.size);
        break;

    // ctrulib homebrew sends all relevant command list data with this command,
    // hence we do all "interesting" stuff here and do nothing in SET_COMMAND_LIST_FIRST.
    // TODO: This will need some rework in the future.
    case CommandId::SET_COMMAND_LIST_LAST:
    {
        auto& params = command.set_command_list_last;
        WriteGPURegister(GPU_REG_INDEX(command_processor_config.address), Memory::VirtualToPhysicalAddress(params.address) >> 3);
        WriteGPURegister(GPU_REG_INDEX(command_processor_config.size), params.size >> 3);

        // TODO: Not sure if we are supposed to always write this .. seems to trigger processing though
        WriteGPURegister(GPU_REG_INDEX(command_processor_config.trigger), 1);

        SignalInterrupt(InterruptId::P3D);
        break;
    }

    // It's assumed that the two "blocks" behave equivalently.
    // Presumably this is done simply to allow two memory fills to run in parallel.
    case CommandId::SET_MEMORY_FILL:
    {
        auto& params = command.memory_fill;
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[0].address_start), Memory::VirtualToPhysicalAddress(params.start1) >> 3);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[0].address_end), Memory::VirtualToPhysicalAddress(params.end1) >> 3);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[0].size), params.end1 - params.start1);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[0].value), params.value1);

        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[1].address_start), Memory::VirtualToPhysicalAddress(params.start2) >> 3);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[1].address_end), Memory::VirtualToPhysicalAddress(params.end2) >> 3);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[1].size), params.end2 - params.start2);
        WriteGPURegister(GPU_REG_INDEX(memory_fill_config[1].value), params.value2);
        break;
    }

    case CommandId::SET_DISPLAY_TRANSFER:
    {
        auto& params = command.image_copy;
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.input_address), Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.output_address), Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.input_size), params.in_buffer_size);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.output_size), params.out_buffer_size);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.flags), params.flags);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.trigger), 1);

        // TODO(bunnei): Signalling all of these interrupts here is totally wrong, but it seems to
        // work well enough for running demos. Need to figure out how these all work and trigger
        // them correctly.
        SignalInterrupt(InterruptId::PSC0);
        SignalInterrupt(InterruptId::PSC1);
        SignalInterrupt(InterruptId::PPF);
        SignalInterrupt(InterruptId::P3D);
        SignalInterrupt(InterruptId::DMA);

        // Update framebuffer information if requested
        for (int screen_id = 0; screen_id < 2; ++screen_id) {
            FrameBufferUpdate* info = GetFrameBufferInfo(thread_id, screen_id);
            if (info->is_dirty)
                SetBufferSwap(screen_id, info->framebuffer_info[info->index]);

            info->is_dirty = false;
        }
        break;
    }

    // TODO: Check if texture copies are implemented correctly..
    case CommandId::SET_TEXTURE_COPY:
    {
        auto& params = command.image_copy;
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.input_address), Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.output_address), Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.input_size), params.in_buffer_size);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.output_size), params.out_buffer_size);
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.flags), params.flags);

        // TODO: Should this register be set to 1 or should instead its value be OR-ed with 1?
        WriteGPURegister(GPU_REG_INDEX(display_transfer_config.trigger), 1);
        break;
    }

    // TODO: Figure out what exactly SET_COMMAND_LIST_FIRST and SET_COMMAND_LIST_LAST
    //       are supposed to do.
    case CommandId::SET_COMMAND_LIST_FIRST:
    {
        break;
    }

    default:
        ERROR_LOG(GSP, "unknown command 0x%08X", (int)command.id.Value());
    }
}

/// This triggers handling of the GX command written to the command buffer in shared memory.
void TriggerCmdReqQueue(Service::Interface* self) {

    // Iterate through each thread's command queue...
    for (unsigned thread_id = 0; thread_id < 0x4; ++thread_id) {
        CommandBuffer* command_buffer = (CommandBuffer*)GetCommandBuffer(thread_id);

        // Iterate through each command...
        for (unsigned i = 0; i < command_buffer->number_commands; ++i) {
            g_debugger.GXCommandProcessed((u8*)&command_buffer->commands[i]);

            // Decode and execute command
            ExecuteCommand(command_buffer->commands[i], thread_id);

            // Indicates that command has completed
            command_buffer->number_commands = command_buffer->number_commands - 1;
        }
    }
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, WriteHWRegs,                   "WriteHWRegs"},
    {0x00020084, nullptr,                       "WriteHWRegsWithMask"},
    {0x00030082, nullptr,                       "WriteHWRegRepeat"},
    {0x00040080, ReadHWRegs,                    "ReadHWRegs"},
    {0x00050200, SetBufferSwap,                 "SetBufferSwap"},
    {0x00060082, nullptr,                       "SetCommandList"},
    {0x000700C2, nullptr,                       "RequestDma"},
    {0x00080082, nullptr,                       "FlushDataCache"},
    {0x00090082, nullptr,                       "InvalidateDataCache"},
    {0x000A0044, nullptr,                       "RegisterInterruptEvents"},
    {0x000B0040, nullptr,                       "SetLcdForceBlack"},
    {0x000C0000, TriggerCmdReqQueue,            "TriggerCmdReqQueue"},
    {0x000D0140, nullptr,                       "SetDisplayTransfer"},
    {0x000E0180, nullptr,                       "SetTextureCopy"},
    {0x000F0200, nullptr,                       "SetMemoryFill"},
    {0x00100040, nullptr,                       "SetAxiConfigQoSMode"},
    {0x00110040, nullptr,                       "SetPerfLogMode"},
    {0x00120000, nullptr,                       "GetPerfLog"},
    {0x00130042, RegisterInterruptRelayQueue,   "RegisterInterruptRelayQueue"},
    {0x00140000, nullptr,                       "UnregisterInterruptRelayQueue"},
    {0x00150002, nullptr,                       "TryAcquireRight"},
    {0x00160042, nullptr,                       "AcquireRight"},
    {0x00170000, nullptr,                       "ReleaseRight"},
    {0x00180000, nullptr,                       "ImportDisplayCaptureInfo"},
    {0x00190000, nullptr,                       "SaveVramSysArea"},
    {0x001A0000, nullptr,                       "RestoreVramSysArea"},
    {0x001B0000, nullptr,                       "ResetGpuCore"},
    {0x001C0040, nullptr,                       "SetLedForceOff"},
    {0x001D0040, nullptr,                       "SetTestCommand"},
    {0x001E0080, nullptr,                       "SetInternalPriorities"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));

    g_interrupt_event = 0;
    g_shared_memory = 0;
    g_thread_id = 1;
}

Interface::~Interface() {
}

} // namespace
