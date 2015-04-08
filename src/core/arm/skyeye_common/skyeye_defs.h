#pragma once

#include "common/common_types.h"

struct cpu_config_t
{
    const char* cpu_arch_name; // CPU architecture version name.e.g. ARMv4T
    const char* cpu_name;      // CPU name. e.g. ARM7TDMI or ARM720T
    u32 cpu_val;               // CPU value; also call MMU ID or processor id;see
                               // ARM Architecture Reference Manual B2-6
    u32 cpu_mask;              // cpu_val's mask.
    u32 cachetype;             // CPU cache type
};
