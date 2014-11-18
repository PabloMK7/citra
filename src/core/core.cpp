// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/core.h"

#include "core/settings.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/interpreter/arm_interpreter.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"
#include "core/hw/hw.h"

namespace Core {

static u64         last_ticks = 0;        ///< Last CPU ticks
static ARM_Disasm* disasm     = nullptr;  ///< ARM disassembler
ARM_Interface*     g_app_core = nullptr;  ///< ARM11 application core
ARM_Interface*     g_sys_core = nullptr;  ///< ARM11 system (OS) core

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

    disasm = new ARM_Disasm();
    g_sys_core = new ARM_Interpreter();

    switch (Settings::values.cpu_core) {
        case CPU_FastInterpreter:
            g_app_core = new ARM_DynCom();
            break;
        case CPU_Interpreter:
        default:
            g_app_core = new ARM_Interpreter();
            break;
    }

    last_ticks = Core::g_app_core->GetTicks();

    return 0;
}

void Shutdown() {
    delete disasm;
    delete g_app_core;
    delete g_sys_core;

    NOTICE_LOG(MASTER_LOG, "shutdown OK");
}

} // namespace
