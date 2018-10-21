// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "common/bit_field.h"
#include "common/microprofile.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu_debugger.h"

// Main graphics debugger object - TODO: Here is probably not the best place for this
GraphicsDebugger g_debugger;

namespace Service::GSP {

// Beginning address of HW regs
const u32 REGS_BEGIN = 0x1EB00000;

namespace ErrCodes {
enum {
    // TODO(purpasmart): Check if this name fits its actual usage
    OutofRangeOrMisalignedAddress = 513,
    FirstInitialization = 519,
};
}

constexpr ResultCode RESULT_FIRST_INITIALIZATION(ErrCodes::FirstInitialization, ErrorModule::GX,
                                                 ErrorSummary::Success, ErrorLevel::Success);
constexpr ResultCode ERR_REGS_OUTOFRANGE_OR_MISALIGNED(ErrCodes::OutofRangeOrMisalignedAddress,
                                                       ErrorModule::GX,
                                                       ErrorSummary::InvalidArgument,
                                                       ErrorLevel::Usage); // 0xE0E02A01
constexpr ResultCode ERR_REGS_MISALIGNED(ErrorDescription::MisalignedSize, ErrorModule::GX,
                                         ErrorSummary::InvalidArgument,
                                         ErrorLevel::Usage); // 0xE0E02BF2
constexpr ResultCode ERR_REGS_INVALID_SIZE(ErrorDescription::InvalidSize, ErrorModule::GX,
                                           ErrorSummary::InvalidArgument,
                                           ErrorLevel::Usage); // 0xE0E02BEC

/// Maximum number of threads that can be registered at the same time in the GSP module.
constexpr u32 MaxGSPThreads = 4;

/// Thread ids currently in use by the sessions connected to the GSPGPU service.
static std::array<bool, MaxGSPThreads> used_thread_ids = {false, false, false, false};

static u32 GetUnusedThreadId() {
    for (u32 id = 0; id < MaxGSPThreads; ++id) {
        if (!used_thread_ids[id])
            return id;
    }
    ASSERT_MSG(false, "All GSP threads are in use");
}

/// Gets a pointer to a thread command buffer in GSP shared memory
static inline u8* GetCommandBuffer(Kernel::SharedPtr<Kernel::SharedMemory> shared_memory,
                                   u32 thread_id) {
    return shared_memory->GetPointer(0x800 + (thread_id * sizeof(CommandBuffer)));
}

FrameBufferUpdate* GSP_GPU::GetFrameBufferInfo(u32 thread_id, u32 screen_index) {
    DEBUG_ASSERT_MSG(screen_index < 2, "Invalid screen index");

    // For each thread there are two FrameBufferUpdate fields
    u32 offset = 0x200 + (2 * thread_id + screen_index) * sizeof(FrameBufferUpdate);
    u8* ptr = shared_memory->GetPointer(offset);
    return reinterpret_cast<FrameBufferUpdate*>(ptr);
}

/// Gets a pointer to the interrupt relay queue for a given thread index
static inline InterruptRelayQueue* GetInterruptRelayQueue(
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory, u32 thread_id) {
    u8* ptr = shared_memory->GetPointer(sizeof(InterruptRelayQueue) * thread_id);
    return reinterpret_cast<InterruptRelayQueue*>(ptr);
}

void GSP_GPU::ClientDisconnected(Kernel::SharedPtr<Kernel::ServerSession> server_session) {
    SessionData* session_data = GetSessionData(server_session);
    if (active_thread_id == session_data->thread_id)
        ReleaseRight(session_data);
    SessionRequestHandler::ClientDisconnected(server_session);
}

/**
 * Writes a single GSP GPU hardware registers with a single u32 value
 * (For internal use.)
 *
 * @param base_address The address of the register in question
 * @param data Data to be written
 */
static void WriteSingleHWReg(u32 base_address, u32 data) {
    DEBUG_ASSERT_MSG((base_address & 3) == 0 && base_address < 0x420000,
                     "Write address out of range or misaligned");
    HW::Write<u32>(base_address + REGS_BEGIN, data);
}

/**
 * Writes sequential GSP GPU hardware registers using an array of source data
 *
 * @param base_address The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data A vector containing the source data
 * @return RESULT_SUCCESS if the parameters are valid, error code otherwise
 */
static ResultCode WriteHWRegs(u32 base_address, u32 size_in_bytes, const std::vector<u8>& data) {
    // This magic number is verified to be done by the gsp module
    const u32 max_size_in_bytes = 0x80;

    if (base_address & 3 || base_address >= 0x420000) {
        LOG_ERROR(Service_GSP,
                  "Write address was out of range or misaligned! (address=0x{:08x}, size=0x{:08x})",
                  base_address, size_in_bytes);
        return ERR_REGS_OUTOFRANGE_OR_MISALIGNED;
    } else if (size_in_bytes <= max_size_in_bytes) {
        if (size_in_bytes & 3) {
            LOG_ERROR(Service_GSP, "Misaligned size 0x{:08x}", size_in_bytes);
            return ERR_REGS_MISALIGNED;
        } else {
            std::size_t offset = 0;
            while (size_in_bytes > 0) {
                u32 value;
                std::memcpy(&value, &data[offset], sizeof(u32));
                WriteSingleHWReg(base_address, value);

                size_in_bytes -= 4;
                offset += 4;
                base_address += 4;
            }
            return RESULT_SUCCESS;
        }

    } else {
        LOG_ERROR(Service_GSP, "Out of range size 0x{:08x}", size_in_bytes);
        return ERR_REGS_INVALID_SIZE;
    }
}

/**
 * Updates sequential GSP GPU hardware registers using parallel arrays of source data and masks.
 * For each register, the value is updated only where the mask is high
 *
 * @param base_address  The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data    A vector containing the data to write
 * @param masks   A vector containing the masks
 * @return RESULT_SUCCESS if the parameters are valid, error code otherwise
 */
static ResultCode WriteHWRegsWithMask(u32 base_address, u32 size_in_bytes,
                                      const std::vector<u8>& data, const std::vector<u8>& masks) {
    // This magic number is verified to be done by the gsp module
    const u32 max_size_in_bytes = 0x80;

    if (base_address & 3 || base_address >= 0x420000) {
        LOG_ERROR(Service_GSP,
                  "Write address was out of range or misaligned! (address=0x{:08x}, size=0x{:08x})",
                  base_address, size_in_bytes);
        return ERR_REGS_OUTOFRANGE_OR_MISALIGNED;
    } else if (size_in_bytes <= max_size_in_bytes) {
        if (size_in_bytes & 3) {
            LOG_ERROR(Service_GSP, "Misaligned size 0x{:08x}", size_in_bytes);
            return ERR_REGS_MISALIGNED;
        } else {
            std::size_t offset = 0;
            while (size_in_bytes > 0) {
                const u32 reg_address = base_address + REGS_BEGIN;

                u32 reg_value;
                HW::Read<u32>(reg_value, reg_address);

                u32 value, mask;
                std::memcpy(&value, &data[offset], sizeof(u32));
                std::memcpy(&mask, &masks[offset], sizeof(u32));

                // Update the current value of the register only for set mask bits
                reg_value = (reg_value & ~mask) | (value & mask);

                WriteSingleHWReg(base_address, reg_value);

                size_in_bytes -= 4;
                offset += 4;
                base_address += 4;
            }
            return RESULT_SUCCESS;
        }

    } else {
        LOG_ERROR(Service_GSP, "Out of range size 0x{:08x}", size_in_bytes);
        return ERR_REGS_INVALID_SIZE;
    }
}

void GSP_GPU::WriteHWRegs(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 2, 2);
    u32 reg_addr = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    std::vector<u8> src_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegs(reg_addr, size, src_data));
}

