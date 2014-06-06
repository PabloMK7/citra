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

ARM_Disasm*     g_disasm    = NULL; ///< ARM disassembler
ARM_Interface*  g_app_core  = NULL; ///< ARM11 application core
ARM_Interface*  g_sys_core  = NULL; ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop() {
    for (;;){
        g_app_core->Run(LCD::kFrameTicks / 3);
        HW::Update();
        Kernel::Reschedule();
    }
}

/// Step the CPU one instruction
void SingleStep() {
    static int ticks = 0;

    g_app_core->Step();
    
    if ((ticks >= LCD::kFrameTicks / 3) || HLE::g_reschedule) {
        HW::Update();
        Kernel::Reschedule();
        ticks = 0;
    } else {
        ticks++;
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

    return 0;
}

void Shutdown() {
    delete g_disasm;
    delete g_app_core;
    delete g_sys_core;

    NOTICE_LOG(MASTER_LOG, "shutdown OK");
}

} // namespace
