// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"
#include "common/symbols.h"

#include "video_core/video_core.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "core/hw/gpu.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/interpreter/arm_interpreter.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"

namespace Core {

u64             g_last_ticks    = 0;        ///< Last CPU ticks
ARM_Disasm*     g_disasm        = nullptr;  ///< ARM disassembler
ARM_Interface*  g_app_core      = nullptr;  ///< ARM11 application core
ARM_Interface*  g_sys_core      = nullptr;  ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop(int tight_loop) {
    g_app_core->Run(tight_loop);
    HW::Update();
    if (HLE::g_reschedule) {
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    RunLoop(1);
}

/// Halt the core
void Halt(const char *msg) {
    // TODO(ShizZy): ImplementMe
}

/// Kill the core
void Stop() {
    // TODO(ShizZy): ImplementMe
}

/// Initialize the core
int Init() {
    NOTICE_LOG(MASTER_LOG, "initialized OK");

    g_disasm = new ARM_Disasm();
    g_app_core = new ARM_Interpreter();
    g_sys_core = new ARM_Interpreter();

    g_last_ticks = Core::g_app_core->GetTicks();

    return 0;
}

void Shutdown() {
    delete g_disasm;
    delete g_app_core;
    delete g_sys_core;

    NOTICE_LOG(MASTER_LOG, "shutdown OK");
}

} // namespace
