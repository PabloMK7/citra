// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/arm/interpreter/arm_interpreter.h"

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

ARM_Interpreter::~ARM_Interpreter() {
    delete m_state;
}

/**
 * Set the Program Counter to an address
 * @param addr Address to set PC to
 */
void ARM_Interpreter::SetPC(u32 pc) {
    m_state->pc = m_state->Reg[15] = pc;
}

/*
 * Get the current Program Counter
 * @return Returns current PC
 */
u32 ARM_Interpreter::GetPC() const {
    return m_state->pc;
}

/**
 * Get an ARM register
 * @param index Register index (0-15)
 * @return Returns the value in the register
 */
u32 ARM_Interpreter::GetReg(int index) const {
    return m_state->Reg[index];
}

/**
 * Set an ARM register
 * @param index Register index (0-15)
 * @param value Value to set register to
 */
void ARM_Interpreter::SetReg(int index, u32 value) {
    m_state->Reg[index] = value;
}

/**
 * Get the current CPSR register
 * @return Returns the value of the CPSR register
 */
u32 ARM_Interpreter::GetCPSR() const {
    return m_state->Cpsr;
}

/**
 * Set the current CPSR register
 * @param cpsr Value to set CPSR to
 */
void ARM_Interpreter::SetCPSR(u32 cpsr) {
    m_state->Cpsr = cpsr;
}

/**
 * Returns the number of clock ticks since the last reset
 * @return Returns number of clock ticks
 */
u64 ARM_Interpreter::GetTicks() const {
    return ARMul_Time(m_state);
}

/**
 * Executes the given number of instructions
 * @param num_instructions Number of instructions to executes
 */
void ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    m_state->NumInstrsToExecute = num_instructions;
    ARMul_Emulate32(m_state);
}

/**
 * Saves the current CPU context
 * @param ctx Thread context to save
 * @todo Do we need to save Reg[15] and NextInstr?
 */
void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, m_state->Reg, sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, m_state->ExtReg, sizeof(ctx.fpu_registers));

    ctx.sp = m_state->Reg[13];
    ctx.lr = m_state->Reg[14];
    ctx.pc = m_state->pc;
    ctx.cpsr = m_state->Cpsr;
    
    ctx.fpscr = m_state->VFP[1];
    ctx.fpexc = m_state->VFP[2];
}

/**
 * Loads a CPU context
 * @param ctx Thread context to load
 * @param Do we need to load Reg[15] and NextInstr?
 */
void ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    memcpy(m_state->Reg, ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(m_state->ExtReg, ctx.fpu_registers, sizeof(ctx.fpu_registers));

    m_state->Reg[13] = ctx.sp;
    m_state->Reg[14] = ctx.lr;
    m_state->pc = ctx.pc;
    m_state->Cpsr = ctx.cpsr;

    m_state->VFP[1] = ctx.fpscr;
    m_state->VFP[2] = ctx.fpexc;
}
