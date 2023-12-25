// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <vector>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/archives.h"
#include "common/bit_field.h"
#include "common/microprofile.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/plugin_3gx.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/memory.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu_debugger.h"

SERIALIZE_EXPORT_IMPL(Service::GSP::SessionData)
SERIALIZE_EXPORT_IMPL(Service::GSP::GSP_GPU)
SERVICE_CONSTRUCT_IMPL(Service::GSP::GSP_GPU)

// Main graphics debugger object - TODO: Here is probably not the best place for this
GraphicsDebugger g_debugger;

namespace Service::GSP {

// Beginning address of HW regs
constexpr u32 REGS_BEGIN = 0x1EB00000;

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

static PAddr VirtualToPhysicalAddress(VAddr addr) {
    if (addr == 0) {
        return 0;
    }

    // Note: the region end check is inclusive because the game can pass in an address that
    // represents an open right boundary
    if (addr >= Memory::VRAM_VADDR && addr <= Memory::VRAM_VADDR_END) {
        return addr - Memory::VRAM_VADDR + Memory::VRAM_PADDR;
    }
    if (addr >= Memory::LINEAR_HEAP_VADDR && addr <= Memory::LINEAR_HEAP_VADDR_END) {
        return addr - Memory::LINEAR_HEAP_VADDR + Memory::FCRAM_PADDR;
    }
    if (addr >= Memory::NEW_LINEAR_HEAP_VADDR && addr <= Memory::NEW_LINEAR_HEAP_VADDR_END) {
        return addr - Memory::NEW_LINEAR_HEAP_VADDR + Memory::FCRAM_PADDR;
    }
    if (addr >= Memory::PLUGIN_3GX_FB_VADDR && addr <= Memory::PLUGIN_3GX_FB_VADDR_END) {
        return addr - Memory::PLUGIN_3GX_FB_VADDR + Service::PLGLDR::PLG_LDR::GetPluginFBAddr();
    }

    LOG_ERROR(HW_Memory, "Unknown virtual address @ 0x{:08X}", addr);
    // To help with debugging, set bit on address so that it's obviously invalid.
    // TODO: find the correct way to handle this error
    return addr | 0x80000000;
}

u32 GSP_GPU::GetUnusedThreadId() const {
    for (u32 id = 0; id < MaxGSPThreads; ++id) {
        if (!used_thread_ids[id])
            return id;
    }

    UNREACHABLE_MSG("All GSP threads are in use");
    return 0;
}

/// Gets a pointer to a thread command buffer in GSP shared memory
static inline u8* GetCommandBuffer(std::shared_ptr<Kernel::SharedMemory> shared_memory,
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
    std::shared_ptr<Kernel::SharedMemory> shared_memory, u32 thread_id) {
    u8* ptr = shared_memory->GetPointer(sizeof(InterruptRelayQueue) * thread_id);
    return reinterpret_cast<InterruptRelayQueue*>(ptr);
}

void GSP_GPU::ClientDisconnected(std::shared_ptr<Kernel::ServerSession> server_session) {
    const SessionData* session_data = GetSessionData(server_session);
    if (active_thread_id == session_data->thread_id) {
        ReleaseRight(session_data);
    }
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
static ResultCode WriteHWRegs(u32 base_address, u32 size_in_bytes, std::span<const u8> data) {
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
static ResultCode WriteHWRegsWithMask(u32 base_address, u32 size_in_bytes, std::span<const u8> data,
                                      std::span<const u8> masks) {
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
    IPC::RequestParser rp(ctx);
    u32 reg_addr = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();
    std::vector<u8> src_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegs(reg_addr, size, src_data));
}

void GSP_GPU::WriteHWRegsWithMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 reg_addr = rp.Pop<u32>();
    u32 size = rp.Pop<u32>();

    std::vector<u8> src_data = rp.PopStaticBuffer();
    std::vector<u8> mask_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegsWithMask(reg_addr, size, src_data, mask_data));
}

