// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_field.h"

#include "core/mem_map.h"
#include "core/memory.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hw/hw.h"
#include "core/hw/gpu.h"
#include "core/hw/lcd.h"

#include "video_core/gpu_debugger.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

#include "gsp_gpu.h"

// Main graphics debugger object - TODO: Here is probably not the best place for this
GraphicsDebugger g_debugger;

// Beginning address of HW regs
const static u32 REGS_BEGIN = 0x1EB00000;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace GSP_GPU

namespace GSP_GPU {

/// Event triggered when GSP interrupt has been signalled
Kernel::SharedPtr<Kernel::Event> g_interrupt_event;
/// GSP shared memoryings
Kernel::SharedPtr<Kernel::SharedMemory> g_shared_memory;
/// Thread index into interrupt relay queue
u32 g_thread_id = 0;

/// Gets a pointer to a thread command buffer in GSP shared memory
static inline u8* GetCommandBuffer(u32 thread_id) {
    return g_shared_memory->GetPointer(0x800 + (thread_id * sizeof(CommandBuffer)));
}

FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index) {
    DEBUG_ASSERT_MSG(screen_index < 2, "Invalid screen index");

    // For each thread there are two FrameBufferUpdate fields
    u32 offset = 0x200 + (2 * thread_id + screen_index) * sizeof(FrameBufferUpdate);
    u8* ptr = g_shared_memory->GetPointer(offset);
    return reinterpret_cast<FrameBufferUpdate*>(ptr);
}

/// Gets a pointer to the interrupt relay queue for a given thread index
static inline InterruptRelayQueue* GetInterruptRelayQueue(u32 thread_id) {
    u8* ptr = g_shared_memory->GetPointer(sizeof(InterruptRelayQueue) * thread_id);
    return reinterpret_cast<InterruptRelayQueue*>(ptr);
}

/**
 * Checks if the parameters in a register write call are valid and logs in the case that
 * they are not
 * @param base_address The first address in the sequence of registers that will be written
 * @param size_in_bytes The number of registers that will be written
 * @return true if the parameters are valid, false otherwise
 */
static bool CheckWriteParameters(u32 base_address, u32 size_in_bytes) {
    // TODO: Return proper error codes
    if (base_address + size_in_bytes >= 0x420000) {
        LOG_ERROR(Service_GSP, "Write address out of range! (address=0x%08x, size=0x%08x)",
                  base_address, size_in_bytes);
        return false;
    }

    // size should be word-aligned
    if ((size_in_bytes % 4) != 0) {
        LOG_ERROR(Service_GSP, "Invalid size 0x%08x", size_in_bytes);
        return false;
    }

    return true;
}

/**
 * Writes sequential GSP GPU hardware registers using an array of source data
 *
 * @param base_address The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data A pointer to the source data
 */
static void WriteHWRegs(u32 base_address, u32 size_in_bytes, const u32* data) {
    // TODO: Return proper error codes
    if (!CheckWriteParameters(base_address, size_in_bytes))
        return;

    while (size_in_bytes > 0) {
        HW::Write<u32>(base_address + REGS_BEGIN, *data);

        size_in_bytes -= 4;
        ++data;
        base_address += 4;
    }
}

/**
 * GSP_GPU::WriteHWRegs service function
 *
 * Writes sequential GSP GPU hardware registers
 *
 *  Inputs:
 *      1 : address of first GPU register
 *      2 : number of registers to write sequentially
 *      4 : pointer to source data array
 */
static void WriteHWRegs(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    u32* src = (u32*)Memory::GetPointer(cmd_buff[4]);

    WriteHWRegs(reg_addr, size, src);
}

/**
 * Updates sequential GSP GPU hardware registers using parallel arrays of source data and masks.
 * For each register, the value is updated only where the mask is high
 *
 * @param base_address The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data A pointer to the source data to use for updates
 * @param masks A pointer to the masks
 */
static void WriteHWRegsWithMask(u32 base_address, u32 size_in_bytes, const u32* data, const u32* masks) {
    // TODO: Return proper error codes
    if (!CheckWriteParameters(base_address, size_in_bytes))
        return;

    while (size_in_bytes > 0) {
        const u32 reg_address = base_address + REGS_BEGIN;

        u32 reg_value;
        HW::Read<u32>(reg_value, reg_address);

        // Update the current value of the register only for set mask bits
        reg_value = (reg_value & ~*masks) | (*data | *masks);

        HW::Write<u32>(reg_address, reg_value);

        size_in_bytes -= 4;
        ++data;
        ++masks;
        base_address += 4;
    }
}

