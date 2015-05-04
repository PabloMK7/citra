// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"

#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

SharedPtr<Process> Process::Create(std::string name, u64 program_id) {
    SharedPtr<Process> process(new Process);

    process->svc_access_mask.set();
    process->name = std::move(name);
    process->program_id = program_id;

    return process;
}

void Process::ParseKernelCaps(const u32 * kernel_caps, size_t len) {
    //UNIMPLEMENTED();
}

void Process::Run(VAddr entry_point, s32 main_thread_priority, u32 stack_size) {
    g_main_thread = Kernel::SetupMainThread(stack_size, entry_point, main_thread_priority);
}

Kernel::Process::Process() {}
Kernel::Process::~Process() {}

SharedPtr<Process> g_current_process;

}