void GSP_GPU::ReadHWRegs(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    PAddr phys_address_left = VirtualToPhysicalAddress(info.address_left);
    PAddr phys_address_right = VirtualToPhysicalAddress(info.address_right);
    if (info.active_fb == 0) {
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(
                                                screen_id, address_left1)),
                         phys_address_left);
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(
                                                screen_id, address_right1)),
                         phys_address_right);
    } else {
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(
                                                screen_id, address_left2)),
                         phys_address_left);
        WriteSingleHWReg(base_address + 4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(
                                                screen_id, address_right2)),
                         phys_address_right);
    }
    WriteSingleHWReg(base_address +
                         4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(screen_id, stride)),
                     info.stride);
    WriteSingleHWReg(base_address +
                         4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(screen_id, color_format)),
                     info.format);
    WriteSingleHWReg(base_address +
                         4 * static_cast<u32>(GPU_FRAMEBUFFER_REG_INDEX(screen_id, active_fb)),
                     info.shown_fb);

    if (Pica::g_debug_context)
        Pica::g_debug_context->OnEvent(Pica::DebugContext::Event::BufferSwapped, nullptr);

    if (screen_id == 0) {
        MicroProfileFlip();
        Core::System::GetInstance().perf_stats->EndGameFrame();
    }

    return RESULT_SUCCESS;
}

void GSP_GPU::SetBufferSwap(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 screen_id = rp.Pop<u32>();
    auto fb_info = rp.PopRaw<FrameBufferInfo>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::SetBufferSwap(screen_id, fb_info));
}

void GSP_GPU::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    [[maybe_unused]] auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    [[maybe_unused]] auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetAxiConfigQoSMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 mode = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called mode=0x{:08X}", mode);
}

void GSP_GPU::RegisterInterruptRelayQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
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
    IPC::RequestParser rp(ctx);

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
    int screen_id = (interrupt_id == InterruptId::PDC0)   ? 0
                    : (interrupt_id == InterruptId::PDC1) ? 1
                                                          : -1;
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
    if (active_thread_id == std::numeric_limits<u32>::max()) {
        return;
    }

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
        Memory::MemorySystem& memory = Core::System::GetInstance().Memory();

        // TODO: Consider attempting rasterizer-accelerated surface blit if that usage is ever
        // possible/likely
        Memory::RasterizerFlushVirtualRegion(command.dma_request.source_address,
                                             command.dma_request.size, Memory::FlushMode::Flush);
        Memory::RasterizerFlushVirtualRegion(command.dma_request.dest_address,
                                             command.dma_request.size,
                                             Memory::FlushMode::Invalidate);

        // TODO(Subv): These memory accesses should not go through the application's memory mapping.
        // They should go through the GSP module's memory mapping.
        memory.CopyBlock(*Core::System::GetInstance().Kernel().GetCurrentProcess(),
                         command.dma_request.dest_address, command.dma_request.source_address,
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
                         VirtualToPhysicalAddress(params.address) >> 3);
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
                             VirtualToPhysicalAddress(params.start1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].address_end)),
                             VirtualToPhysicalAddress(params.end1) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].value_32bit)),
                             params.value1);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[0].control)),
                             params.control1);
        }

        if (params.start2 != 0) {
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_start)),
                             VirtualToPhysicalAddress(params.start2) >> 3);
            WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(memory_fill_config[1].address_end)),
                             VirtualToPhysicalAddress(params.end2) >> 3);
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
                         VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister(static_cast<u32>(GPU_REG_INDEX(display_transfer_config.output_address)),
                         VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
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
                         VirtualToPhysicalAddress(params.in_buffer_address) >> 3);
        WriteGPURegister((u32)GPU_REG_INDEX(display_transfer_config.output_address),
                         VirtualToPhysicalAddress(params.out_buffer_address) >> 3);
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
    IPC::RequestParser rp(ctx);

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
    IPC::RequestParser rp(ctx);

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
    IPC::RequestParser rp(ctx);

    if (active_thread_id == std::numeric_limits<u32>::max()) {
        LOG_WARNING(Service_GSP, "Called without an active thread.");

        // TODO: Find the right error code.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(-1);
        return;
    }

    FrameBufferUpdate* top_screen = GetFrameBufferInfo(active_thread_id, 0);
    FrameBufferUpdate* bottom_screen = GetFrameBufferInfo(active_thread_id, 1);

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

static void CopyFrameBuffer(Core::System& system, VAddr dst, VAddr src, u32 stride, u32 lines) {
    auto dst_ptr = system.Memory().GetPointer(dst);
    const auto src_ptr = system.Memory().GetPointer(src);
    if (!dst_ptr || !src_ptr) {
        LOG_WARNING(Service_GSP,
                    "Could not resolve pointers for framebuffer capture, skipping screen.");
        return;
    }

    Memory::RasterizerFlushVirtualRegion(src, stride * lines, Memory::FlushMode::Flush);
    std::memcpy(dst_ptr, src_ptr, stride * lines);
    Memory::RasterizerFlushVirtualRegion(dst, stride * lines, Memory::FlushMode::Invalidate);
}