void GSP_GPU::WriteHWRegsWithMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2, 2, 4);
    u32 reg_addr = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();

    std::vector<u8> src_data = rp.PopStaticBuffer();
    std::vector<u8> mask_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegsWithMask(reg_addr, size, src_data, mask_data));
}

void GSP_GPU::ReadHWRegs(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x4, 2, 0);
    u32 reg_addr = rp.Pop<u32>();
    u32 input_size = rp.Pop<u32>();

    static constexpr u32 MaxReadSize = 0x80;
    u32 size = std::min(input_size, MaxReadSize);

    if ((reg_addr % 4) != 0 || reg_addr >= 0x420000) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERR_REGS_OUTOFRANGE_OR_MISALIGNED);
        LOG_ERROR(Service_GSP, "Invalid address 0x{:08x}", reg_addr);
        return;
    }

    // size should be word-aligned
    if ((size % 4) != 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERR_REGS_MISALIGNED);
        LOG_ERROR(Service_GSP, "Invalid size 0x{:08x}", size);
        return;
    }

    std::vector<u8> buffer(size);
    for (u32 offset = 0; offset < size; ++offset) {
        HW::Read<u8>(buffer[offset], REGS_BEGIN + reg_addr + offset);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushStaticBuffer(std::move(buffer), 0);
}

