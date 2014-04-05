/**
* Copyright (C) 2013 Citrus Emulator
*
* @file    arm_interpreter.h
* @author  bunnei
* @date    2014-04-04
* @brief   ARM interface instance for SkyEye interprerer
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

#include "arm_interpreter.h"

const static cpu_config_t s_arm11_cpu_info = {
    "armv6", "arm11", 0x0007b000, 0x0007f000, NONCACHE
};

ARM_Interpreter::ARM_Interpreter()  {

    state = new ARMul_State;

    ARMul_EmulateInit();
    ARMul_NewState(state);

    state->abort_model = 0;
    state->cpu = (cpu_config_t*)&s_arm11_cpu_info;
    state->bigendSig = LOW;

    ARMul_SelectProcessor(state, ARM_v6_Prop | ARM_v5_Prop | ARM_v5e_Prop);
    state->lateabtSig = LOW;
    mmu_init(state);

    // Reset the core to initial state
    ARMul_Reset(state);
    state->NextInstr = 0;
    state->Emulate = 3;

    state->pc = state->Reg[15] = 0x00000000;
    state->Reg[13] = 0x10000000; // Set stack pointer to the top of the stack
}

void ARM_Interpreter::SetPC(u32 pc) {
    state->pc = state->Reg[15] = pc;
}

u32 ARM_Interpreter::PC() {
    return state->pc;
}

u32 ARM_Interpreter::Reg(int index){
    return state->Reg[index];
}

u32 ARM_Interpreter::CPSR() {
    return state->Cpsr;
}

ARM_Interpreter::~ARM_Interpreter() {
    delete state;
}

void ARM_Interpreter::ExecuteInstruction() {
    state->step++;
    state->cycle++;
    state->EndCondition = 0;
    state->stop_simulator = 0;
    state->NextInstr = RESUME;
    state->last_pc = state->Reg[15];
    state->Reg[15] = ARMul_DoInstr(state);
    state->Cpsr = ((state->Cpsr & 0x0fffffdf) | (state->NFlag << 31) | (state->ZFlag << 30) | 
                   (state->CFlag << 29) | (state->VFlag << 28) | (state->TFlag << 5));
    FLUSHPIPE;
}