void GSP_GPU::SaveVramSysArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (active_thread_id == std::numeric_limits<u32>::max()) {
        LOG_WARNING(Service_GSP, "Called without an active thread.");

        // TODO: Find the right error code.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(-1);
        return;
    }

    LOG_INFO(Service_GSP, "called");

    // TODO: This should also save LCD register state.
    Memory::RasterizerFlushVirtualRegion(Memory::VRAM_VADDR, Memory::VRAM_SIZE,
                                         Memory::FlushMode::Flush);
    const auto vram = system.Memory().GetPointer(Memory::VRAM_VADDR);
    saved_vram.emplace(std::vector<u8>(Memory::VRAM_SIZE));
    std::memcpy(saved_vram.get().data(), vram, Memory::VRAM_SIZE);

    const auto top_screen = GetFrameBufferInfo(active_thread_id, 0);
    if (top_screen) {
        const auto top_fb = top_screen->framebuffer_info[top_screen->index];
        if (top_fb.address_left) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_TOP_LEFT, top_fb.address_left,
                            top_fb.stride, TOP_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to top left screen, skipping capture.");
        }
        if (top_fb.address_right) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_TOP_RIGHT, top_fb.address_right,
                            top_fb.stride, TOP_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to top right screen, skipping capture.");
        }
    } else {
        LOG_WARNING(Service_GSP, "No top screen bound, skipping capture.");
    }

    const auto bottom_screen = GetFrameBufferInfo(active_thread_id, 1);
    if (bottom_screen) {
        const auto bottom_fb = bottom_screen->framebuffer_info[bottom_screen->index];
        if (bottom_fb.address_left) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_BOTTOM, bottom_fb.address_left,
                            bottom_fb.stride, BOTTOM_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to bottom screen, skipping capture.");
        }
    } else {
        LOG_WARNING(Service_GSP, "No bottom screen bound, skipping capture.");
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void GSP_GPU::RestoreVramSysArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_GSP, "called");

    if (saved_vram) {
        // TODO: This should also restore LCD register state.
        auto vram = system.Memory().GetPointer(Memory::VRAM_VADDR);
        std::memcpy(vram, saved_vram.get().data(), Memory::VRAM_SIZE);
        Memory::RasterizerFlushVirtualRegion(Memory::VRAM_VADDR, Memory::VRAM_SIZE,
                                             Memory::FlushMode::Invalidate);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

ResultCode GSP_GPU::AcquireGpuRight(const Kernel::HLERequestContext& ctx,
                                    const std::shared_ptr<Kernel::Process>& process, u32 flag,
                                    bool blocking) {
    const auto session_data = GetSessionData(ctx.Session());

    LOG_DEBUG(Service_GSP, "called flag={:08X} process={} thread_id={}", flag, process->process_id,
              session_data->thread_id);

    if (active_thread_id == session_data->thread_id) {
        return {ErrorDescription::AlreadyDone, ErrorModule::GX, ErrorSummary::Success,
                ErrorLevel::Success};
    }

    if (blocking) {
        // TODO: The thread should be put to sleep until acquired.
        ASSERT_MSG(active_thread_id == std::numeric_limits<u32>::max(),
                   "Sleeping for GPU right is not yet supported.");
    } else if (active_thread_id != std::numeric_limits<u32>::max()) {
        return {ErrorDescription::Busy, ErrorModule::GX, ErrorSummary::WouldBlock,
                ErrorLevel::Status};
    }

    active_thread_id = session_data->thread_id;
    return RESULT_SUCCESS;
}

void GSP_GPU::TryAcquireRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto process = rp.PopObject<Kernel::Process>();

    const auto result = AcquireGpuRight(ctx, process, 0, false);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void GSP_GPU::AcquireRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto flag = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    const auto result = AcquireGpuRight(ctx, process, flag, true);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void GSP_GPU::ReleaseRight(const SessionData* session_data) {
    ASSERT_MSG(active_thread_id == session_data->thread_id,
               "Wrong thread tried to release GPU right");
    active_thread_id = std::numeric_limits<u32>::max();
}

void GSP_GPU::ReleaseRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const SessionData* session_data = GetSessionData(ctx.Session());
    ReleaseRight(session_data);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_GSP, "called");
}

void GSP_GPU::StoreDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetLedForceOff(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u8 state = rp.Pop<u8>();

    system.Kernel().GetSharedPageHandler().Set3DLed(state);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_GSP, "(STUBBED) called");
}

