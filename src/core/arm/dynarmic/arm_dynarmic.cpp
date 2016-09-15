// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/microprofile.h"

#include <dynarmic/dynarmic.h>

#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/svc.h"
#include "core/memory.h"

static void InterpreterFallback(u32 pc, Dynarmic::Jit* jit, void* user_arg) {
    ARMul_State* state = static_cast<ARMul_State*>(user_arg);

    state->Reg = jit->Regs();
    state->Cpsr = jit->Cpsr();
    state->Reg[15] = pc;
    state->ExtReg = jit->ExtRegs();
    state->VFP[VFP_FPSCR] = jit->Fpscr();
    state->NumInstrsToExecute = 1;

    InterpreterMainLoop(state);

    bool is_thumb = (state->Cpsr & (1 << 5)) != 0;
    state->Reg[15] &= (is_thumb ? 0xFFFFFFFE : 0xFFFFFFFC);

    jit->Regs() = state->Reg;
    jit->Cpsr() = state->Cpsr;
    jit->ExtRegs() = state->ExtReg;
    jit->SetFpscr(state->VFP[VFP_FPSCR]);
}

static bool IsReadOnlyMemory(u32 vaddr) {
    // TODO(bunnei): ImplementMe
    return false;
}

static Dynarmic::UserCallbacks GetUserCallbacks(ARMul_State* interpeter_state) {
    Dynarmic::UserCallbacks user_callbacks{};
    user_callbacks.InterpreterFallback = &InterpreterFallback;
    user_callbacks.user_arg = static_cast<void*>(interpeter_state);
    user_callbacks.CallSVC = &SVC::CallSVC;
    user_callbacks.IsReadOnlyMemory = &IsReadOnlyMemory;
    user_callbacks.MemoryRead8 = &Memory::Read8;
    user_callbacks.MemoryRead16 = &Memory::Read16;
    user_callbacks.MemoryRead32 = &Memory::Read32;
    user_callbacks.MemoryRead64 = &Memory::Read64;
    user_callbacks.MemoryWrite8 = &Memory::Write8;
    user_callbacks.MemoryWrite16 = &Memory::Write16;
    user_callbacks.MemoryWrite32 = &Memory::Write32;
    user_callbacks.MemoryWrite64 = &Memory::Write64;
    return user_callbacks;
}

ARM_Dynarmic::ARM_Dynarmic(PrivilegeMode initial_mode) {
    interpreter_state = std::make_unique<ARMul_State>(initial_mode);
    jit = std::make_unique<Dynarmic::Jit>(GetUserCallbacks(interpreter_state.get()));
}

void ARM_Dynarmic::SetPC(u32 pc) {
    jit->Regs()[15] = pc;
}

u32 ARM_Dynarmic::GetPC() const {
    return jit->Regs()[15];
}

u32 ARM_Dynarmic::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic::SetReg(int index, u32 value) {
    jit->Regs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPReg(int index) const {
    return jit->ExtRegs()[index];
}

void ARM_Dynarmic::SetVFPReg(int index, u32 value) {
    jit->ExtRegs()[index] = value;
}

u32 ARM_Dynarmic::GetVFPSystemReg(VFPSystemRegister reg) const {
    if (reg == VFP_FPSCR) {
        return jit->Fpscr();
    }

    // Dynarmic does not implement and/or expose other VFP registers, fallback to interpreter state
    return interpreter_state->VFP[reg];
}

void ARM_Dynarmic::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    if (reg == VFP_FPSCR) {
        jit->SetFpscr(value);
    }

    // Dynarmic does not implement and/or expose other VFP registers, fallback to interpreter state
    interpreter_state->VFP[reg] = value;
}

u32 ARM_Dynarmic::GetCPSR() const {
    return jit->Cpsr();
}

void ARM_Dynarmic::SetCPSR(u32 cpsr) {
    jit->Cpsr() = cpsr;
}

u32 ARM_Dynarmic::GetCP15Register(CP15Register reg) {
    return interpreter_state->CP15[reg];
}

void ARM_Dynarmic::SetCP15Register(CP15Register reg, u32 value) {
    interpreter_state->CP15[reg] = value;
}

void ARM_Dynarmic::AddTicks(u64 ticks) {
    down_count -= ticks;
    if (down_count < 0) {
        CoreTiming::Advance();
    }
}

MICROPROFILE_DEFINE(ARM_Jit, "ARM JIT", "ARM JIT", MP_RGB(255, 64, 64));

void ARM_Dynarmic::ExecuteInstructions(int num_instructions) {
    MICROPROFILE_SCOPE(ARM_Jit);

    jit->Run(static_cast<unsigned>(num_instructions));

    AddTicks(num_instructions);
}

void ARM_Dynarmic::SaveContext(Core::ThreadContext& ctx) {
    memcpy(ctx.cpu_registers, jit->Regs().data(), sizeof(ctx.cpu_registers));
    memcpy(ctx.fpu_registers, jit->ExtRegs().data(), sizeof(ctx.fpu_registers));

    ctx.sp = jit->Regs()[13];
    ctx.lr = jit->Regs()[14];
    ctx.pc = jit->Regs()[15];
    ctx.cpsr = jit->Cpsr();

    ctx.fpscr = jit->Fpscr();
    ctx.fpexc = interpreter_state->VFP[VFP_FPEXC];
}

void ARM_Dynarmic::LoadContext(const Core::ThreadContext& ctx) {
    memcpy(jit->Regs().data(), ctx.cpu_registers, sizeof(ctx.cpu_registers));
    memcpy(jit->ExtRegs().data(), ctx.fpu_registers, sizeof(ctx.fpu_registers));

    jit->Regs()[13] = ctx.sp;
    jit->Regs()[14] = ctx.lr;
    jit->Regs()[15] = ctx.pc;
    jit->Cpsr() = ctx.cpsr;

    jit->SetFpscr(ctx.fpscr);
    interpreter_state->VFP[VFP_FPEXC] = ctx.fpexc;
}

void ARM_Dynarmic::PrepareReschedule() {
    if (jit->IsExecuting()) {
        jit->HaltExecution();
    }
}

void ARM_Dynarmic::ClearInstructionCache() {
    jit->ClearCache();
}
