// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/context.h>
#include "common/assert.h"
#include "common/microprofile.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dyncom/arm_dyncom_interpreter.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/gdbstub/gdbstub.h"
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

    u32 GetCpuRegister(std::size_t index) const override {
        return ctx.Regs()[index];
    }
    void SetCpuRegister(std::size_t index, u32 value) override {
        ctx.Regs()[index] = value;
    }
    u32 GetCpsr() const override {
        return ctx.Cpsr();
    }
    void SetCpsr(u32 value) override {
        ctx.SetCpsr(value);
    }
    u32 GetFpuRegister(std::size_t index) const override {
        return ctx.ExtRegs()[index];
    }
    void SetFpuRegister(std::size_t index, u32 value) override {
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

    Dynarmic::A32::Context ctx;
    u32 fpexc;
};

class DynarmicUserCallbacks final : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicUserCallbacks(ARM_Dynarmic& parent)
        : parent(parent), svc_context(parent.system), memory(parent.memory) {}
    ~DynarmicUserCallbacks() = default;

    std::uint8_t MemoryRead8(VAddr vaddr) override {
        return memory.Read8(vaddr);
    }
    std::uint16_t MemoryRead16(VAddr vaddr) override {
        return memory.Read16(vaddr);
    }
    std::uint32_t MemoryRead32(VAddr vaddr) override {
        return memory.Read32(vaddr);
    }
    std::uint64_t MemoryRead64(VAddr vaddr) override {
        return memory.Read64(vaddr);
    }

    void MemoryWrite8(VAddr vaddr, std::uint8_t value) override {
        memory.Write8(vaddr, value);
    }
    void MemoryWrite16(VAddr vaddr, std::uint16_t value) override {
        memory.Write16(vaddr, value);
    }
    void MemoryWrite32(VAddr vaddr, std::uint32_t value) override {
        memory.Write32(vaddr, value);
    }
    void MemoryWrite64(VAddr vaddr, std::uint64_t value) override {
        memory.Write64(vaddr, value);
    }

    void InterpreterFallback(VAddr pc, std::size_t num_instructions) override {
        parent.interpreter_state->Reg = parent.jit->Regs();
        parent.interpreter_state->Cpsr = parent.jit->Cpsr();
        parent.interpreter_state->Reg[15] = pc;
        parent.interpreter_state->ExtReg = parent.jit->ExtRegs();
        parent.interpreter_state->VFP[VFP_FPSCR] = parent.jit->Fpscr();
        parent.interpreter_state->NumInstrsToExecute = num_instructions;

        InterpreterMainLoop(parent.interpreter_state.get());

        bool is_thumb = (parent.interpreter_state->Cpsr & (1 << 5)) != 0;
        parent.interpreter_state->Reg[15] &= (is_thumb ? 0xFFFFFFFE : 0xFFFFFFFC);

        parent.jit->Regs() = parent.interpreter_state->Reg;
        parent.jit->SetCpsr(parent.interpreter_state->Cpsr);
        parent.jit->ExtRegs() = parent.interpreter_state->ExtReg;
        parent.jit->SetFpscr(parent.interpreter_state->VFP[VFP_FPSCR]);

        parent.interpreter_state->ServeBreak();
    }

    void CallSVC(std::uint32_t swi) override {
        svc_context.CallSVC(swi);
    }

    void ExceptionRaised(VAddr pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            break;
        case Dynarmic::A32::Exception::Breakpoint:
            if (GDBStub::IsConnected()) {
                parent.jit->HaltExecution();
                parent.SetPC(pc);
                Kernel::Thread* thread =
                    parent.system.Kernel().GetCurrentThreadManager().GetCurrentThread();
                parent.SaveContext(thread->context);
                GDBStub::Break();
                GDBStub::SendTrap(thread, 5);
                return;
            }
            break;
        }
        ASSERT_MSG(false, "ExceptionRaised(exception = {}, pc = {:08X}, code = {:08X})",
                   static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));
    }

    void AddTicks(std::uint64_t ticks) override {
        parent.GetTimer()->AddTicks(ticks);
    }
    std::uint64_t GetTicksRemaining() override {
        s64 ticks = parent.GetTimer()->GetDowncount();
        return static_cast<u64>(ticks <= 0 ? 0 : ticks);
    }

    ARM_Dynarmic& parent;
    Kernel::SVCContext svc_context;
    Memory::MemorySystem& memory;
};

ARM_Dynarmic::ARM_Dynarmic(Core::System* system, Memory::MemorySystem& memory,
                           PrivilegeMode initial_mode, u32 id,
                           std::shared_ptr<Core::Timing::Timer> timer)
    : ARM_Interface(id, timer), system(*system), memory(memory),
      cb(std::make_unique<DynarmicUserCallbacks>(*this)) {
    interpreter_state = std::make_shared<ARMul_State>(system, memory, initial_mode);
    SetPageTable(memory.GetCurrentPageTable());
}

ARM_Dynarmic::~ARM_Dynarmic() = default;

MICROPROFILE_DEFINE(ARM_Jit, "ARM JIT", "ARM JIT", MP_RGB(255, 64, 64));

void ARM_Dynarmic::Run() {
    ASSERT(memory.GetCurrentPageTable() == current_page_table);
    MICROPROFILE_SCOPE(ARM_Jit);

    jit->Run();
}

void ARM_Dynarmic::Step() {
    cb->InterpreterFallback(jit->Regs()[15], 1);
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

u32 ARM_Dynarmic::GetCP15Register(CP15Register reg) const {
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

void ARM_Dynarmic::InvalidateCacheRange(u32 start_address, std::size_t length) {
    jit->InvalidateCacheRange(start_address, length);
}

std::shared_ptr<Memory::PageTable> ARM_Dynarmic::GetPageTable() const {
    return current_page_table;
}

void ARM_Dynarmic::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    current_page_table = page_table;
    Dynarmic::A32::Context ctx{};
    if (jit) {
        jit->SaveContext(ctx);
    }

    auto iter = jits.find(current_page_table);
    if (iter != jits.end()) {
        jit = iter->second.get();
        jit->LoadContext(ctx);
        return;
    }

    auto new_jit = MakeJit();
    jit = new_jit.get();
    jit->LoadContext(ctx);
    jits.emplace(current_page_table, std::move(new_jit));
}

std::unique_ptr<Dynarmic::A32::Jit> ARM_Dynarmic::MakeJit() {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    config.page_table = &current_page_table->GetPointerArray();
    config.coprocessors[15] = std::make_shared<DynarmicCP15>(interpreter_state);
    config.define_unpredictable_behaviour = true;
    return std::make_unique<Dynarmic::A32::Jit>(config);
}

void ARM_Dynarmic::PurgeState() {
    ClearInstructionCache();
}
