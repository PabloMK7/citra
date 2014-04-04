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
#include "arm/armdefs.h"
#include "arm/disassembler/arm_disasm.h"

namespace Core {

typedef struct arm11_core{
    conf_object_t* obj;
    ARMul_State* state;
    memory_space_intf* space;
}arm11_core_t;

arm11_core* core = NULL;

Arm* disasm = NULL;

//ARMul_State* g_arm_state = NULL;

/// Start the core
void Start() {
    // TODO(ShizZy): ImplementMe
}

/// Run the core CPU loop
void RunLoop() {
    // TODO(ShizZy): ImplementMe
}

/// Step the CPU one instruction
void SingleStep() {
    //arm11_core_t* core = (arm11_core_t*)opaque->obj;
    ARMul_State *state = core->state;
    //if (state->space.conf_obj == NULL){
    //    state->space.conf_obj = core->space->conf_obj;
    //    state->space.read = core->space->read;
    //    state->space.write = core->space->write;
    //}

    char next_instr[255];

    disasm->disasm(state->pc, Memory::Read32(state->pc), next_instr);

    NOTICE_LOG(ARM11, "0x%08X : %s", state->pc, next_instr);


    for (int i = 0; i < 15; i++) {
        NOTICE_LOG(ARM11, "Reg[%02d] = 0x%08X", i, state->Reg[i]);
    }


    state->step++;
    state->cycle++;
    state->EndCondition = 0;
    state->stop_simulator = 0;
    //state->NextInstr = RESUME;      /* treat as PC change */
    state->last_pc = state->Reg[15];
    state->Reg[15] = ARMul_DoInstr(state);
    state->Cpsr = (state->Cpsr & 0x0fffffdf) | \
        (state->NFlag << 31) | \
        (state->ZFlag << 30) | \
        (state->CFlag << 29) | \
        (state->VFlag << 28);// | \
        //(state->TFlag << 5);

    //FLUSHPIPE;
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
const static cpu_config_t arm11_cpu_info = { "armv6", "arm11", 0x0007b000, 0x0007f000, NONCACHE };
int Init() {
    NOTICE_LOG(MASTER_LOG, "Core initialized OK");

    disasm = new Arm();
    core = (arm11_core_t*)malloc(sizeof(arm11_core_t));
    //core->obj = new_conf_object(obj_name, core);
    ARMul_EmulateInit();
    ARMul_State* state = new ARMul_State;
    ARMul_NewState(state);
    state->abort_model = 0;
    state->cpu = (cpu_config_t*)&arm11_cpu_info;
    state->bigendSig = LOW;

    ARMul_SelectProcessor(state, ARM_v6_Prop | ARM_v5_Prop | ARM_v5e_Prop);
    state->lateabtSig = LOW;
    mmu_init(state);
    /* reset the core to initial state */
    ARMul_Reset(state);
    state->NextInstr = 0;
    state->Emulate = 3;
#if 0
    state->mmu.ops.read_byte = arm11_read_byte;
    state->mmu.ops.read_halfword = arm11_read_halfword;
    state->mmu.ops.read_word = arm11_read_word;
    state->mmu.ops.write_byte = arm11_write_byte;
    state->mmu.ops.write_halfword = arm11_write_halfword;
    state->mmu.ops.write_word = arm11_write_word;
#endif
    core->state = state;

    state->pc = state->Reg[15] = 0x080c3ee0; // Hardcoded set PC to start address of a homebrew ROM
                                             // this is where most launcher.dat code loads /bunnei

    state->Reg[13] = 0x10000000; // Set stack pointer to the top of the stack, not sure if this is 
                                    // right? /bunnei

    //state->s
    return 0;
}

ARMul_State* GetState()
{
    return core->state;
}

void Shutdown() {
    //delete g_arm_state;
    //g_arm_state = NULL;
}

} // namespace
