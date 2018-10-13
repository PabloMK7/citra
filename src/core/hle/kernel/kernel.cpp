// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/config_mem.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/shared_page.h"

namespace Kernel {

/// Initialize the kernel
KernelSystem::KernelSystem(u32 system_mode) {
    ConfigMem::Init();

    Kernel::MemoryInit(system_mode);

    resource_limits = std::make_unique<ResourceLimitList>(*this);
    Kernel::ThreadingInit();
    Kernel::TimersInit();
    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    Process::next_process_id = 10;
}

/// Shutdown the kernel
KernelSystem::~KernelSystem() {
    g_handle_table.Clear(); // Free all kernel objects

    Kernel::ThreadingShutdown();
    g_current_process = nullptr;

    Kernel::TimersShutdown();
    Kernel::MemoryShutdown();
}

ResourceLimitList& KernelSystem::ResourceLimit() {
    return *resource_limits;
}

const ResourceLimitList& KernelSystem::ResourceLimit() const {
    return *resource_limits;
}

u32 KernelSystem::GenerateObjectID() {
    return next_object_id++;
}

} // namespace Kernel
