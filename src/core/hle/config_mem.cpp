// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/common_funcs.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hle/config_mem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ConfigMem {

struct ConfigMemDef {
    u8  kernel_unk;                      // 0
    u8  kernel_version_rev;              // 1
    u8  kernel_version_min;              // 2
    u8  kernel_version_maj;              // 3
    u32 update_flag;                     // 4
    u64 ns_tid;                          // 8
    u32 sys_core_ver;                    // 10
    u8  unit_info;                       // 14
    u8  boot_firm;                       // 15
    u8  prev_firm;                       // 16
    INSERT_PADDING_BYTES(0x1);           // 17
    u32 ctr_sdk_ver;                     // 18
    INSERT_PADDING_BYTES(0x30 - 0x1C);   // 1C
    u32 app_mem_type;                    // 30
    INSERT_PADDING_BYTES(0x40 - 0x34);   // 34
    u32 app_mem_alloc;                   // 40
    u32 sys_mem_alloc;                   // 44
    u32 base_mem_alloc;                  // 48
    INSERT_PADDING_BYTES(0x60 - 0x4C);   // 4C
    u8  firm_unk;                        // 60
    u8  firm_version_rev;                // 61
    u8  firm_version_min;                // 62
    u8  firm_version_maj;                // 63
    u32 firm_sys_core_ver;               // 64
    u32 firm_ctr_sdk_ver;                // 68
    INSERT_PADDING_BYTES(0x1000 - 0x6C); // 6C
};

static_assert(sizeof(ConfigMemDef) == Memory::CONFIG_MEMORY_SIZE, "Config Memory structure size is wrong");

static ConfigMemDef config_mem;

template <typename T>
inline void Read(T &var, const u32 addr) {
    u32 offset = addr - Memory::CONFIG_MEMORY_VADDR;
    ASSERT(offset < Memory::CONFIG_MEMORY_SIZE);
    var = *(reinterpret_cast<T*>(((uintptr_t)&config_mem) + offset));
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

void Init() {
    std::memset(&config_mem, 0, sizeof(config_mem));

    config_mem.update_flag = 0; // No update
    config_mem.sys_core_ver = 0x2;
    config_mem.unit_info = 0x1; // Bit 0 set for Retail
    config_mem.prev_firm = 0;
    config_mem.app_mem_type = 0x2; // Default app mem type is 0
    config_mem.app_mem_alloc = 0x06000000; // Set to 96MB, since some games use more than the default (64MB)
    config_mem.base_mem_alloc = 0x01400000; // Default base memory is 20MB
    config_mem.sys_mem_alloc = Memory::FCRAM_SIZE - (config_mem.app_mem_alloc + config_mem.base_mem_alloc);
    config_mem.firm_unk = 0;
    config_mem.firm_version_rev = 0;
    config_mem.firm_version_min = 0x40;
    config_mem.firm_version_maj = 0x2;
    config_mem.firm_sys_core_ver = 0x2;
}

void Shutdown() {
}

} // namespace
