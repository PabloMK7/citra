// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/log.h"
#include "core/core.h"

#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/interpreter/arm_interpreter.h"

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

    char current_instr[512];

    if (g_app_core->GetPC() == 0x080D1534) {
        g_disasm->disasm(g_app_core->GetPC(), Memory::Read32(g_app_core->GetPC()), current_instr);


        NOTICE_LOG(ARM11, "0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
            g_app_core->GetReg(0),
            g_app_core->GetReg(1),
            g_app_core->GetReg(2),
            g_app_core->GetReg(3), Memory::Read32(g_app_core->GetReg(0)), Memory::Read32(g_app_core->GetReg(1)));


        NOTICE_LOG(ARM11, "0x%08X\t%s", g_app_core->GetPC(), current_instr);
    }


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