ResultCode SetBufferSwap(u32 screen_id, const FrameBufferInfo& info) {
    u32 base_address = 0x400000;
    PAddr phys_address_left = Memory::VirtualToPhysicalAddress(info.address_left);
    PAddr phys_address_right = Memory::VirtualToPhysicalAddress(info.address_right);
    if (info.active_fb == 0) {
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(
                                                framebuffer_config[screen_id].address_left1)),
                         phys_address_left);
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(
                                                framebuffer_config[screen_id].address_right1)),
                         phys_address_right);
    } else {
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(
                                                framebuffer_config[screen_id].address_left2)),
                         phys_address_left);
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(
                                                framebuffer_config[screen_id].address_right2)),
                         phys_address_right);
    }
    WriteSingleHWReg(base_address +
                         4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].stride)),
                     info.stride);
    WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_REG_INDEX(
                                            framebuffer_config[screen_id].color_format)),
                     info.format);
    WriteSingleHWReg(
        base_address + 4 * static_cast<u32>(GPU_REG_INDEX(framebuffer_config[screen_id].active_fb)),
        info.shown_fb);

    if (Pica::g_debug_context)
        Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::BufferSwapped, nullptr);

    if (screen_id == 0) {
        MicroProfileFlip();
        Core::System::GetInstance().perf_stats.EndGameFrame();
    }

    return RESULT_SUCCESS;
}

void GSP_GPU::SetBufferSwap(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 8, 0);
    u32 screen_id = rp.Pop<u32>();
    auto fb_info = rp.PopRaw<FrameBufferInfo>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::SetBufferSwap(screen_id, fb_info));
}

void GSP_GPU::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x8, 2, 2);
    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 2, 2);
    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetAxiConfigQoSMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 1, 0);
    u32 mode = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called mode=0x{:08X}", mode);
}

void GSP_GPU::RegisterInterruptRelayQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 1, 2);
    u32 flags = rp.Pop<u32>();

    auto interrupt_event = rp.PopObject<Kernel::Event>();
    // TODO(mailwl): return right error code instead assert
    ASSERT_MSG((interrupt_event != nullptr), "handle is not valid!");

    interrupt_event->SetName("GSP_GSP_GPU::interrupt_event");

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->interrupt_event = std::move(interrupt_event);
    session_data->registered = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    if (first_initialization) {
        // This specific code is required for a successful initialization, rather than 0
        first_initialization = false;
        rb.Push(RESULT_FIRST_INITIALIZATION);
    } else {
        rb.Push(RESULT_SUCCESS);
    }

    rb.Push(session_data->thread_id);
    rb.PushCopyObjects(shared_memory);

    LOG_DEBUG(Service_GSP, "called, flags=0x{:08X}", flags);
}

