// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
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
    config_mem.firm_unk = 0;
    config_mem.firm_version_rev = 0;
    config_mem.firm_version_min = 0x40;
    config_mem.firm_version_maj = 0x2;
    config_mem.firm_sys_core_ver = 0x2;
}

} // namespace