/**
 * GSP_GPU::WriteHWRegsWithMask service function
 *
 * Updates sequential GSP GPU hardware registers using masks
 *
 *  Inputs:
 *      1 : address of first GPU register
 *      2 : number of registers to update sequentially
 *      4 : pointer to source data array
 *      6 : pointer to mask array
 */
static void WriteHWRegsWithMask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    u32* src_data = (u32*)Memory::GetPointer(cmd_buff[4]);
    u32* mask_data = (u32*)Memory::GetPointer(cmd_buff[6]);

    WriteHWRegsWithMask(reg_addr, size, src_data, mask_data);
}

/// Read a GSP GPU hardware register
static void ReadHWRegs(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 reg_addr = cmd_buff[1];
    u32 size = cmd_buff[2];

    // TODO: Return proper error codes
    if (reg_addr + size >= 0x420000) {
        LOG_ERROR(Service_GSP, "Read address out of range! (address=0x%08x, size=0x%08x)", reg_addr, size);
        return;
    }

    // size should be word-aligned
    if ((size % 4) != 0) {
        LOG_ERROR(Service_GSP, "Invalid size 0x%08x", size);
        return;
    }

    u32* dst = (u32*)Memory::GetPointer(cmd_buff[0x41]);

    while (size > 0) {
        HW::Read<u32>(*dst, reg_addr + REGS_BEGIN);

        size -= 4;
        ++dst;
        reg_addr += 4;
    }
}

void SetBufferSwap(u32 screen_id, const FrameBufferInfo& info) {
    u32 base_address = 0x400000;
    PAddr phys_address_left = Memory::VirtualToPhysicalAddress(info.address_left);
    PAddr phys_address_right = Memory::VirtualToPhysicalAddress(info.address_right);
    if (info.active_fb == 0) {
        WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].address_left1)), 4,
                &phys_address_left);
        WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].address_right1)), 4,
                &phys_address_right);
    } else {
        WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].address_left2)), 4,
                &phys_address_left);
        WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].address_right2)), 4,
                &phys_address_right);
    }
    WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].stride)), 4,
            &info.stride);
    WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].color_format)), 4,
            &info.format);
    WriteHWRegs(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].active_fb)), 4,
            &info.shown_fb);

    if (Pica::g_debug_context)
        Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::BufferSwapped, nullptr);
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
static void SetBufferSwap(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 screen_id = cmd_buff[1];
    FrameBufferInfo* fb_info = (FrameBufferInfo*)&cmd_buff[2];
    SetBufferSwap(screen_id, *fb_info);

    cmd_buff[1] = 0; // No error
}

/**
 * GSP_GPU::FlushDataCache service function
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
    u32 size    = cmd_buff[2];
    u32 process = cmd_buff[4];

    VideoCore::g_renderer->hw_rasterizer->NotifyFlush(Memory::VirtualToPhysicalAddress(address), size);

    // TODO(purpasmart96): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_DEBUG(Service_GSP, "(STUBBED) called address=0x%08X, size=0x%08X, process=0x%08X",
              address, size, process);
}

/**
 * GSP_GPU::RegisterInterruptRelayQueue service function
 *  Inputs:
 *      1 : "Flags" field, purpose is unknown
 *      3 : Handle to GSP synchronization event
 *  Outputs:
 *      1 : Result of function, 0x2A07 on success, otherwise error code
 *      2 : Thread index into GSP command buffer
 *      4 : Handle to GSP shared memory
 */
static void RegisterInterruptRelayQueue(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 flags = cmd_buff[1];

    g_interrupt_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[3]);
    ASSERT_MSG((g_interrupt_event != nullptr), "handle is not valid!");

    Handle shmem_handle = Kernel::g_handle_table.Create(g_shared_memory).MoveFrom();

    // This specific code is required for a successful initialization, rather than 0
    cmd_buff[1] = ResultCode((ErrorDescription)519, ErrorModule::GX,
                             ErrorSummary::Success, ErrorLevel::Success).raw;
    cmd_buff[2] = g_thread_id++; // Thread ID
    cmd_buff[4] = shmem_handle; // GSP shared memory

    g_interrupt_event->Signal(); // TODO(bunnei): Is this correct?
}

/**
 * Signals that the specified interrupt type has occurred to userland code
 * @param interrupt_id ID of interrupt that is being signalled
 * @todo This should probably take a thread_id parameter and only signal this thread?
 * @todo This probably does not belong in the GSP module, instead move to video_core
 */
