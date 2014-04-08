// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "log.h"
#include "core.h"
#include "mem_map.h"
#include "hw/hw.h"
#include "arm/disassembler/arm_disasm.h"
#include "arm/interpreter/arm_interpreter.h"

namespace Core {

ARM_Disasm*     g_disasm    = NULL; ///< ARM disassembler
ARM_Interface*  g_app_core  = NULL; ///< ARM11 application core
ARM_Interface*  g_sys_core  = NULL; ///< ARM11 system (OS) core

/// Run the core CPU loop
void RunLoop() {
    // TODO(ShizZy): ImplementMe
}

/// Step the CPU one instruction
void SingleStep() {
    g_app_core->Step();
    HW::Update();
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
    NOTICE_LOG(MASTER_LOG, "Core initialized OK");

    g_disasm = new ARM_Disasm();
    g_app_core = new ARM_Interpreter();
    g_sys_core = new ARM_Interpreter();

    return 0;
}

void Shutdown() {
    delete g_disasm;
    delete g_app_core;
    delete g_sys_core;

    NOTICE_LOG(MASTER_LOG, "Core shutdown OK");
}

} // namespace