void GSP_GPU::UnregisterInterruptRelayQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 0, 0);

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->interrupt_event = nullptr;
    session_data->registered = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "called");
}

void GSP_GPU::SignalInterruptForThread(InterruptId interrupt_id, u32 thread_id) {
    SessionData* session_data = FindRegisteredThreadData(thread_id);
    if (session_data == nullptr)
        return;

    auto interrupt_event = session_data->interrupt_event;
    if (interrupt_event == nullptr) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP event has been created!");
        return;
    }
    InterruptRelayQueue* interrupt_relay_queue = GetInterruptRelayQueue(shared_memory, thread_id);
    u8 next = interrupt_relay_queue->index;
    next += interrupt_relay_queue->number_interrupts;
    next = next % 0x34; // 0x34 is the number of interrupt slots

    interrupt_relay_queue->number_interrupts += 1;

    interrupt_relay_queue->slot[next] = interrupt_id;
    interrupt_relay_queue->error_code = 0x0; // No error

    // Update framebuffer information if requested
    // TODO(yuriks): Confirm where this code should be called. It is definitely updated without
    //               executing any GSP commands, only waiting on the event.
    // TODO(Subv): The real GSP module triggers PDC0 after updating both the top and bottom
    // screen, it is currently unknown what PDC1 does.
    int screen_id =
        (interrupt_id == InterruptId::PDC0) ? 0 : (interrupt_id == InterruptId::PDC1) ? 1 : -1;
    if (screen_id != -1) {
        FrameBufferUpdate* info = GetFrameBufferInfo(thread_id, screen_id);
        if (info->is_dirty) {
            GSP::SetBufferSwap(screen_id, info->framebuffer_info[info->index]);
            info->is_dirty.Assign(false);
        }
    }
    interrupt_event->Signal();
}

/**
 * Signals that the specified interrupt type has occurred to userland code
 * @param interrupt_id ID of interrupt that is being signalled
 * @todo This should probably take a thread_id parameter and only signal this thread?
 * @todo This probably does not belong in the GSP module, instead move to video_core
 */
void GSP_GPU::SignalInterrupt(InterruptId interrupt_id) {
    if (nullptr == shared_memory) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP shared memory has been created!");
        return;
    }

    // The PDC0 and PDC1 interrupts are fired even if the GPU right hasn't been acquired.
    // Normal interrupts are only signaled for the active thread (ie, the thread that has the GPU
    // right), but the PDC0/1 interrupts are signaled for every registered thread.
    if (interrupt_id == InterruptId::PDC0 || interrupt_id == InterruptId::PDC1) {
        for (u32 thread_id = 0; thread_id < MaxGSPThreads; ++thread_id) {
            SignalInterruptForThread(interrupt_id, thread_id);
        }
        return;
    }

    // For normal interrupts, don't do anything if no process has acquired the GPU right.
    if (active_thread_id == -1)
        return;

    SignalInterruptForThread(interrupt_id, active_thread_id);
}

MICROPROFILE_DEFINE(GPU_GSP_DMA, "GPU", "GSP DMA", MP_RGB(100, 0, 255));