void SignalInterrupt(InterruptId interrupt_id) {
    if (0 == g_interrupt_event) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP event has been created!");
        return;
    }
    if (nullptr == g_shared_memory) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP shared memory has been created!");
        return;
    }
    for (int thread_id = 0; thread_id < 0x4; ++thread_id) {
        InterruptRelayQueue* interrupt_relay_queue = GetInterruptRelayQueue(thread_id);
        u8 next = interrupt_relay_queue->index;
        next += interrupt_relay_queue->number_interrupts;
        next = next % 0x34; // 0x34 is the number of interrupt slots

        interrupt_relay_queue->number_interrupts += 1;

        interrupt_relay_queue->slot[next] = interrupt_id;
        interrupt_relay_queue->error_code = 0x0; // No error

        // Update framebuffer information if requested
        // TODO(yuriks): Confirm where this code should be called. It is definitely updated without
        //               executing any GSP commands, only waiting on the event.
        int screen_id = (interrupt_id == InterruptId::PDC0) ? 0 : (interrupt_id == InterruptId::PDC1) ? 1 : -1;
        if (screen_id != -1) {
            FrameBufferUpdate* info = GetFrameBufferInfo(thread_id, screen_id);
            if (info->is_dirty) {
                SetBufferSwap(screen_id, info->framebuffer_info[info->index]);
                info->is_dirty = false;
            }
        }
    }
    g_interrupt_event->Signal();
}

/// Executes the next GSP command
static void ExecuteCommand(const Command& command, u32 thread_id) {
    // Utility function to convert register ID to address
    static auto WriteGPURegister = [](u32 id, u32 data) {
        GPU::Write<u32>(0x1EF00000 + 4 * id, data);
    };

    switch (command.id) {

    // GX request DMA - typically used for copying memory from GSP heap to VRAM
    case CommandId::REQUEST_DMA:
        VideoCore::g_renderer->hw_rasterizer->NotifyPreRead(Memory::VirtualToPhysicalAddress(command.dma_request.source_address),
                                                            command.dma_request.size);

        memcpy(Memory::GetPointer(command.dma_request.dest_address),
               Memory::GetPointer(command.dma_request.source_address),
               command.dma_request.size);
        SignalInterrupt(InterruptId::DMA);

        VideoCore::g_renderer->hw_rasterizer->NotifyFlush(Memory::VirtualToPhysicalAddress(command.dma_request.dest_address),
                                                          command.dma_request.size);
        break;

    // ctrulib homebrew sends all relevant command list data with this command,
    // hence we do all "interesting" stuff here and do nothing in SET_COMMAND_LIST_FIRST.
    // TODO: This will need some rework in the future.
    case CommandId::SET_COMMAND_LIST_LAST:
    {
        auto& params = command.set_command_list_last;

        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.address)),
                Memory::VirtualToPhysicalAddress(params.address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.size)), params.size);

        // TODO: Not sure if we are supposed to always write this .. seems to trigger processing though
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.trigger)), 1);

        break;
    }

    // It's assumed that the two "blocks" behave equivalently.
    // Presumably this is done simply to allow two memory fills to run in parallel.
    case CommandId::SET_MEMORY_FILL:
    {
        auto& params = command.memory_fill;

        if (params.start1 != 0) {
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].address_start)),
                    Memory::VirtualToPhysicalAddress(params.start1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].address_end)),
                    Memory::VirtualToPhysicalAddress(params.end1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].value_32bit)), params.value1);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].control)), params.control1);
        }

        if (params.start2 != 0) {
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_start)),
                    Memory::VirtualToPhysicalAddress(params.start2) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_end)),
                    Memory::VirtualToPhysicalAddress(params.end2) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].value_32bit)), params.value2);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].control)), params.control2);
        }
        break;
    }

    case CommandId::SET_DISPLAY_TRANSFER:
    {
        auto& params = command.image_copy;
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_address)),
                Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_address)),
                Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_size)), params.in_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_size)), params.out_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.flags)), params.flags);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.trigger)), 1);
        break;
    }

    // TODO: Check if texture copies are implemented correctly..
    case CommandId::SET_TEXTURE_COPY:
    {
        auto& params = command.image_copy;
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_address)),
                Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_address)),
                Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_size)), params.in_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_size)), params.out_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.flags)), params.flags);

        // TODO: Should this register be set to 1 or should instead its value be OR-ed with 1?
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.trigger)), 1);
        break;
    }

    // TODO: Figure out what exactly SET_COMMAND_LIST_FIRST and SET_COMMAND_LIST_LAST
    //       are supposed to do.
    case CommandId::SET_COMMAND_LIST_FIRST:
    {
        break;
    }

    default:
        LOG_ERROR(Service_GSP, "unknown command 0x%08X", (int)command.id.Value());
    }

    if (Pica::g_debug_context)
        Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::GSPCommandProcessed, (void*)&command);
}

/**
 * GSP_GPU::SetLcdForceBlack service function
 *
 * Enable or disable REG_LCDCOLORFILL with the color black.
 *
 *  Inputs:
 *      1: Black color fill flag (0 = don't fill, !0 = fill)
 *  Outputs:
 *      1: Result code
 */
