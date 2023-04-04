// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/kernel/resource_limit.h"

SERIALIZE_EXPORT_IMPL(Kernel::ResourceLimit)

namespace Kernel {

ResourceLimit::ResourceLimit(KernelSystem& kernel) : Object(kernel) {}
ResourceLimit::~ResourceLimit() {}

std::shared_ptr<ResourceLimit> ResourceLimit::Create(KernelSystem& kernel, std::string name) {
    auto resource_limit{std::make_shared<ResourceLimit>(kernel)};

    resource_limit->name = std::move(name);
    return resource_limit;
}

std::shared_ptr<ResourceLimit> ResourceLimitList::GetForCategory(ResourceLimitCategory category) {
    switch (category) {
    case ResourceLimitCategory::APPLICATION:
    case ResourceLimitCategory::SYS_APPLET:
    case ResourceLimitCategory::LIB_APPLET:
    case ResourceLimitCategory::OTHER:
        return resource_limits[static_cast<u8>(category)];
    default:
        LOG_CRITICAL(Kernel, "Unknown resource limit category");
        UNREACHABLE();
    }
}

s32 ResourceLimit::GetCurrentResourceValue(u32 resource) const {
    switch (resource) {
    case COMMIT:
        return current_commit;
    case THREAD:
        return current_threads;
    case EVENT:
        return current_events;
    case MUTEX:
        return current_mutexes;
    case SEMAPHORE:
        return current_semaphores;
    case TIMER:
        return current_timers;
    case SHARED_MEMORY:
        return current_shared_mems;
    case ADDRESS_ARBITER:
        return current_address_arbiters;
    case CPU_TIME:
        return current_cpu_time;
    default:
        LOG_ERROR(Kernel, "Unknown resource type={:08X}", resource);
        UNIMPLEMENTED();
        return 0;
    }
}

u32 ResourceLimit::GetMaxResourceValue(u32 resource) const {
    switch (resource) {
    case PRIORITY:
        return max_priority;
    case COMMIT:
        return max_commit;
    case THREAD:
        return max_threads;
    case EVENT:
        return max_events;
    case MUTEX:
        return max_mutexes;
    case SEMAPHORE:
        return max_semaphores;
    case TIMER:
        return max_timers;
    case SHARED_MEMORY:
        return max_shared_mems;
    case ADDRESS_ARBITER:
        return max_address_arbiters;
    case CPU_TIME:
        return max_cpu_time;
    default:
        LOG_ERROR(Kernel, "Unknown resource type={:08X}", resource);
        UNIMPLEMENTED();
        return 0;
    }
}

ResourceLimitList::ResourceLimitList(KernelSystem& kernel) {
    // Create the four resource limits that the system uses
    // Create the APPLICATION resource limit
    const bool is_new_3ds = Settings::values.is_new_3ds.GetValue();

    std::shared_ptr<ResourceLimit> resource_limit = ResourceLimit::Create(kernel, "Applications");
    resource_limit->max_priority = 0x18;
    resource_limit->max_commit = is_new_3ds ? 0x7C00000 : 0x4000000;
    resource_limit->max_threads = 0x20;
    resource_limit->max_events = 0x20;
    resource_limit->max_mutexes = 0x20;
    resource_limit->max_semaphores = 0x8;
    resource_limit->max_timers = 0x8;
    resource_limit->max_shared_mems = 0x10;
    resource_limit->max_address_arbiters = 0x2;
    resource_limit->max_cpu_time = 0x0;
    resource_limits[static_cast<u8>(ResourceLimitCategory::APPLICATION)] = resource_limit;

    // Create the SYS_APPLET resource limit
    resource_limit = ResourceLimit::Create(kernel, "System Applets");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = is_new_3ds ? 0x5E06000 : 0x2606000;
    resource_limit->max_threads = is_new_3ds ? 0x1D : 0xE;
    resource_limit->max_events = is_new_3ds ? 0xB : 0x8;
    resource_limit->max_mutexes = 0x8;
    resource_limit->max_semaphores = 0x4;
    resource_limit->max_timers = 0x4;
    resource_limit->max_shared_mems = 0x8;
    resource_limit->max_address_arbiters = 0x3;
    resource_limit->max_cpu_time = 0x2710;
    resource_limits[static_cast<u8>(ResourceLimitCategory::SYS_APPLET)] = resource_limit;

    // Create the LIB_APPLET resource limit
    resource_limit = ResourceLimit::Create(kernel, "Library Applets");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = 0x602000;
    resource_limit->max_threads = 0xE;
    resource_limit->max_events = 0x8;
    resource_limit->max_mutexes = 0x8;
    resource_limit->max_semaphores = 0x4;
    resource_limit->max_timers = 0x4;
    resource_limit->max_shared_mems = 0x8;
    resource_limit->max_address_arbiters = 0x1;
    resource_limit->max_cpu_time = 0x2710;
    resource_limits[static_cast<u8>(ResourceLimitCategory::LIB_APPLET)] = resource_limit;

    // Create the OTHER resource limit
    resource_limit = ResourceLimit::Create(kernel, "Others");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = is_new_3ds ? 0x2182000 : 0x1682000;
    resource_limit->max_threads = is_new_3ds ? 0xE1 : 0xCA;
    resource_limit->max_events = is_new_3ds ? 0x108 : 0xF8;
    resource_limit->max_mutexes = is_new_3ds ? 0x25 : 0x23;
    resource_limit->max_semaphores = is_new_3ds ? 0x43 : 0x40;
    resource_limit->max_timers = is_new_3ds ? 0x2C : 0x2B;
    resource_limit->max_shared_mems = is_new_3ds ? 0x1F : 0x1E;
    resource_limit->max_address_arbiters = is_new_3ds ? 0x2D : 0x2B;
    resource_limit->max_cpu_time = 0x3E8;
    resource_limits[static_cast<u8>(ResourceLimitCategory::OTHER)] = resource_limit;
}

ResourceLimitList::~ResourceLimitList() = default;

} // namespace Kernel