/// Executes the next GSP command
static void ExecuteCommand(const Command& command, u32 thread_id) {
    // Utility function to convert register ID to address
    static auto WriteGPURegister = [](u32 id, u32 data) {
        GPU::Write<u32>(0x1EF00000 + 4 * id, data);
    };

    switch (command.id) {

    // GX request DMA - typically used for copying memory from GSP heap to VRAM
    case CommandId::REQUEST_DMA: {
        MICROPROFILE_SCOPE(GPU_GSP_DMA);

        // TODO: Consider attempting rasterizer-accelerated surface blit if that usage is ever
        // possible/likely
        Memory::RasterizerFlushVirtualRegion(command.dma_request.source_address,
                                             command.dma_request.size, Memory::FlushMode::Flush);
        Memory::RasterizerFlushVirtualRegion(command.dma_request.dest_address,
                                             command.dma_request.size,
                                             Memory::FlushMode::Invalidate);

        // TODO(Subv): These memory accesses should not go through the application's memory mapping.
        // They should go through the GSP module's memory mapping.
        Memory::CopyBlock(command.dma_request.dest_address, command.dma_request.source_address,
                          command.dma_request.size);
        SignalInterrupt(InterruptId::DMA);
        break;
    }
    // TODO: This will need some rework in the future. (why?)
    case CommandId::SUBMIT_GPU_CMDLIST: {
        auto& params = command.submit_gpu_cmdlist;

        if (params.do_flush) {
            // This flag flushes the command list (params.address, params.size) from the cache.
            // Command lists are not processed by the hardware renderer, so we don't need to
            // actually flush them in Citra.
        }

        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.address)),
                         Memory::VirtualToPhysicalAddress(params.address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.size)),
                         params.size);

        // TODO: Not sure if we are supposed to always write this .. seems to trigger processing
        // though
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(command_processor_config.trigger)), 1);

        // TODO(yuriks): Figure out the meaning of the `flags` field.

        break;
    }

    // It's assumed that the two "blocks" behave equivalently.
    // Presumably this is done simply to allow two memory fills to run in parallel.
    case CommandId::SET_MEMORY_FILL: {
        auto& params = command.memory_fill;

        if (params.start1 != 0) {
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].address_start)),
                             Memory::VirtualToPhysicalAddress(params.start1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].address_end)),
                             Memory::VirtualToPhysicalAddress(params.end1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].value_32bit)),
                             params.value1);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].control)),
                             params.control1);
        }

        if (params.start2 != 0) {
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_start)),
                             Memory::VirtualToPhysicalAddress(params.start2) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_end)),
                             Memory::VirtualToPhysicalAddress(params.end2) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].value_32bit)),
                             params.value2);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].control)),
                             params.control2);
        }
        break;
    }

    case CommandId::SET_DISPLAY_TRANSFER: {
        auto& params = command.display_transfer;
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_address)),
                         Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_address)),
                         Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.input_size)),
                         params.in_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_size)),
                         params.out_buffer_size);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.flags)),
                         params.flags);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.trigger)), 1);
        break;
    }

    case CommandId::SET_TEXTURE_COPY: {
        auto& params = command.texture_copy;
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.input_address),
                         Memory::VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.output_address),
                         Memory::VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.texture_copy.size),
                         params.size);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.texture_copy.input_size),
                         params.in_width_gap);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.texture_copy.output_size),
                         params.out_width_gap);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.flags), params.flags);

        // NOTE: Actual GSP ORs 1 with current register instead of overwriting. Doesn't seem to
        // matter.
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.trigger), 1);
        break;
    }

    case CommandId::CACHE_FLUSH: {
        // NOTE: Rasterizer flushing handled elsewhere in CPU read/write and other GPU handlers
        // Use command.cache_flush.regions to implement this handler
        break;
    }

    default:
        LOG_ERROR(Service_GSP, "unknown command 0x{:08X}", (int)command.id.Value());
    }

    if (Pica::g_debug_context)
        Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::GSPCommandProcessed,
                                       (void*)&command);
}

