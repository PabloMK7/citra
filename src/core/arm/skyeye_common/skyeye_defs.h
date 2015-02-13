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

enum {
    // No exception
    No_exp = 0,
    // Memory allocation exception
    Malloc_exp,
    // File open exception
    File_open_exp,
    // DLL open exception
    Dll_open_exp,
    // Invalid argument exception
    Invarg_exp,
    // Invalid module exception
    Invmod_exp,
    // wrong format exception for config file parsing
    Conf_format_exp,
    // some reference excess the predefiend range. Such as the index out of array range
    Excess_range_exp,
    // Can not find the desirable result
    Not_found_exp,
    // Unknown exception
    Unknown_exp
};

typedef u32 addr_t;
