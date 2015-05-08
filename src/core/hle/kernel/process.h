// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include <boost/container/static_vector.hpp>

#include "common/bit_field.h"
#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/result.h"

namespace Kernel {

struct StaticAddressMapping {
    // Address and size must be page-aligned
    VAddr address;
    u32 size;
    bool writable;
    bool unk_flag;
};

enum class MemoryRegion : u16 {
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

union ProcessFlags {
    u16 raw;

    BitField< 0, 1, u16> allow_debug; ///< Allows other processes to attach to and debug this process.
    BitField< 1, 1, u16> force_debug; ///< Allows this process to attach to processes even if they don't have allow_debug set.
    BitField< 2, 1, u16> allow_nonalphanum;
    BitField< 3, 1, u16> shared_page_writable; ///< Shared page is mapped with write permissions.
    BitField< 4, 1, u16> privileged_priority; ///< Can use priority levels higher than 24.
    BitField< 5, 1, u16> allow_main_args;
    BitField< 6, 1, u16> shared_device_mem;
    BitField< 7, 1, u16> runnable_on_sleep;
    BitField< 8, 4, MemoryRegion> memory_region; ///< Default region for memory allocations for this process
    BitField<12, 1, u16> loaded_high; ///< Application loaded high (not at 0x00100000).
};

class Process final : public Object {
public:
    static SharedPtr<Process> Create(std::string name, u64 program_id);

    std::string GetTypeName() const override { return "Process"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::Process;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    std::string name; ///< Name of the process
    u64 program_id;

    std::bitset<0x80> svc_access_mask;
    unsigned int handle_table_size = 0x200;
    boost::container::static_vector<StaticAddressMapping, 8> static_address_mappings; // TODO: Determine a good upper limit
    ProcessFlags flags;

    void ParseKernelCaps(const u32* kernel_caps, size_t len);
    void Run(VAddr entry_point, s32 main_thread_priority, u32 stack_size);

private:
    Process();
    ~Process() override;
};

extern SharedPtr<Process> g_current_process;

}
