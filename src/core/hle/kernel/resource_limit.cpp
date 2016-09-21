// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/logging/log.h"
#include "core/hle/kernel/resource_limit.h"

namespace Kernel {

static SharedPtr<ResourceLimit> resource_limits[4];

ResourceLimit::ResourceLimit() {}
ResourceLimit::~ResourceLimit() {}

SharedPtr<ResourceLimit> ResourceLimit::Create(std::string name) {
    SharedPtr<ResourceLimit> resource_limit(new ResourceLimit);

    resource_limit->name = std::move(name);
    return resource_limit;
}

SharedPtr<ResourceLimit> ResourceLimit::GetForCategory(ResourceLimitCategory category) {
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
        LOG_ERROR(Kernel, "Unknown resource type=%08X", resource);
        UNIMPLEMENTED();
        return 0;
    }
}

s32 ResourceLimit::GetMaxResourceValue(u32 resource) const {
    switch (resource) {
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
        LOG_ERROR(Kernel, "Unknown resource type=%08X", resource);
        UNIMPLEMENTED();
        return 0;
    }
}

void ResourceLimitsInit() {
    // Create the four resource limits that the system uses
    // Create the APPLICATION resource limit
    SharedPtr<ResourceLimit> resource_limit = ResourceLimit::Create("Applications");
    resource_limit->max_priority = 0x18;
    resource_limit->max_commit = 0x4000000;
    resource_limit->max_threads = 0x20;
    resource_limit->max_events = 0x20;
    resource_limit->max_mutexes = 0x20;
    resource_limit->max_semaphores = 0x8;
    resource_limit->max_timers = 0x8;
    resource_limit->max_shared_mems = 0x10;
    resource_limit->max_address_arbiters = 0x2;
    resource_limit->max_cpu_time = 0x1E;
    resource_limits[static_cast<u8>(ResourceLimitCategory::APPLICATION)] = resource_limit;

    // Create the SYS_APPLET resource limit
    resource_limit = ResourceLimit::Create("System Applets");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = 0x5E00000;
    resource_limit->max_threads = 0x1D;
    resource_limit->max_events = 0xB;
    resource_limit->max_mutexes = 0x8;
    resource_limit->max_semaphores = 0x4;
    resource_limit->max_timers = 0x4;
    resource_limit->max_shared_mems = 0x8;
    resource_limit->max_address_arbiters = 0x3;
    resource_limit->max_cpu_time = 0x2710;
    resource_limits[static_cast<u8>(ResourceLimitCategory::SYS_APPLET)] = resource_limit;

    // Create the LIB_APPLET resource limit
    resource_limit = ResourceLimit::Create("Library Applets");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = 0x600000;
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
    resource_limit = ResourceLimit::Create("Others");
    resource_limit->max_priority = 0x4;
    resource_limit->max_commit = 0x2180000;
    resource_limit->max_threads = 0xE1;
    resource_limit->max_events = 0x108;
    resource_limit->max_mutexes = 0x25;
    resource_limit->max_semaphores = 0x43;
    resource_limit->max_timers = 0x2C;
    resource_limit->max_shared_mems = 0x1F;
    resource_limit->max_address_arbiters = 0x2D;
    resource_limit->max_cpu_time = 0x3E8;
    resource_limits[static_cast<u8>(ResourceLimitCategory::OTHER)] = resource_limit;
}

void ResourceLimitsShutdown() {}

} // namespace
