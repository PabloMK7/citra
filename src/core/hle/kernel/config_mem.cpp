// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "core/hle/kernel/config_mem.h"

namespace ConfigMem {

Handler::Handler() {
    std::memset(&config_mem, 0, sizeof(config_mem));

    // Values extracted from firmware 11.17.0-50E
    config_mem.kernel_version_min = 0x3a;
    config_mem.kernel_version_maj = 0x2;
    config_mem.ns_tid = 0x0004013000008002;
    config_mem.sys_core_ver = 0x2;
    config_mem.unit_info = 0x1; // Bit 0 set for Retail
    config_mem.prev_firm = 0x1;
    config_mem.ctr_sdk_ver = 0x0000F450;
    config_mem.firm_version_min = 0x3a;
    config_mem.firm_version_maj = 0x2;
    config_mem.firm_sys_core_ver = 0x2;
    config_mem.firm_ctr_sdk_ver = 0x0000F450;
}

ConfigMemDef& Handler::GetConfigMem() {
    return config_mem;
}

} // namespace ConfigMem
