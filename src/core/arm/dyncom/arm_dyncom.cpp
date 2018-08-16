// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/arm/dyncom/arm_dyncom_trans.h"
#include "core/arm/skyeye_common/armstate.h"
#include "core/core.h"
#include "core/core_timing.h"

class DynComThreadContext final : public ARM_Interface::ThreadContext {
public:
    DynComThreadContext() {
        Reset();
    }
    ~DynComThreadContext() override = default;

    void Reset() override {
        cpu_registers = {};
        cpsr = 0;
        fpu_registers = {};
        fpscr = 0;
        fpexc = 0;
    }

    u32 GetCpuRegister(size_t index) const override {
        return cpu_registers[index];
    }
    void SetCpuRegister(size_t index, u32 value) override {
        cpu_registers[index] = value;
    }
    u32 GetCpsr() const override {
        return cpsr;
    }
    void SetCpsr(u32 value) override {
        cpsr = value;
    }
    u32 GetFpuRegister(size_t index) const override {
        return fpu_registers[index];
    }
    void SetFpuRegister(size_t index, u32 value) override {
        fpu_registers[index] = value;
    }
    u32 GetFpscr() const override {
        return fpscr;
    }
    void SetFpscr(u32 value) override {
        fpscr = value;
    }
    u32 GetFpexc() const override {
        return fpexc;
    }
    void SetFpexc(u32 value) override {
        fpexc = value;
    }

private:
    friend class ARM_DynCom;

    std::array<u32, 16> cpu_registers;
    u32 cpsr;
    std::array<u32, 64> fpu_registers;
    u32 fpscr;
    u32 fpexc;
};

ARM_DynCom::ARM_DynCom(PrivilegeMode initial_mode) {
    state = std::make_unique<ARMul_State>(initial_mode);
}

ARM_DynCom::~ARM_DynCom() {}

void ARM_DynCom::Run() {
    ExecuteInstructions(std::max<s64>(CoreTiming::GetDowncount(), 0));
}

void ARM_DynCom::Step() {
    ExecuteInstructions(1);
}

void ARM_DynCom::ClearInstructionCache() {
    state->instruction_cache.clear();
    trans_cache_buf_top = 0;
}

void ARM_DynCom::InvalidateCacheRange(u32, size_t) {
    ClearInstructionCache();
}

void ARM_DynCom::PageTableChanged() {
    ClearInstructionCache();
}

void ARM_DynCom::SetPC(u32 pc) {
    state->Reg[15] = pc;
}

u32 ARM_DynCom::GetPC() const {
    return state->Reg[15];
}

u32 ARM_DynCom::GetReg(int index) const {
    return state->Reg[index];
}

void ARM_DynCom::SetReg(int index, u32 value) {
    state->Reg[index] = value;
}

u32 ARM_DynCom::GetVFPReg(int index) const {
    return state->ExtReg[index];
}

void ARM_DynCom::SetVFPReg(int index, u32 value) {
    state->ExtReg[index] = value;
}

u32 ARM_DynCom::GetVFPSystemReg(VFPSystemRegister reg) const {
    return state->VFP[reg];
}

void ARM_DynCom::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    state->VFP[reg] = value;
}

u32 ARM_DynCom::GetCPSR() const {
    return state->Cpsr;
}

void ARM_DynCom::SetCPSR(u32 cpsr) {
    state->Cpsr = cpsr;
}

u32 ARM_DynCom::GetCP15Register(CP15Register reg) {
    return state->CP15[reg];
}

void ARM_DynCom::SetCP15Register(CP15Register reg, u32 value) {
    state->CP15[reg] = value;
}

void ARM_DynCom::ExecuteInstructions(u64 num_instructions) {
    state->NumInstrsToExecute = num_instructions;
    unsigned ticks_executed = InterpreterMainLoop(state.get());
    CoreTiming::AddTicks(ticks_executed);
    state.get()->ServeBreak();
}

std::unique_ptr<ARM_Interface::ThreadContext> ARM_DynCom::NewContext() const {
    return std::make_unique<DynComThreadContext>();
}

void ARM_DynCom::SaveContext(const std::unique_ptr<ThreadContext>& arg) {
    DynComThreadContext* ctx = dynamic_cast<DynComThreadContext*>(arg.get());
    ASSERT(ctx);

    ctx->cpu_registers = state->Reg;
    ctx->cpsr = state->Cpsr;
    ctx->fpu_registers = state->ExtReg;
    ctx->fpscr = state->VFP[VFP_FPSCR];
    ctx->fpexc = state->VFP[VFP_FPEXC];
}

void ARM_DynCom::LoadContext(const std::unique_ptr<ThreadContext>& arg) {
    DynComThreadContext* ctx = dynamic_cast<DynComThreadContext*>(arg.get());
    ASSERT(ctx);

    state->Reg = ctx->cpu_registers;
    state->Cpsr = ctx->cpsr;
    state->ExtReg = ctx->fpu_registers;
    state->VFP[VFP_FPSCR] = ctx->fpscr;
    state->VFP[VFP_FPEXC] = ctx->fpexc;
}

void ARM_DynCom::PrepareReschedule() {
    state->NumInstrsToExecute = 0;
}