void GSP_GPU::SetLcdForceBlack(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xB, 1, 0);

    bool enable_black = rp.Pop<bool>();
    LCD::Regs::ColorFill data = {0};

    // Since data is already zeroed, there is no need to explicitly set
    // the color to black (all zero).
    data.is_enabled.Assign(enable_black);

    LCD::Write(HW::VADDR_LCD + 4 * LCD_REG_INDEX(color_fill_top), data.raw);    // Top LCD
    LCD::Write(HW::VADDR_LCD + 4 * LCD_REG_INDEX(color_fill_bottom), data.raw); // Bottom LCD

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void GSP_GPU::TriggerCmdReqQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xC, 0, 0);

    // Iterate through each thread's command queue...
    for (unsigned thread_id = 0; thread_id < 0x4; ++thread_id) {
        CommandBuffer* command_buffer = (CommandBuffer*)GetCommandBuffer(shared_memory, thread_id);

        // Iterate through each command...
        for (unsigned i = 0; i < command_buffer->number_commands; ++i) {
            g_debugger.GXCommandProcessed((u8*)&command_buffer->commands[i]);

            // Decode and execute command
            ExecuteCommand(command_buffer->commands[i], thread_id);

            // Indicates that command has completed
            command_buffer->number_commands.Assign(command_buffer->number_commands - 1);
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void GSP_GPU::ImportDisplayCaptureInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x18, 0, 0);

    // TODO(Subv): We're always returning the framebuffer structures for thread_id = 0,
    // because we only support a single running application at a time.
    // This should always return the framebuffer data that is currently displayed on the screen.

    u32 thread_id = 0;

    FrameBufferUpdate* top_screen = GetFrameBufferInfo(thread_id, 0);
    FrameBufferUpdate* bottom_screen = GetFrameBufferInfo(thread_id, 1);

    struct CaptureInfoEntry {
        u32_le address_left;
        u32_le address_right;
        u32_le format;
        u32_le stride;
    };

    CaptureInfoEntry top_entry, bottom_entry;
    // Top Screen
    top_entry.address_left = top_screen->framebuffer_info[top_screen->index].address_left;
    top_entry.address_right = top_screen->framebuffer_info[top_screen->index].address_right;
    top_entry.format = top_screen->framebuffer_info[top_screen->index].format;
    top_entry.stride = top_screen->framebuffer_info[top_screen->index].stride;
    // Bottom Screen
    bottom_entry.address_left = bottom_screen->framebuffer_info[bottom_screen->index].address_left;
    bottom_entry.address_right =
        bottom_screen->framebuffer_info[bottom_screen->index].address_right;
    bottom_entry.format = bottom_screen->framebuffer_info[bottom_screen->index].format;
    bottom_entry.stride = bottom_screen->framebuffer_info[bottom_screen->index].stride;

    IPC::RequestBuilder rb = rp.MakeBuilder(9, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(top_entry);
    rb.PushRaw(bottom_entry);

    LOG_WARNING(Service_GSP, "called");
}

void GSP_GPU::AcquireRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 1, 2);

    u32 flag = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    SessionData* session_data = GetSessionData(ctx.Session());

    LOG_WARNING(Service_GSP, "called flag={:08X} process={} thread_id={}", flag,
                process->process_id, session_data->thread_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (active_thread_id == session_data->thread_id) {
        rb.Push(ResultCode(ErrorDescription::AlreadyDone, ErrorModule::GX, ErrorSummary::Success,
                           ErrorLevel::Success));
        return;
    }

    // TODO(Subv): This case should put the caller thread to sleep until the right is released.
    ASSERT_MSG(active_thread_id == -1, "GPU right has already been acquired");

    active_thread_id = session_data->thread_id;

    rb.Push(RESULT_SUCCESS);
}

void GSP_GPU::ReleaseRight(SessionData* session_data) {
    ASSERT_MSG(active_thread_id == session_data->thread_id,
               "Wrong thread tried to release GPU right");
    active_thread_id = -1;
}

void GSP_GPU::ReleaseRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 0, 0);

    SessionData* session_data = GetSessionData(ctx.Session());
    ReleaseRight(session_data);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_GSP, "called");
}

void GSP_GPU::StoreDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 2, 2);

    u32 address = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetLedForceOff(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1C, 1, 0);

    u8 state = rp.Pop<u8>();

    system.GetSharedPageHandler()->Set3DLed(state);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_GSP, "(STUBBED) called");
}

