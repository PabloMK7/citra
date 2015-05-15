// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/common_funcs.h"

#include "core/core.h"
#include "core/memory.h"
#include "core/hle/config_mem.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ConfigMem {

ConfigMemDef config_mem;

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