static void SetLcdForceBlack(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    bool enable_black = cmd_buff[1] != 0;
    LCD::Regs::ColorFill data = {0};

    // Since data is already zeroed, there is no need to explicitly set
    // the color to black (all zero).
    data.is_enabled = enable_black;

    LCD::Write(HW::VADDR_LCD + 4 * LCD_REG_INDEX(color_fill_top), data.raw); // Top LCD
    LCD::Write(HW::VADDR_LCD + 4 * LCD_REG_INDEX(color_fill_bottom), data.raw); // Bottom LCD

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

/// This triggers handling of the GX command written to the command buffer in shared memory.
static void TriggerCmdReqQueue(Service::Interface* self) {
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

    u32* cmd_buff = Kernel::GetCommandBuffer();
    cmd_buff[1] = 0; // No error
}

/**
 * GSP_GPU::ImportDisplayCaptureInfo service function
 *
 * Returns information about the current framebuffer state
 *
 *  Inputs:
 *      0: Header 0x00180000
 *  Outputs:
 *      1: Result code
 *      2: Left framebuffer virtual address for the main screen
 *      3: Right framebuffer virtual address for the main screen
 *      4: Main screen framebuffer format
 *      5: Main screen framebuffer width
 *      6: Left framebuffer virtual address for the bottom screen
 *      7: Right framebuffer virtual address for the bottom screen
 *      8: Bottom screen framebuffer format
 *      9: Bottom screen framebuffer width
 */
static void ImportDisplayCaptureInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(Subv): We're always returning the framebuffer structures for thread_id = 0,
    // because we only support a single running application at a time.
    // This should always return the framebuffer data that is currently displayed on the screen.

    u32 thread_id = 0;

    FrameBufferUpdate* top_screen = GetFrameBufferInfo(thread_id, 0);
    FrameBufferUpdate* bottom_screen = GetFrameBufferInfo(thread_id, 1);

    cmd_buff[2] = top_screen->framebuffer_info[top_screen->index].address_left;
    cmd_buff[3] = top_screen->framebuffer_info[top_screen->index].address_right;
    cmd_buff[4] = top_screen->framebuffer_info[top_screen->index].format;
    cmd_buff[5] = top_screen->framebuffer_info[top_screen->index].stride;

    cmd_buff[6] = bottom_screen->framebuffer_info[bottom_screen->index].address_left;
    cmd_buff[7] = bottom_screen->framebuffer_info[bottom_screen->index].address_right;
    cmd_buff[8] = bottom_screen->framebuffer_info[bottom_screen->index].format;
    cmd_buff[9] = bottom_screen->framebuffer_info[bottom_screen->index].stride;

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_GSP, "called");
}


const Interface::FunctionInfo FunctionTable[] = {
    {0x00010082, WriteHWRegs,                   "WriteHWRegs"},
    {0x00020084, WriteHWRegsWithMask,           "WriteHWRegsWithMask"},
    {0x00030082, nullptr,                       "WriteHWRegRepeat"},
    {0x00040080, ReadHWRegs,                    "ReadHWRegs"},
    {0x00050200, SetBufferSwap,                 "SetBufferSwap"},
    {0x00060082, nullptr,                       "SetCommandList"},
    {0x000700C2, nullptr,                       "RequestDma"},
    {0x00080082, FlushDataCache,                "FlushDataCache"},
    {0x00090082, nullptr,                       "InvalidateDataCache"},
    {0x000A0044, nullptr,                       "RegisterInterruptEvents"},
    {0x000B0040, SetLcdForceBlack,              "SetLcdForceBlack"},
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
    {0x00180000, ImportDisplayCaptureInfo,      "ImportDisplayCaptureInfo"},
    {0x00190000, nullptr,                       "SaveVramSysArea"},
    {0x001A0000, nullptr,                       "RestoreVramSysArea"},
    {0x001B0000, nullptr,                       "ResetGpuCore"},
    {0x001C0040, nullptr,                       "SetLedForceOff"},
    {0x001D0040, nullptr,                       "SetTestCommand"},
    {0x001E0080, nullptr,                       "SetInternalPriorities"},
    {0x001F0082, nullptr,                       "StoreDataCache"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);

    g_interrupt_event = nullptr;

    using Kernel::MemoryPermission;
    g_shared_memory = Kernel::SharedMemory::Create(0x1000, MemoryPermission::ReadWrite,
            MemoryPermission::ReadWrite, "GSPSharedMem");

    g_thread_id = 0;
}

Interface::~Interface() {
    g_interrupt_event = nullptr;
    g_shared_memory = nullptr;
}

} // namespace