SessionData* GSP_GPU::FindRegisteredThreadData(u32 thread_id) {
    for (auto& session_info : connected_sessions) {
        SessionData* data = static_cast<SessionData*>(session_info.data.get());
        if (!data->registered)
            continue;
        if (data->thread_id == thread_id)
            return data;
    }
    return nullptr;
}

GSP_GPU::GSP_GPU(Core::System& system) : ServiceFramework("gsp::Gpu", 2), system(system) {
    static const FunctionInfo functions[] = {
        {0x00010082, &GSP_GPU::WriteHWRegs, "WriteHWRegs"},
        {0x00020084, &GSP_GPU::WriteHWRegsWithMask, "WriteHWRegsWithMask"},
        {0x00030082, nullptr, "WriteHWRegRepeat"},
        {0x00040080, &GSP_GPU::ReadHWRegs, "ReadHWRegs"},
        {0x00050200, &GSP_GPU::SetBufferSwap, "SetBufferSwap"},
        {0x00060082, nullptr, "SetCommandList"},
        {0x000700C2, nullptr, "RequestDma"},
        {0x00080082, &GSP_GPU::FlushDataCache, "FlushDataCache"},
        {0x00090082, &GSP_GPU::InvalidateDataCache, "InvalidateDataCache"},
        {0x000A0044, nullptr, "RegisterInterruptEvents"},
        {0x000B0040, &GSP_GPU::SetLcdForceBlack, "SetLcdForceBlack"},
        {0x000C0000, &GSP_GPU::TriggerCmdReqQueue, "TriggerCmdReqQueue"},
        {0x000D0140, nullptr, "SetDisplayTransfer"},
        {0x000E0180, nullptr, "SetTextureCopy"},
        {0x000F0200, nullptr, "SetMemoryFill"},
        {0x00100040, &GSP_GPU::SetAxiConfigQoSMode, "SetAxiConfigQoSMode"},
        {0x00110040, nullptr, "SetPerfLogMode"},
        {0x00120000, nullptr, "GetPerfLog"},
        {0x00130042, &GSP_GPU::RegisterInterruptRelayQueue, "RegisterInterruptRelayQueue"},
        {0x00140000, &GSP_GPU::UnregisterInterruptRelayQueue, "UnregisterInterruptRelayQueue"},
        {0x00150002, nullptr, "TryAcquireRight"},
        {0x00160042, &GSP_GPU::AcquireRight, "AcquireRight"},
        {0x00170000, &GSP_GPU::ReleaseRight, "ReleaseRight"},
        {0x00180000, &GSP_GPU::ImportDisplayCaptureInfo, "ImportDisplayCaptureInfo"},
        {0x00190000, nullptr, "SaveVramSysArea"},
        {0x001A0000, nullptr, "RestoreVramSysArea"},
        {0x001B0000, nullptr, "ResetGpuCore"},
        {0x001C0040, &GSP_GPU::SetLedForceOff, "SetLedForceOff"},
        {0x001D0040, nullptr, "SetTestCommand"},
        {0x001E0080, nullptr, "SetInternalPriorities"},
        {0x001F0082, &GSP_GPU::StoreDataCache, "StoreDataCache"},
    };
    RegisterHandlers(functions);

    using Kernel::MemoryPermission;
    shared_memory = system.Kernel().CreateSharedMemory(
        nullptr, 0x1000, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite, 0,
        Kernel::MemoryRegion::BASE, "GSP:SharedMemory");

    first_initialization = true;
};

SessionData::SessionData() {
    // Assign a new thread id to this session when it connects. Note: In the real GSP service this
    // is done through a real thread (svcCreateThread) but we have to simulate it since our HLE
    // services don't have threads.
    thread_id = GetUnusedThreadId();
    used_thread_ids[thread_id] = true;
}

SessionData::~SessionData() {
    // Free the thread id slot so that other sessions can use it.
    used_thread_ids[thread_id] = false;
}

} // namespace Service::GSP
