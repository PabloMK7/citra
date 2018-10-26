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

SharedPtr<CodeSet> KernelSystem::CreateCodeSet(std::string name, u64 program_id) {
    SharedPtr<CodeSet> codeset(new CodeSet(*this));

    codeset->name = std::move(name);
    codeset->program_id = program_id;

    return codeset;
}

CodeSet::CodeSet(KernelSystem& kernel) : Object(kernel) {}
CodeSet::~CodeSet() {}

SharedPtr<Process> KernelSystem::CreateProcess(SharedPtr<CodeSet> code_set) {
    SharedPtr<Process> process(new Process(*this));

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
    memory_region = GetMemoryRegion(flags.memory_region);

    auto MapSegment = [&](CodeSet::Segment& segment, VMAPermission permissions,
                          MemoryState memory_state) {
        auto vma = vm_manager
                       .MapMemoryBlock(segment.addr, codeset->memory, segment.offset, segment.size,
                                       memory_state)
                       .Unwrap();
        vm_manager.Reprotect(vma, permissions);
        misc_memory_used += segment.size;
        memory_region->used += segment.size;
    };

    // Map CodeSet segments
    MapSegment(codeset->CodeSegment(), VMAPermission::ReadExecute, MemoryState::Code);
    MapSegment(codeset->RODataSegment(), VMAPermission::Read, MemoryState::Code);
    MapSegment(codeset->DataSegment(), VMAPermission::ReadWrite, MemoryState::Private);

    // Allocate and map stack
    vm_manager
        .MapMemoryBlock(Memory::HEAP_VADDR_END - stack_size,
                        std::make_shared<std::vector<u8>>(stack_size, 0), 0, stack_size,
                        MemoryState::Locked)
        .Unwrap();
    misc_memory_used += stack_size;
    memory_region->used += stack_size;

    // Map special address mappings
    MapSharedPages(vm_manager);
    for (const auto& mapping : address_mappings) {
        HandleSpecialMapping(vm_manager, mapping);
    }

    status = ProcessStatus::Running;

    vm_manager.LogLayout(Log::Level::Debug);
    Kernel::SetupMainThread(kernel, codeset->entrypoint, main_thread_priority, this);
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

ResultVal<VAddr> Process::HeapAllocate(VAddr target, u32 size, VMAPermission perms) {
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        return ERR_INVALID_ADDRESS;
    }

    if (heap_memory == nullptr) {
        // Initialize heap
        heap_memory = std::make_shared<std::vector<u8>>();
        heap_start = heap_end = target;
    }

    // If necessary, expand backing vector to cover new heap extents.
    if (target < heap_start) {
        heap_memory->insert(begin(*heap_memory), heap_start - target, 0);
        heap_start = target;
        vm_manager.RefreshMemoryBlockMappings(heap_memory.get());
    }
    if (target + size > heap_end) {
        heap_memory->insert(end(*heap_memory), (target + size) - heap_end, 0);
        heap_end = target + size;
        vm_manager.RefreshMemoryBlockMappings(heap_memory.get());
    }
    ASSERT(heap_end - heap_start == heap_memory->size());

    CASCADE_RESULT(auto vma, vm_manager.MapMemoryBlock(target, heap_memory, target - heap_start,
                                                       size, MemoryState::Private));
    vm_manager.Reprotect(vma, perms);

    heap_used += size;
    memory_region->used += size;

    return MakeResult<VAddr>(heap_end - size);
}

ResultCode Process::HeapFree(VAddr target, u32 size) {
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        return RESULT_SUCCESS;
    }

    ResultCode result = vm_manager.UnmapRange(target, size);
    if (result.IsError())
        return result;

    heap_used -= size;
    memory_region->used -= size;

    return RESULT_SUCCESS;
}

ResultVal<VAddr> Process::LinearAllocate(VAddr target, u32 size, VMAPermission perms) {
    auto& linheap_memory = memory_region->linear_heap_memory;

    VAddr heap_end = GetLinearHeapBase() + (u32)linheap_memory->size();
    // Games and homebrew only ever seem to pass 0 here (which lets the kernel decide the address),
    // but explicit addresses are also accepted and respected.
    if (target == 0) {
        target = heap_end;
    }

    if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() || target > heap_end ||
        target + size < target) {

        return ERR_INVALID_ADDRESS;
    }

    // Expansion of the linear heap is only allowed if you do an allocation immediately at its
    // end. It's possible to free gaps in the middle of the heap and then reallocate them later,
    // but expansions are only allowed at the end.
    if (target == heap_end) {
        linheap_memory->insert(linheap_memory->end(), size, 0);
        vm_manager.RefreshMemoryBlockMappings(linheap_memory.get());
    }

    // TODO(yuriks): As is, this lets processes map memory allocated by other processes from the
    // same region. It is unknown if or how the 3DS kernel checks against this.
    std::size_t offset = target - GetLinearHeapBase();
    CASCADE_RESULT(auto vma, vm_manager.MapMemoryBlock(target, linheap_memory, offset, size,
                                                       MemoryState::Continuous));
    vm_manager.Reprotect(vma, perms);

    linear_heap_used += size;
    memory_region->used += size;

    return MakeResult<VAddr>(target);
}

ResultCode Process::LinearFree(VAddr target, u32 size) {
    auto& linheap_memory = memory_region->linear_heap_memory;

    if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() ||
        target + size < target) {

        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        return RESULT_SUCCESS;
    }

    VAddr heap_end = GetLinearHeapBase() + (u32)linheap_memory->size();
    if (target + size > heap_end) {
        return ERR_INVALID_ADDRESS_STATE;
    }

    ResultCode result = vm_manager.UnmapRange(target, size);
    if (result.IsError())
        return result;

    linear_heap_used -= size;
    memory_region->used -= size;

    if (target + size == heap_end) {
        // End of linear heap has been freed, so check what's the last allocated block in it and
        // reduce the size.
        auto vma = vm_manager.FindVMA(target);
        ASSERT(vma != vm_manager.vma_map.end());
        ASSERT(vma->second.type == VMAType::Free);
        VAddr new_end = vma->second.base;
        if (new_end >= GetLinearHeapBase()) {
            linheap_memory->resize(new_end - GetLinearHeapBase());
        }
    }

    return RESULT_SUCCESS;
}

Kernel::Process::Process(KernelSystem& kernel)
    : Object(kernel), handle_table(kernel), kernel(kernel) {}
Kernel::Process::~Process() {}

SharedPtr<Process> KernelSystem::GetProcessById(u32 process_id) const {
    auto itr = std::find_if(
        process_list.begin(), process_list.end(),
        [&](const SharedPtr<Process>& process) { return process->process_id == process_id; });

    if (itr == process_list.end())
        return nullptr;

    return *itr;
}
} // namespace Kernel
