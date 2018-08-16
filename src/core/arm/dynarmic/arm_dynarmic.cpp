// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <dynarmic/context.h>
#include <dynarmic/dynarmic.h>
#include "common/assert.h"
#include "common/microprofile.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"

class DynarmicThreadContext final : public ARM_Interface::ThreadContext {
public:
    DynarmicThreadContext() {
        Reset();
    }
    ~DynarmicThreadContext() override = default;

    void Reset() override {
        ctx.Regs() = {};
        ctx.SetCpsr(0);
        ctx.ExtRegs() = {};
        ctx.SetFpscr(0);
        fpexc = 0;
    }

    u32 GetCpuRegister(size_t index) const override {
        return ctx.Regs()[index];
    }
    void SetCpuRegister(size_t index, u32 value) override {
        ctx.Regs()[index] = value;
    }
    u32 GetCpsr() const override {
        return ctx.Cpsr();
    }
    void SetCpsr(u32 value) override {
        ctx.SetCpsr(value);
    }
    u32 GetFpuRegister(size_t index) const override {
        return ctx.ExtRegs()[index];
    }
    void SetFpuRegister(size_t index, u32 value) override {
        ctx.ExtRegs()[index] = value;
    }
    u32 GetFpscr() const override {
        return ctx.Fpscr();
    }
    void SetFpscr(u32 value) override {
        ctx.SetFpscr(value);
    }
    u32 GetFpexc() const override {
        return fpexc;
    }
    void SetFpexc(u32 value) override {
        fpexc = value;
    }

private:
    friend class ARM_Dynarmic;

    Dynarmic::Context ctx;
    u32 fpexc;
};

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
    jit->SetCpsr(state->Cpsr);
    jit->ExtRegs() = state->ExtReg;
    jit->SetFpscr(state->VFP[VFP_FPSCR]);

    state->ServeBreak();
}

static bool IsReadOnlyMemory(u32 vaddr) {
    // TODO(bunnei): ImplementMe
    return false;
}

static void AddTicks(u64 ticks) {
    CoreTiming::AddTicks(ticks);
}

static u64 GetTicksRemaining() {
    s64 ticks = CoreTiming::GetDowncount();
    return static_cast<u64>(ticks <= 0 ? 0 : ticks);
}

static Dynarmic::UserCallbacks GetUserCallbacks(
    const std::shared_ptr<ARMul_State>& interpreter_state, Memory::PageTable* current_page_table) {
    Dynarmic::UserCallbacks user_callbacks{};
    user_callbacks.InterpreterFallback = &InterpreterFallback;
    user_callbacks.user_arg = static_cast<void*>(interpreter_state.get());
    user_callbacks.CallSVC = &Kernel::CallSVC;
    user_callbacks.memory.IsReadOnlyMemory = &IsReadOnlyMemory;
    user_callbacks.memory.ReadCode = &Memory::Read32;
    user_callbacks.memory.Read8 = &Memory::Read8;
    user_callbacks.memory.Read16 = &Memory::Read16;
    user_callbacks.memory.Read32 = &Memory::Read32;
    user_callbacks.memory.Read64 = &Memory::Read64;
    user_callbacks.memory.Write8 = &Memory::Write8;
    user_callbacks.memory.Write16 = &Memory::Write16;
    user_callbacks.memory.Write32 = &Memory::Write32;
    user_callbacks.memory.Write64 = &Memory::Write64;
    user_callbacks.AddTicks = &AddTicks;
    user_callbacks.GetTicksRemaining = &GetTicksRemaining;
    user_callbacks.page_table = &current_page_table->pointers;
    user_callbacks.coprocessors[15] = std::make_shared<DynarmicCP15>(interpreter_state);
    return user_callbacks;
}

ARM_Dynarmic::ARM_Dynarmic(PrivilegeMode initial_mode) {
    interpreter_state = std::make_shared<ARMul_State>(initial_mode);
    PageTableChanged();
}

MICROPROFILE_DEFINE(ARM_Jit, "ARM JIT", "ARM JIT", MP_RGB(255, 64, 64));

void ARM_Dynarmic::Run() {
    ASSERT(Memory::GetCurrentPageTable() == current_page_table);
    MICROPROFILE_SCOPE(ARM_Jit);

    jit->Run(GetTicksRemaining());
}

void ARM_Dynarmic::Step() {
    InterpreterFallback(jit->Regs()[15], jit, static_cast<void*>(interpreter_state.get()));
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
    jit->SetCpsr(cpsr);
}

u32 ARM_Dynarmic::GetCP15Register(CP15Register reg) {
    return interpreter_state->CP15[reg];
}

void ARM_Dynarmic::SetCP15Register(CP15Register reg, u32 value) {
    interpreter_state->CP15[reg] = value;
}

std::unique_ptr<ARM_Interface::ThreadContext> ARM_Dynarmic::NewContext() const {
    return std::make_unique<DynarmicThreadContext>();
}

void ARM_Dynarmic::SaveContext(const std::unique_ptr<ThreadContext>& arg) {
    DynarmicThreadContext* ctx = dynamic_cast<DynarmicThreadContext*>(arg.get());
    ASSERT(ctx);

    jit->SaveContext(ctx->ctx);
    ctx->fpexc = interpreter_state->VFP[VFP_FPEXC];
}

void ARM_Dynarmic::LoadContext(const std::unique_ptr<ThreadContext>& arg) {
    const DynarmicThreadContext* ctx = dynamic_cast<DynarmicThreadContext*>(arg.get());
    ASSERT(ctx);

    jit->LoadContext(ctx->ctx);
    interpreter_state->VFP[VFP_FPEXC] = ctx->fpexc;
}

void ARM_Dynarmic::PrepareReschedule() {
    if (jit->IsExecuting()) {
        jit->HaltExecution();
    }
}

void ARM_Dynarmic::ClearInstructionCache() {
    // TODO: Clear interpreter cache when appropriate.
    for (const auto& j : jits) {
        j.second->ClearCache();
    }
    interpreter_state->instruction_cache.clear();
}

void ARM_Dynarmic::InvalidateCacheRange(u32 start_address, size_t length) {
    jit->InvalidateCacheRange(start_address, length);
}

void ARM_Dynarmic::PageTableChanged() {
    current_page_table = Memory::GetCurrentPageTable();

    auto iter = jits.find(current_page_table);
    if (iter != jits.end()) {
        jit = iter->second.get();
        return;
    }

    jit = new Dynarmic::Jit(GetUserCallbacks(interpreter_state, current_page_table));
    jits.emplace(current_page_table, std::unique_ptr<Dynarmic::Jit>(jit));
}
