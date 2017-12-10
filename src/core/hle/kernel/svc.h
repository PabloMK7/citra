// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {

struct MemoryInfo {
    u32 base_address;
    u32 size;
    u32 permission;
    u32 state;
};

struct PageInfo {
    u32 flags;
};

/// Values accepted by svcGetSystemInfo's type parameter.
enum class SystemInfoType {
    /**
     * Reports total used memory for all regions or a specific one, according to the extra
     * parameter. See `SystemInfoMemUsageRegion`.
     */
    REGION_MEMORY_USAGE = 0,
    /**
     * Returns the memory usage for certain allocations done internally by the kernel.
     */
    KERNEL_ALLOCATED_PAGES = 2,
    /**
     * "This returns the total number of processes which were launched directly by the kernel.
     * For the ARM11 NATIVE_FIRM kernel, this is 5, for processes sm, fs, pm, loader, and pxi."
     */
    KERNEL_SPAWNED_PIDS = 26,
};

/**
 * Accepted by svcGetSystemInfo param with REGION_MEMORY_USAGE type. Selects a region to query
 * memory usage of.
 */
enum class SystemInfoMemUsageRegion {
    ALL = 0,
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

void CallSVC(u32 immediate);

} // namespace Kernel
