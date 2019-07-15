// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/memory.h"

namespace Kernel {

std::shared_ptr<CodeSet> KernelSystem::CreateCodeSet(std::string name, u64 program_id) {
    auto codeset{std::make_shared<CodeSet>(*this)};

    codeset->name = std::move(name);
    codeset->program_id = program_id;

    return codeset;
}

CodeSet::CodeSet(KernelSystem& kernel) : Object(kernel) {}
CodeSet::~CodeSet() {}

std::shared_ptr<Process> KernelSystem::CreateProcess(std::shared_ptr<CodeSet> code_set) {
    auto process{std::make_shared<Process>(*this)};

    process->codeset = std::move(code_set);
    process->flags.raw = 0;
    process->flags.memory_region.Assign(MemoryRegion::APPLICATION);
    process->status = ProcessStatus::Created;
    process->process_id = ++next_process_id;

    process_list.push_back(process);
    return process;
}

void Process::ParseKernelCaps(const u32* kernel_caps, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        u32 descriptor = kernel_caps[i];
        u32 type = descriptor >> 20;

        if (descriptor == 0xFFFFFFFF) {
            // Unused descriptor entry
            continue;
        } else if ((type & 0xF00) == 0xE00) { // 0x0FFF
            // Allowed interrupts list
            LOG_WARNING(Loader, "ExHeader allowed interrupts list ignored");
        } else if ((type & 0xF80) == 0xF00) { // 0x07FF
            // Allowed syscalls mask
            unsigned int index = ((descriptor >> 24) & 7) * 24;
            u32 bits = descriptor & 0xFFFFFF;

            while (bits && index < svc_access_mask.size()) {
                svc_access_mask.set(index, bits & 1);
                ++index;
                bits >>= 1;
            }
        } else if ((type & 0xFF0) == 0xFE0) { // 0x00FF
            // Handle table size
            handle_table_size = descriptor & 0x3FF;
        } else if ((type & 0xFF8) == 0xFF0) { // 0x007F
            // Misc. flags
            flags.raw = descriptor & 0xFFFF;
        } else if ((type & 0xFFE) == 0xFF8) { // 0x001F
            // Mapped memory range
            if (i + 1 >= len || ((kernel_caps[i + 1] >> 20) & 0xFFE) != 0xFF8) {
                LOG_WARNING(Loader, "Incomplete exheader memory range descriptor ignored.");
                continue;
            }
            u32 end_desc = kernel_caps[i + 1];
            ++i; // Skip over the second descriptor on the next iteration

            AddressMapping mapping;
            mapping.address = descriptor << 12;
            VAddr end_address = end_desc << 12;

            if (mapping.address < end_address) {
                mapping.size = end_address - mapping.address;
            } else {
                mapping.size = 0;
            }

            mapping.read_only = (descriptor & (1 << 20)) != 0;
            mapping.unk_flag = (end_desc & (1 << 20)) != 0;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFFF) == 0xFFE) { // 0x000F
            // Mapped memory page
            AddressMapping mapping;
            mapping.address = descriptor << 12;
            mapping.size = Memory::PAGE_SIZE;
            mapping.read_only = false;
            mapping.unk_flag = false;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFE0) == 0xFC0) { // 0x01FF
            // Kernel version
            kernel_version = descriptor & 0xFFFF;

            int minor = kernel_version & 0xFF;
            int major = (kernel_version >> 8) & 0xFF;
            LOG_INFO(Loader, "ExHeader kernel version: {}.{}", major, minor);
        } else {
            LOG_ERROR(Loader, "Unhandled kernel caps descriptor: 0x{:08X}", descriptor);
        }
    }
}

