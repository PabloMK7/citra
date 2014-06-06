// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"
#include "common/symbols.h"

#include "core/core.h"
#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/interpreter/arm_interpreter.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/thread.h"

namespace Core {

u64             g_last_ticks    = 0;    ///< Last CPU ticks
ARM_Disasm*     g_disasm        = NULL; ///< ARM disassembler
ARM_Interface*  g_app_core      = NULL; ///< ARM11 application core
ARM_Interface*  g_sys_core      = NULL; ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop() {
    for (;;){
        g_app_core->Run(LCD::kFrameTicks);
        HW::Update();
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    g_app_core->Step();

    // Update and reschedule after approx. 1 frame
    u64 current_ticks = Core::g_app_core->GetTicks();
    if ((current_ticks - g_last_ticks) >= LCD::kFrameTicks || HLE::g_reschedule) {
        g_last_ticks = current_ticks;
        HW::Update();
        Kernel::Reschedule();
    }
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
