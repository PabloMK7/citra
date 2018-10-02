// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"

namespace Kernel {

ResultCode TranslateCommandBuffer(SharedPtr<Thread> src_thread, SharedPtr<Thread> dst_thread,
                                  VAddr src_address, VAddr dst_address, bool reply) {

    auto& src_process = src_thread->owner_process;
    auto& dst_process = dst_thread->owner_process;

    IPC::Header header;
    // TODO(Subv): Replace by Memory::Read32 when possible.
    Memory::ReadBlock(*src_process, src_address, &header.raw, sizeof(header.raw));

    std::size_t untranslated_size = 1u + header.normal_params_size;
    std::size_t command_size = untranslated_size + header.translate_params_size;

    // Note: The real kernel does not check that the command length fits into the IPC buffer area.
    ASSERT(command_size <= IPC::COMMAND_BUFFER_LENGTH);

    std::array<u32, IPC::COMMAND_BUFFER_LENGTH> cmd_buf;
    Memory::ReadBlock(*src_process, src_address, cmd_buf.data(), command_size * sizeof(u32));

    // Create a copy of the target's command buffer
    IPC::Header dst_header;
    Memory::ReadBlock(*dst_process, dst_address, &dst_header.raw, sizeof(dst_header.raw));

    std::size_t dst_untranslated_size = 1u + dst_header.normal_params_size;
    std::size_t dst_command_size = dst_untranslated_size + dst_header.translate_params_size;
    std::size_t target_index = dst_untranslated_size;

    std::array<u32, IPC::COMMAND_BUFFER_LENGTH> dst_cmd_buf;
    Memory::ReadBlock(*dst_process, dst_address, dst_cmd_buf.data(),
                      dst_command_size * sizeof(u32));

    std::size_t i = untranslated_size;
    while (i < command_size) {
        u32 descriptor = cmd_buf[i];
        i += 1;

        switch (IPC::GetDescriptorType(descriptor)) {
        case IPC::DescriptorType::CopyHandle:
        case IPC::DescriptorType::MoveHandle: {
            u32 num_handles = IPC::HandleNumberFromDesc(descriptor);
            // Note: The real kernel does not check that the number of handles fits into the command
            // buffer before writing them, only after finishing.
            if (i + num_handles > command_size) {
                return ResultCode(ErrCodes::CommandTooLarge, ErrorModule::OS,
                                  ErrorSummary::InvalidState, ErrorLevel::Status);
            }

            for (u32 j = 0; j < num_handles; ++j) {
                Handle handle = cmd_buf[i];
                SharedPtr<Object> object = nullptr;
                // Perform pseudo-handle detection here because by the time this function is called,
                // the current thread and process are no longer the ones which created this IPC
                // request, but the ones that are handling it.
                if (handle == CurrentThread) {
                    object = src_thread;
                } else if (handle == CurrentProcess) {
                    object = src_process;
                } else if (handle != 0) {
                    object = g_handle_table.GetGeneric(handle);
                    if (descriptor == IPC::DescriptorType::MoveHandle) {
                        g_handle_table.Close(handle);
                    }
                }

                if (object == nullptr) {
                    // Note: The real kernel sets invalid translated handles to 0 in the target
                    // command buffer.
                    cmd_buf[i++] = 0;
                    continue;
                }

                auto result = g_handle_table.Create(std::move(object));
                cmd_buf[i++] = result.ValueOr(0);
            }
            break;
        }
        case IPC::DescriptorType::CallingPid: {
            cmd_buf[i++] = src_process->process_id;
            break;
        }
        case IPC::DescriptorType::StaticBuffer: {
            IPC::StaticBufferDescInfo bufferInfo{descriptor};
            VAddr static_buffer_src_address = cmd_buf[i];

            std::vector<u8> data(bufferInfo.size);
            Memory::ReadBlock(*src_process, static_buffer_src_address, data.data(), data.size());

            // Grab the address that the target thread set up to receive the response static buffer
            // and write our data there. The static buffers area is located right after the command
            // buffer area.
            struct StaticBuffer {
                IPC::StaticBufferDescInfo descriptor;
                VAddr address;
            };

            static_assert(sizeof(StaticBuffer) == 8, "StaticBuffer struct has incorrect size.");

            StaticBuffer target_buffer;

            u32 static_buffer_offset = IPC::COMMAND_BUFFER_LENGTH * sizeof(u32) +
                                       sizeof(StaticBuffer) * bufferInfo.buffer_id;
            Memory::ReadBlock(*dst_process, dst_address + static_buffer_offset, &target_buffer,
                              sizeof(target_buffer));

            // Note: The real kernel doesn't seem to have any error recovery mechanisms for this
            // case.
            ASSERT_MSG(target_buffer.descriptor.size >= data.size(),
                       "Static buffer data is too big");

            Memory::WriteBlock(*dst_process, target_buffer.address, data.data(), data.size());

            cmd_buf[i++] = target_buffer.address;
            break;
        }
        case IPC::DescriptorType::MappedBuffer: {
            IPC::MappedBufferDescInfo descInfo{descriptor};
            VAddr source_address = cmd_buf[i];

            u32 size = static_cast<u32>(descInfo.size);
            IPC::MappedBufferPermissions permissions = descInfo.perms;

            VAddr page_start = Common::AlignDown(source_address, Memory::PAGE_SIZE);
            u32 page_offset = source_address - page_start;
            u32 num_pages =
                Common::AlignUp(page_offset + size, Memory::PAGE_SIZE) >> Memory::PAGE_BITS;

            // Skip when the size is zero and num_pages == 0
            if (size == 0) {
                cmd_buf[i++] = 0;
                break;
            }
            ASSERT(num_pages >= 1);

            if (reply) {
                // Scan the target's command buffer for the matching mapped buffer
                while (target_index < dst_command_size) {
                    u32 desc = dst_cmd_buf[target_index++];

                    if (IPC::GetDescriptorType(desc) == IPC::DescriptorType::MappedBuffer) {
                        IPC::MappedBufferDescInfo dest_descInfo{desc};
                        VAddr dest_address = dst_cmd_buf[target_index];

                        u32 dest_size = static_cast<u32>(dest_descInfo.size);
                        IPC::MappedBufferPermissions dest_permissions = dest_descInfo.perms;

                        ASSERT(permissions == dest_permissions && size == dest_size);
                        // Readonly buffers do not need to be copied over to the target
                        // process again because they were (presumably) not modified. This
                        // behavior is consistent with the real kernel.
                        if (permissions != IPC::MappedBufferPermissions::R) {
                            // Copy the modified buffer back into the target process
                            Memory::CopyBlock(*src_process, *dst_process, source_address,
                                              dest_address, size);
                        }

                        VAddr prev_reserve = page_start - Memory::PAGE_SIZE;
                        VAddr next_reserve = page_start + num_pages * Memory::PAGE_SIZE;

                        auto& prev_vma = src_process->vm_manager.FindVMA(prev_reserve)->second;
                        auto& next_vma = src_process->vm_manager.FindVMA(next_reserve)->second;
                        ASSERT(prev_vma.meminfo_state == MemoryState::Reserved &&
                               next_vma.meminfo_state == MemoryState::Reserved);

                        // Unmap the buffer and guard pages from the source process
                        ResultCode result = src_process->vm_manager.UnmapRange(
                            page_start - Memory::PAGE_SIZE, (num_pages + 2) * Memory::PAGE_SIZE);
                        ASSERT(result == RESULT_SUCCESS);

                        target_index += 1;
                        break;
                    }

                    target_index += 1;
                }

                i += 1;
                break;
            }

            VAddr target_address = 0;

            // TODO(Subv): Perform permission checks.

            // Reserve a page of memory before the mapped buffer
            auto reserve_buffer = std::make_shared<std::vector<u8>>(Memory::PAGE_SIZE);
            dst_process->vm_manager.MapMemoryBlockToBase(
                Memory::IPC_MAPPING_VADDR, Memory::IPC_MAPPING_SIZE, reserve_buffer, 0,
                static_cast<u32>(reserve_buffer->size()), Kernel::MemoryState::Reserved);

            auto buffer = std::make_shared<std::vector<u8>>(num_pages * Memory::PAGE_SIZE);
            Memory::ReadBlock(*src_process, source_address, buffer->data() + page_offset, size);

            // Map the page(s) into the target process' address space.
            target_address = dst_process->vm_manager
                                 .MapMemoryBlockToBase(
                                     Memory::IPC_MAPPING_VADDR, Memory::IPC_MAPPING_SIZE, buffer, 0,
                                     static_cast<u32>(buffer->size()), Kernel::MemoryState::Shared)
                                 .Unwrap();

            cmd_buf[i++] = target_address + page_offset;

            // Reserve a page of memory after the mapped buffer
            dst_process->vm_manager.MapMemoryBlockToBase(
                Memory::IPC_MAPPING_VADDR, Memory::IPC_MAPPING_SIZE, reserve_buffer, 0,
                static_cast<u32>(reserve_buffer->size()), Kernel::MemoryState::Reserved);
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unsupported handle translation: {:#010X}", descriptor);
        }
    }

    Memory::WriteBlock(*dst_process, dst_address, cmd_buf.data(), command_size * sizeof(u32));

    return RESULT_SUCCESS;
}
} // namespace Kernel