void Process::Run(s32 main_thread_priority, u32 stack_size) {
    memory_region = kernel.GetMemoryRegion(flags.memory_region);

    auto MapSegment = [&](CodeSet::Segment& segment, VMAPermission permissions,
                          MemoryState memory_state) {
        HeapAllocate(segment.addr, segment.size, permissions, memory_state, true);
        kernel.memory.WriteBlock(*this, segment.addr, codeset->memory.data() + segment.offset,
                                 segment.size);
    };

    // Map CodeSet segments
    MapSegment(codeset->CodeSegment(), VMAPermission::ReadExecute, MemoryState::Code);
    MapSegment(codeset->RODataSegment(), VMAPermission::Read, MemoryState::Code);
    MapSegment(codeset->DataSegment(), VMAPermission::ReadWrite, MemoryState::Private);

    // Allocate and map stack
    HeapAllocate(Memory::HEAP_VADDR_END - stack_size, stack_size, VMAPermission::ReadWrite,
                 MemoryState::Locked, true);

    // Map special address mappings
    kernel.MapSharedPages(vm_manager);
    for (const auto& mapping : address_mappings) {
        kernel.HandleSpecialMapping(vm_manager, mapping);
    }

    status = ProcessStatus::Running;

    vm_manager.LogLayout(Log::Level::Debug);
    Kernel::SetupMainThread(kernel, codeset->entrypoint, main_thread_priority, SharedFrom(this));
}

VAddr Process::GetLinearHeapAreaAddress() const {
    // Starting from system version 8.0.0 a new linear heap layout is supported to allow usage of
    // the extra RAM in the n3DS.
    return kernel_version < 0x22C ? Memory::LINEAR_HEAP_VADDR : Memory::NEW_LINEAR_HEAP_VADDR;
}

VAddr Process::GetLinearHeapBase() const {
    return GetLinearHeapAreaAddress() + memory_region->base;
}

VAddr Process::GetLinearHeapLimit() const {
    return GetLinearHeapBase() + memory_region->size;
}

ResultVal<VAddr> Process::HeapAllocate(VAddr target, u32 size, VMAPermission perms,
                                       MemoryState memory_state, bool skip_range_check) {
    LOG_DEBUG(Kernel, "Allocate heap target={:08X}, size={:08X}", target, size);
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        if (!skip_range_check) {
            LOG_ERROR(Kernel, "Invalid heap address");
            return ERR_INVALID_ADDRESS;
        }
    }

    auto vma = vm_manager.FindVMA(target);
    if (vma->second.type != VMAType::Free || vma->second.base + vma->second.size < target + size) {
        LOG_ERROR(Kernel, "Trying to allocate already allocated memory");
        return ERR_INVALID_ADDRESS_STATE;
    }

    auto allocated_fcram = memory_region->HeapAllocate(size);
    if (allocated_fcram.empty()) {
        LOG_ERROR(Kernel, "Not enough space");
        return ERR_OUT_OF_HEAP_MEMORY;
    }

    // Maps heap block by block
    VAddr interval_target = target;
    for (const auto& interval : allocated_fcram) {
        u32 interval_size = interval.upper() - interval.lower();
        LOG_DEBUG(Kernel, "Allocated FCRAM region lower={:08X}, upper={:08X}", interval.lower(),
                  interval.upper());
        std::fill(kernel.memory.GetFCRAMPointer(interval.lower()),
                  kernel.memory.GetFCRAMPointer(interval.upper()), 0);
        auto vma = vm_manager.MapBackingMemory(interval_target,
                                               kernel.memory.GetFCRAMPointer(interval.lower()),
                                               interval_size, memory_state);
        ASSERT(vma.Succeeded());
        vm_manager.Reprotect(vma.Unwrap(), perms);
        interval_target += interval_size;
    }

    memory_used += size;
    resource_limit->current_commit += size;

    return MakeResult<VAddr>(target);
}

ResultCode Process::HeapFree(VAddr target, u32 size) {
    LOG_DEBUG(Kernel, "Free heap target={:08X}, size={:08X}", target, size);
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        LOG_ERROR(Kernel, "Invalid heap address");
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        return RESULT_SUCCESS;
    }

    // Free heaps block by block
    CASCADE_RESULT(auto backing_blocks, vm_manager.GetBackingBlocksForRange(target, size));
    for (const auto [backing_memory, block_size] : backing_blocks) {
        memory_region->Free(kernel.memory.GetFCRAMOffset(backing_memory), block_size);
    }

    ResultCode result = vm_manager.UnmapRange(target, size);
    ASSERT(result.IsSuccess());

    memory_used -= size;
    resource_limit->current_commit -= size;

    return RESULT_SUCCESS;
}

