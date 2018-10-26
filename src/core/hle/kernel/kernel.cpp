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
}

/// Shutdown the kernel
KernelSystem::~KernelSystem() {
    Kernel::ThreadingShutdown();

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

SharedPtr<Process> KernelSystem::GetCurrentProcess() const {
    return current_process;
}

void KernelSystem::SetCurrentProcess(SharedPtr<Process> process) {
    current_process = std::move(process);
}

} // namespace Kernel
