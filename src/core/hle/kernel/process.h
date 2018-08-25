// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <boost/container/static_vector.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/vm_manager.h"

namespace Kernel {

struct AddressMapping {
    // Address and size must be page-aligned
    VAddr address;
    u32 size;
    bool read_only;
    bool unk_flag;
};

enum class MemoryRegion : u16 {
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

union ProcessFlags {
    u16 raw;

    BitField<0, 1, u16>
        allow_debug; ///< Allows other processes to attach to and debug this process.
    BitField<1, 1, u16> force_debug; ///< Allows this process to attach to processes even if they
                                     /// don't have allow_debug set.
    BitField<2, 1, u16> allow_nonalphanum;
    BitField<3, 1, u16> shared_page_writable; ///< Shared page is mapped with write permissions.
    BitField<4, 1, u16> privileged_priority;  ///< Can use priority levels higher than 24.
    BitField<5, 1, u16> allow_main_args;
    BitField<6, 1, u16> shared_device_mem;
    BitField<7, 1, u16> runnable_on_sleep;
    BitField<8, 4, MemoryRegion>
        memory_region;                ///< Default region for memory allocations for this process
    BitField<12, 1, u16> loaded_high; ///< Application loaded high (not at 0x00100000).
};

enum class ProcessStatus { Created, Running, Exited };

class ResourceLimit;
struct MemoryRegionInfo;

struct CodeSet final : public Object {
    struct Segment {
        size_t offset = 0;
        VAddr addr = 0;
        u32 size = 0;
    };

    static SharedPtr<CodeSet> Create(std::string name, u64 program_id);

    std::string GetTypeName() const override {
        return "CodeSet";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::CodeSet;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    Segment& CodeSegment() {
        return segments[0];
    }

    const Segment& CodeSegment() const {
        return segments[0];
    }

    Segment& RODataSegment() {
        return segments[1];
    }

    const Segment& RODataSegment() const {
        return segments[1];
    }

    Segment& DataSegment() {
        return segments[2];
    }

    const Segment& DataSegment() const {
        return segments[2];
    }

    std::shared_ptr<std::vector<u8>> memory;

    std::array<Segment, 3> segments;
    VAddr entrypoint;

    /// Name of the process
    std::string name;
    /// Title ID corresponding to the process
    u64 program_id;

private:
    CodeSet();
    ~CodeSet() override;
};

class Process final : public Object {
public:
    static SharedPtr<Process> Create(SharedPtr<CodeSet> code_set);

    std::string GetTypeName() const override {
        return "Process";
    }
    std::string GetName() const override {
        return codeset->name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Process;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    static u32 next_process_id;

    SharedPtr<CodeSet> codeset;
    /// Resource limit descriptor for this process
    SharedPtr<ResourceLimit> resource_limit;

    /// The process may only call SVCs which have the corresponding bit set.
    std::bitset<0x80> svc_access_mask;
    /// Maximum size of the handle table for the process.
    unsigned int handle_table_size = 0x200;
    /// Special memory ranges mapped into this processes address space. This is used to give
    /// processes access to specific I/O regions and device memory.
    boost::container::static_vector<AddressMapping, 8> address_mappings;
    ProcessFlags flags;
    /// Kernel compatibility version for this process
    u16 kernel_version = 0;
    /// The default CPU for this process, threads are scheduled on this cpu by default.
    u8 ideal_processor = 0;
    /// Current status of the process
    ProcessStatus status;

    /// The id of this process
    u32 process_id = next_process_id++;

    /**
     * Parses a list of kernel capability descriptors (as found in the ExHeader) and applies them
     * to this process.
     */
    void ParseKernelCaps(const u32* kernel_caps, size_t len);

    /**
     * Applies address space changes and launches the process main thread.
     */
    void Run(s32 main_thread_priority, u32 stack_size);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Memory Management

    VMManager vm_manager;

    // Memory used to back the allocations in the regular heap. A single vector is used to cover
    // the entire virtual address space extents that bound the allocations, including any holes.
    // This makes deallocation and reallocation of holes fast and keeps process memory contiguous
    // in the emulator address space, allowing Memory::GetPointer to be reasonably safe.
    std::shared_ptr<std::vector<u8>> heap_memory;
    // The left/right bounds of the address space covered by heap_memory.
    VAddr heap_start = 0, heap_end = 0;

    u32 heap_used = 0, linear_heap_used = 0, misc_memory_used = 0;

    MemoryRegionInfo* memory_region = nullptr;

    /// The Thread Local Storage area is allocated as processes create threads,
    /// each TLS area is 0x200 bytes, so one page (0x1000) is split up in 8 parts, and each part
    /// holds the TLS for a specific thread. This vector contains which parts are in use for each
    /// page as a bitmask.
    /// This vector will grow as more pages are allocated for new threads.
    std::vector<std::bitset<8>> tls_slots;

    VAddr GetLinearHeapAreaAddress() const;
    VAddr GetLinearHeapBase() const;
    VAddr GetLinearHeapLimit() const;

    ResultVal<VAddr> HeapAllocate(VAddr target, u32 size, VMAPermission perms);
    ResultCode HeapFree(VAddr target, u32 size);

    ResultVal<VAddr> LinearAllocate(VAddr target, u32 size, VMAPermission perms);
    ResultCode LinearFree(VAddr target, u32 size);

private:
    Process();
    ~Process() override;
};

void ClearProcessList();

/// Retrieves a process from the current list of processes.
SharedPtr<Process> GetProcessById(u32 process_id);

extern SharedPtr<Process> g_current_process;
} // namespace Kernel