ResultVal<VAddr> Process::LinearAllocate(VAddr target, u32 size, VMAPermission perms) {
    LOG_DEBUG(Kernel, "Allocate linear heap target={:08X}, size={:08X}", target, size);
    u32 physical_offset;
    if (target == 0) {
        auto offset = memory_region->LinearAllocate(size);
        if (!offset) {
            LOG_ERROR(Kernel, "Not enough space");
            return ERR_OUT_OF_HEAP_MEMORY;
        }
        physical_offset = *offset;
        target = physical_offset + GetLinearHeapAreaAddress();
    } else {
        if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() ||
            target + size < target) {
            LOG_ERROR(Kernel, "Invalid linear heap address");
            return ERR_INVALID_ADDRESS;
        }

        // Kernel would crash/return error when target doesn't meet some requirement.
        // It seems that target is required to follow immediately after the allocated linear heap,
        // or cover the entire hole if there is any.
        // Right now we just ignore these checks because they are still unclear. Further more,
        // games and homebrew only ever seem to pass target = 0 here (which lets the kernel decide
        // the address), so this not important.

        physical_offset = target - GetLinearHeapAreaAddress(); // relative to FCRAM
        if (!memory_region->LinearAllocate(physical_offset, size)) {
            LOG_ERROR(Kernel, "Trying to allocate already allocated memory");
            return ERR_INVALID_ADDRESS_STATE;
        }
    }

    u8* backing_memory = kernel.memory.GetFCRAMPointer(physical_offset);

    std::fill(backing_memory, backing_memory + size, 0);
    auto vma = vm_manager.MapBackingMemory(target, backing_memory, size, MemoryState::Continuous);
    ASSERT(vma.Succeeded());
    vm_manager.Reprotect(vma.Unwrap(), perms);

    memory_used += size;
    resource_limit->current_commit += size;

    LOG_DEBUG(Kernel, "Allocated at target={:08X}", target);
    return MakeResult<VAddr>(target);
}

ResultCode Process::LinearFree(VAddr target, u32 size) {
    LOG_DEBUG(Kernel, "Free linear heap target={:08X}, size={:08X}", target, size);
    if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() ||
        target + size < target) {
        LOG_ERROR(Kernel, "Invalid linear heap address");
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        return RESULT_SUCCESS;
    }

    ResultCode result = vm_manager.UnmapRange(target, size);
    if (result.IsError()) {
        LOG_ERROR(Kernel, "Trying to free already freed memory");
        return result;
    }

    memory_used -= size;
    resource_limit->current_commit -= size;

    u32 physical_offset = target - GetLinearHeapAreaAddress(); // relative to FCRAM
    memory_region->Free(physical_offset, size);

    return RESULT_SUCCESS;
}

