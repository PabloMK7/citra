// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "core/arm/interpreter/arm_interpreter.h"

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

ARM_Interpreter::~ARM_Interpreter() {
    delete state;
}

/**
 * Set the Program Counter to an address
 * @param addr Address to set PC to
 */
void ARM_Interpreter::SetPC(u32 pc) {
    state->pc = state->Reg[15] = pc;
}

/*
 * Get the current Program Counter
 * @return Returns current PC
 */
u32 ARM_Interpreter::GetPC() const {
    return state->pc;
}

/**
 * Get an ARM register
 * @param index Register index (0-15)
 * @return Returns the value in the register
 */
u32 ARM_Interpreter::GetReg(int index) const {
    return state->Reg[index];
}

/**
 * Set an ARM register
 * @param index Register index (0-15)
 * @param value Value to set register to
 */
void ARM_Interpreter::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

/**
 * Get the current CPSR register
 * @return Returns the value of the CPSR register
 */
u32 ARM_Interpreter::GetCPSR() const {
    return state->Cpsr;
}

/**
 * Set the current CPSR register
 * @param cpsr Value to set CPSR to
 */
void ARM_Interpreter::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

/**
 * Returns the number of clock ticks since the last reset
 * @return Returns number of clock ticks
 */
u64 ARM_Interpreter::GetTicks() const {
    return ARMul_Time(state);
}

/**
 * Executes the given number of instructions
 * @param num_instructions Number of instructions to executes
 */
void ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    state->NumInstrsToExecute = num_instructions;
    ARMul_Emulate32(state);
}

/**
 * Saves the current CPU context
 * @param ctx Thread context to save
 * @todo Do we need to save Reg[15] and NextInstr?
 */
void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->Reg, sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->ExtReg, sizeof(ctx.fpu_registers));

    ctx.sp = state->Reg[13];
    ctx.lr = state->Reg[14];
    ctx.pc = state->pc;
    ctx.cpsr = state->Cpsr;

    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];
}

/**
 * Loads a CPU context
 * @param ctx Thread context to load
 * @param Do we need to load Reg[15] and NextInstr?
 */
void ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    memcpy(state->Reg, ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(state->ExtReg, ctx.fpu_registers, sizeof(ctx.fpu_registers));

    state->Reg[13] = ctx.sp;
    state->Reg[14] = ctx.lr;
    state->pc = ctx.pc;
    state->Cpsr = ctx.cpsr;

    state->VFP[1] = ctx.fpscr;
    state->VFP[2] = ctx.fpexc;

    state->Reg[15] = ctx.pc;
    state->NextInstr = RESUME;
}
