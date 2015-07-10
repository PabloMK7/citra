// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "common/make_unique.h"

#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/mem_map.h"
#include "core/memory.h"

namespace Kernel {

SharedPtr<CodeSet> CodeSet::Create(std::string name, u64 program_id) {
    SharedPtr<CodeSet> codeset(new CodeSet);

    codeset->name = std::move(name);
    codeset->program_id = program_id;

    return codeset;
}

CodeSet::CodeSet() {}
CodeSet::~CodeSet() {}

u32 Process::next_process_id;

SharedPtr<Process> Process::Create(SharedPtr<CodeSet> code_set) {
    SharedPtr<Process> process(new Process);

    process->codeset = std::move(code_set);
    process->flags.raw = 0;
    process->flags.memory_region = MemoryRegion::APPLICATION;
    process->address_space = Common::make_unique<VMManager>();
    Memory::InitLegacyAddressSpace(*process->address_space);

    return process;
}

void Process::ParseKernelCaps(const u32* kernel_caps, size_t len) {
    for (size_t i = 0; i < len; ++i) {
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
                ++index; bits >>= 1;
            }
        } else if ((type & 0xFF0) == 0xFE0) { // 0x00FF
            // Handle table size
            handle_table_size = descriptor & 0x3FF;
        } else if ((type & 0xFF8) == 0xFF0) { // 0x007F
            // Misc. flags
            flags.raw = descriptor & 0xFFFF;
        } else if ((type & 0xFFE) == 0xFF8) { // 0x001F
            // Mapped memory range
            if (i+1 >= len || ((kernel_caps[i+1] >> 20) & 0xFFE) != 0xFF8) {
                LOG_WARNING(Loader, "Incomplete exheader memory range descriptor ignored.");
                continue;
            }
            u32 end_desc = kernel_caps[i+1];
            ++i; // Skip over the second descriptor on the next iteration

            AddressMapping mapping;
            mapping.address = descriptor << 12;
            mapping.size = (end_desc << 12) - mapping.address;
            mapping.writable = (descriptor & (1 << 20)) != 0;
            mapping.unk_flag = (end_desc & (1 << 20)) != 0;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFFF) == 0xFFE) { // 0x000F
            // Mapped memory page
            AddressMapping mapping;
            mapping.address = descriptor << 12;
            mapping.size = Memory::PAGE_SIZE;
            mapping.writable = true; // TODO: Not sure if correct
            mapping.unk_flag = false;
        } else if ((type & 0xFE0) == 0xFC0) { // 0x01FF
            // Kernel version
            int minor = descriptor & 0xFF;
            int major = (descriptor >> 8) & 0xFF;
            LOG_INFO(Loader, "ExHeader kernel version ignored: %d.%d", major, minor);
        } else {
            LOG_ERROR(Loader, "Unhandled kernel caps descriptor: 0x%08X", descriptor);
        }
    }
}

void Process::Run(s32 main_thread_priority, u32 stack_size) {
    auto MapSegment = [&](CodeSet::Segment& segment, VMAPermission permissions, MemoryState memory_state) {
        auto vma = address_space->MapMemoryBlock(segment.addr, codeset->memory,
                segment.offset, segment.size, memory_state).Unwrap();
        address_space->Reprotect(vma, permissions);
    };

    MapSegment(codeset->code,   VMAPermission::ReadExecute, MemoryState::Code);
    MapSegment(codeset->rodata, VMAPermission::Read,        MemoryState::Code);
    MapSegment(codeset->data,   VMAPermission::ReadWrite,   MemoryState::Private);

    address_space->LogLayout();
    Kernel::SetupMainThread(codeset->entrypoint, main_thread_priority);
}

Kernel::Process::Process() {}
Kernel::Process::~Process() {}

SharedPtr<Process> g_current_process;

}