ResultCode Process::Map(VAddr target, VAddr source, u32 size, VMAPermission perms,
                        bool privileged) {
    LOG_DEBUG(Kernel, "Map memory target={:08X}, source={:08X}, size={:08X}, perms={:08X}", target,
              source, size, static_cast<u8>(perms));
    if (source < Memory::HEAP_VADDR || source + size > Memory::HEAP_VADDR_END ||
        source + size < source) {
        LOG_ERROR(Kernel, "Invalid source address");
        return ERR_INVALID_ADDRESS;
    }

    // TODO(wwylele): check target address range. Is it also restricted to heap region?

    auto vma = vm_manager.FindVMA(target);
    if (vma->second.type != VMAType::Free || vma->second.base + vma->second.size < target + size) {
        LOG_ERROR(Kernel, "Trying to map to already allocated memory");
        return ERR_INVALID_ADDRESS_STATE;
    }

    // Check range overlapping
    if (source - target < size || target - source < size) {
        if (privileged) {
            if (source == target) {
                // privileged Map allows identical source and target address, which simply changes
                // the state and the permission of the memory
                return vm_manager.ChangeMemoryState(source, size, MemoryState::Private,
                                                    VMAPermission::ReadWrite,
                                                    MemoryState::AliasCode, perms);
            } else {
                return ERR_INVALID_ADDRESS;
            }
        } else {
            return ERR_INVALID_ADDRESS_STATE;
        }
    }

    MemoryState source_state = privileged ? MemoryState::Locked : MemoryState::Aliased;
    MemoryState target_state = privileged ? MemoryState::AliasCode : MemoryState::Alias;
    VMAPermission source_perm = privileged ? VMAPermission::None : VMAPermission::ReadWrite;

    // Mark source region as Aliased
    CASCADE_CODE(vm_manager.ChangeMemoryState(source, size, MemoryState::Private,
                                              VMAPermission::ReadWrite, source_state, source_perm));

    CASCADE_RESULT(auto backing_blocks, vm_manager.GetBackingBlocksForRange(source, size));
    VAddr interval_target = target;
    for (const auto [backing_memory, block_size] : backing_blocks) {
        auto target_vma =
            vm_manager.MapBackingMemory(interval_target, backing_memory, block_size, target_state);
        ASSERT(target_vma.Succeeded());
        vm_manager.Reprotect(target_vma.Unwrap(), perms);
        interval_target += block_size;
    }

    return RESULT_SUCCESS;
}
ResultCode Process::Unmap(VAddr target, VAddr source, u32 size, VMAPermission perms,
                          bool privileged) {
    LOG_DEBUG(Kernel, "Unmap memory target={:08X}, source={:08X}, size={:08X}, perms={:08X}",
              target, source, size, static_cast<u8>(perms));
    if (source < Memory::HEAP_VADDR || source + size > Memory::HEAP_VADDR_END ||
        source + size < source) {
        LOG_ERROR(Kernel, "Invalid source address");
        return ERR_INVALID_ADDRESS;
    }

    // TODO(wwylele): check target address range. Is it also restricted to heap region?

    // TODO(wwylele): check that the source and the target are actually a pair created by Map
    // Should return error 0xD8E007F5 in this case

    if (source - target < size || target - source < size) {
        if (privileged) {
            if (source == target) {
                // privileged Unmap allows identical source and target address, which simply changes
                // the state and the permission of the memory
                return vm_manager.ChangeMemoryState(source, size, MemoryState::AliasCode,
                                                    VMAPermission::None, MemoryState::Private,
                                                    perms);
            } else {
                return ERR_INVALID_ADDRESS;
            }
        } else {
            return ERR_INVALID_ADDRESS_STATE;
        }
    }

    MemoryState source_state = privileged ? MemoryState::Locked : MemoryState::Aliased;

    CASCADE_CODE(vm_manager.UnmapRange(target, size));

    // Change back source region state. Note that the permission is reprotected according to param
    CASCADE_CODE(vm_manager.ChangeMemoryState(source, size, source_state, VMAPermission::None,
                                              MemoryState::Private, perms));

    return RESULT_SUCCESS;
}

Kernel::Process::Process(KernelSystem& kernel)
    : Object(kernel), handle_table(kernel), vm_manager(kernel.memory), kernel(kernel) {

    kernel.memory.RegisterPageTable(&vm_manager.page_table);
}
Kernel::Process::~Process() {
    // Release all objects this process owns first so that their potential destructor can do clean
    // up with this process before further destruction.
    // TODO(wwylele): explicitly destroy or invalidate objects this process owns (threads, shared
    // memory etc.) even if they are still referenced by other processes.
    handle_table.Clear();

    kernel.memory.UnregisterPageTable(&vm_manager.page_table);
}

std::shared_ptr<Process> KernelSystem::GetProcessById(u32 process_id) const {
    auto itr = std::find_if(
        process_list.begin(), process_list.end(),
        [&](const std::shared_ptr<Process>& process) { return process->process_id == process_id; });

    if (itr == process_list.end())
        return nullptr;

    return *itr;
}
} // namespace Kernel
