/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    core.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-04
 * @brief   Core of emulator
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "log.h"
#include "core.h"
#include "mem_map.h"
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
    g_app_core->ExecuteInstruction();
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
}

} // namespace
