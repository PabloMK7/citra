// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/config_mem.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

/// Initialize the kernel
KernelSystem::KernelSystem(Memory::MemorySystem& memory, u32 system_mode) : memory(memory) {
    MemoryInit(system_mode);

    resource_limits = std::make_unique<ResourceLimitList>(*this);
    thread_manager = std::make_unique<ThreadManager>();
    timer_manager = std::make_unique<TimerManager>();
}

/// Shutdown the kernel
KernelSystem::~KernelSystem() = default;

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

ThreadManager& KernelSystem::GetThreadManager() {
    return *thread_manager;
}

const ThreadManager& KernelSystem::GetThreadManager() const {
    return *thread_manager;
}

TimerManager& KernelSystem::GetTimerManager() {
    return *timer_manager;
}

const TimerManager& KernelSystem::GetTimerManager() const {
    return *timer_manager;
}

SharedPage::Handler& KernelSystem::GetSharedPageHandler() {
    return *shared_page_handler;
}

const SharedPage::Handler& KernelSystem::GetSharedPageHandler() const {
    return *shared_page_handler;
}

void KernelSystem::AddNamedPort(std::string name, SharedPtr<ClientPort> port) {
    named_ports.emplace(std::move(name), std::move(port));
}

} // namespace Kernel
