// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/arm/interpreter/arm_interpreter.h"

const static cpu_config_t arm11_cpu_info = {
    "armv6", "arm11", 0x0007b000, 0x0007f000, NONCACHE
};

ARM_Interpreter::ARM_Interpreter()  {
    state = new ARMul_State;

    ARMul_EmulateInit();
    memset(state, 0, sizeof(ARMul_State));

    ARMul_NewState(state);

    state->abort_model = 0;
    state->cpu = (cpu_config_t*)&arm11_cpu_info;
    state->bigendSig = LOW;

    ARMul_SelectProcessor(state, ARM_v6_Prop | ARM_v5_Prop | ARM_v5e_Prop);
    state->lateabtSig = LOW;

    // Reset the core to initial state
    ARMul_CoProInit(state);
    ARMul_Reset(state);
    state->NextInstr = RESUME; // NOTE: This will be overwritten by LoadContext
    state->Emulate = 3;

    state->pc = state->Reg[15] = 0x00000000;
    state->Reg[13] = 0x10000000; // Set stack pointer to the top of the stack
    state->servaddr = 0xFFFF0000;
}

ARM_Interpreter::~ARM_Interpreter() {
    delete state;
}

void ARM_Interpreter::SetPC(u32 pc) {
    state->pc = state->Reg[15] = pc;
}

u32 ARM_Interpreter::GetPC() const {
    return state->pc;
}

u32 ARM_Interpreter::GetReg(int index) const {
    return state->Reg[index];
}

void ARM_Interpreter::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

u32 ARM_Interpreter::GetCPSR() const {
    return state->Cpsr;
}

void ARM_Interpreter::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

u64 ARM_Interpreter::GetTicks() const {
    return state->NumInstrs;
}

void ARM_Interpreter::AddTicks(u64 ticks) {
    state->NumInstrs += ticks;
}

void ARM_Interpreter::ExecuteInstructions(int num_instructions) {
    state->NumInstrsToExecute = num_instructions - 1;
    ARMul_Emulate32(state);
}

void ARM_Interpreter::SaveContext(ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, state->Reg, sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, state->ExtReg, sizeof(ctx.fpu_registers));

    ctx.sp = state->Reg[13];
    ctx.lr = state->Reg[14];
    ctx.pc = state->pc;
    ctx.cpsr = state->Cpsr;

    ctx.fpscr = state->VFP[1];
    ctx.fpexc = state->VFP[2];

    ctx.reg_15 = state->Reg[15];
    ctx.mode = state->NextInstr;
}

void ARM_Interpreter::LoadContext(const ThreadContext& ctx) {
    memcpy(state->Reg, ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(state->ExtReg, ctx.fpu_registers, sizeof(ctx.fpu_registers));

    state->Reg[13] = ctx.sp;
    state->Reg[14] = ctx.lr;
    state->pc = ctx.pc;
    state->Cpsr = ctx.cpsr;

    state->VFP[1] = ctx.fpscr;
    state->VFP[2] = ctx.fpexc;

    state->Reg[15] = ctx.reg_15;
    state->NextInstr = ctx.mode;
}

void ARM_Interpreter::PrepareReschedule() {
    state->NumInstrsToExecute = 0;
}