void GSP_GPU::SetInternalPriorities(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto priority = rp.Pop<u32>();
    const auto priority_with_rights = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_GSP, "(STUBBED) called priority={:#02X}, priority_with_rights={:#02X}",
              priority, priority_with_rights);
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

template <class Archive>
void GSP_GPU::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar& shared_memory;
    ar& active_thread_id;
    ar& first_initialization;
    ar& used_thread_ids;
    ar& saved_vram;
}
SERIALIZE_IMPL(GSP_GPU)

GSP_GPU::GSP_GPU(Core::System& system) : ServiceFramework("gsp::Gpu", 4), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &GSP_GPU::WriteHWRegs, "WriteHWRegs"},
        {0x0002, &GSP_GPU::WriteHWRegsWithMask, "WriteHWRegsWithMask"},
        {0x0003, nullptr, "WriteHWRegRepeat"},
        {0x0004, &GSP_GPU::ReadHWRegs, "ReadHWRegs"},
        {0x0005, &GSP_GPU::SetBufferSwap, "SetBufferSwap"},
        {0x0006, nullptr, "SetCommandList"},
        {0x0007, nullptr, "RequestDma"},
        {0x0008, &GSP_GPU::FlushDataCache, "FlushDataCache"},
        {0x0009, &GSP_GPU::InvalidateDataCache, "InvalidateDataCache"},
        {0x000A, nullptr, "RegisterInterruptEvents"},
        {0x000B, &GSP_GPU::SetLcdForceBlack, "SetLcdForceBlack"},
        {0x000C, &GSP_GPU::TriggerCmdReqQueue, "TriggerCmdReqQueue"},
        {0x000D, nullptr, "SetDisplayTransfer"},
        {0x000E, nullptr, "SetTextureCopy"},
        {0x000F, nullptr, "SetMemoryFill"},
        {0x0010, &GSP_GPU::SetAxiConfigQoSMode, "SetAxiConfigQoSMode"},
        {0x0011, nullptr, "SetPerfLogMode"},
        {0x0012, nullptr, "GetPerfLog"},
        {0x0013, &GSP_GPU::RegisterInterruptRelayQueue, "RegisterInterruptRelayQueue"},
        {0x0014, &GSP_GPU::UnregisterInterruptRelayQueue, "UnregisterInterruptRelayQueue"},
        {0x0015, &GSP_GPU::TryAcquireRight, "TryAcquireRight"},
        {0x0016, &GSP_GPU::AcquireRight, "AcquireRight"},
        {0x0017, &GSP_GPU::ReleaseRight, "ReleaseRight"},
        {0x0018, &GSP_GPU::ImportDisplayCaptureInfo, "ImportDisplayCaptureInfo"},
        {0x0019, &GSP_GPU::SaveVramSysArea, "SaveVramSysArea"},
        {0x001A, &GSP_GPU::RestoreVramSysArea, "RestoreVramSysArea"},
        {0x001B, nullptr, "ResetGpuCore"},
        {0x001C, &GSP_GPU::SetLedForceOff, "SetLedForceOff"},
        {0x001D, nullptr, "SetTestCommand"},
        {0x001E, &GSP_GPU::SetInternalPriorities, "SetInternalPriorities"},
        {0x001F, &GSP_GPU::StoreDataCache, "StoreDataCache"},
        // clang-format on
    };
    RegisterHandlers(functions);

    using Kernel::MemoryPermission;
    shared_memory = system.Kernel()
                        .CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                            MemoryPermission::ReadWrite, 0,
                                            Kernel::MemoryRegion::BASE, "GSP:SharedMemory")
                        .Unwrap();

    first_initialization = true;
};

std::unique_ptr<Kernel::SessionRequestHandler::SessionDataBase> GSP_GPU::MakeSessionData() {
    return std::make_unique<SessionData>(this);
}

template <class Archive>
void SessionData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(*this);
    ar& gsp;
    ar& interrupt_event;
    ar& thread_id;
    ar& registered;
}
SERIALIZE_IMPL(SessionData)

SessionData::SessionData(GSP_GPU* gsp) : gsp(gsp) {
    // Assign a new thread id to this session when it connects. Note: In the real GSP service this
    // is done through a real thread (svcCreateThread) but we have to simulate it since our HLE
    // services don't have threads.
    thread_id = gsp->GetUnusedThreadId();
    gsp->used_thread_ids[thread_id] = true;
}

SessionData::~SessionData() {
    // Free the thread id slot so that other sessions can use it.
    gsp->used_thread_ids[thread_id] = false;
}

} // namespace Service::GSP
