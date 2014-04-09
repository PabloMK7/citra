// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "arm_interpreter.h"

const static cpu_config_t s_arm11_cpu_info = {
    "armv6", "arm11", 0x0007b000, 0x0007f000, NONCACHE
};

ARM_Interpreter::ARM_Interpreter()  {
    m_state = new ARMul_State;

    ARMul_EmulateInit();
    ARMul_NewState(m_state);

    m_state->abort_model = 0;
    m_state->cpu = (cpu_config_t*)&s_arm11_cpu_info;
    m_state->bigendSig = LOW;

    ARMul_SelectProcessor(m_state, ARM_v6_Prop | ARM_v5_Prop | ARM_v5e_Prop);
    m_state->lateabtSig = LOW;
    mmu_init(m_state);

    // Reset the core to initial state
    ARMul_Reset(m_state);
    m_state->NextInstr = 0;
    m_state->Emulate = 3;

    m_state->pc = m_state->Reg[15] = 0x00000000;
    m_state->Reg[13] = 0x10000000; // Set stack pointer to the top of the stack
}

void ARM_Interpreter::SetPC(u32 pc) {
    m_state->pc = m_state->Reg[15] = pc;
}

u32 ARM_Interpreter::GetPC() const {
    return m_state->pc;
}

u32 ARM_Interpreter::GetReg(int index) const {
    return m_state->Reg[index];
}

u32 ARM_Interpreter::GetCPSR() const {
    return m_state->Cpsr;
}

u64 ARM_Interpreter::GetTicks() const {
    return ARMul_Time(m_state);
}

ARM_Interpreter::~ARM_Interpreter() {
    delete m_state;
}

void ARM_Interpreter::ExecuteInstruction() {
    m_state->step++;
    m_state->cycle++;
    m_state->EndCondition = 0;
    m_state->stop_simulator = 0;
    m_state->NextInstr = RESUME;
    m_state->last_pc = m_state->Reg[15];
    m_state->Reg[15] = ARMul_DoInstr(m_state);
    m_state->Cpsr = ((m_state->Cpsr & 0x0fffffdf) | (m_state->NFlag << 31) | (m_state->ZFlag << 30) | 
                   (m_state->CFlag << 29) | (m_state->VFlag << 28) | (m_state->TFlag << 5));
    m_state->NextInstr |= PRIMEPIPE; // Flush pipe
}
