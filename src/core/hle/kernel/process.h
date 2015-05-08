// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include <boost/container/static_vector.hpp>

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

enum class MemoryRegion {
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
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

    bool loaded_high = false; // Application loaded high (not at 0x00100000)
    bool shared_page_writable = false;
    bool privileged_priority = false; // Can use priority levels higher than 24
    MemoryRegion memory_region = MemoryRegion::APPLICATION;

    void ParseKernelCaps(const u32* kernel_caps, size_t len);
    void Run(VAddr entry_point, s32 main_thread_priority, u32 stack_size);

private:
    Process();
    ~Process() override;
};

extern SharedPtr<Process> g_current_process;

}
