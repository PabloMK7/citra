// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <initializer_list>
#include <xbyak.h>
#include "common/assert.h"
#include "common/bit_set.h"

namespace Common::X64 {

inline int RegToIndex(const Xbyak::Reg& reg) {
    using Kind = Xbyak::Reg::Kind;
    ASSERT_MSG((reg.getKind() & (Kind::REG | Kind::XMM)) != 0,
               "RegSet only support GPRs and XMM registers.");
    ASSERT_MSG(reg.getIdx() < 16, "RegSet only supports XXM0-15.");
    return reg.getIdx() + (reg.getKind() == Kind::REG ? 0 : 16);
}

inline Xbyak::Reg64 IndexToReg64(int reg_index) {
    ASSERT(reg_index < 16);
    return Xbyak::Reg64(reg_index);
}

inline Xbyak::Xmm IndexToXmm(int reg_index) {
    ASSERT(reg_index >= 16 && reg_index < 32);
    return Xbyak::Xmm(reg_index - 16);
}

inline Xbyak::Reg IndexToReg(int reg_index) {
    if (reg_index < 16) {
        return IndexToReg64(reg_index);
    } else {
        return IndexToXmm(reg_index);
    }
}

inline BitSet32 BuildRegSet(std::initializer_list<Xbyak::Reg> regs) {
    BitSet32 bits;
    for (const Xbyak::Reg& reg : regs) {
        bits[RegToIndex(reg)] = true;
    }
    return bits;
}

const BitSet32 ABI_ALL_GPRS(0x0000FFFF);
const BitSet32 ABI_ALL_XMMS(0xFFFF0000);

#ifdef _WIN32

// Microsoft x64 ABI
const Xbyak::Reg ABI_RETURN = Xbyak::util::rax;
const Xbyak::Reg ABI_PARAM1 = Xbyak::util::rcx;
const Xbyak::Reg ABI_PARAM2 = Xbyak::util::rdx;
const Xbyak::Reg ABI_PARAM3 = Xbyak::util::r8;
const Xbyak::Reg ABI_PARAM4 = Xbyak::util::r9;

const BitSet32 ABI_ALL_CALLER_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rcx,
    Xbyak::util::rdx,
    Xbyak::util::r8,
    Xbyak::util::r9,
    Xbyak::util::r10,
    Xbyak::util::r11,
    // XMMs
    Xbyak::util::xmm0,
    Xbyak::util::xmm1,
    Xbyak::util::xmm2,
    Xbyak::util::xmm3,
    Xbyak::util::xmm4,
    Xbyak::util::xmm5,
});

const BitSet32 ABI_ALL_CALLEE_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rbx,
    Xbyak::util::rsi,
    Xbyak::util::rdi,
    Xbyak::util::rbp,
    Xbyak::util::r12,
    Xbyak::util::r13,
    Xbyak::util::r14,
    Xbyak::util::r15,
    // XMMs
    Xbyak::util::xmm6,
    Xbyak::util::xmm7,
    Xbyak::util::xmm8,
    Xbyak::util::xmm9,
    Xbyak::util::xmm10,
    Xbyak::util::xmm11,
    Xbyak::util::xmm12,
    Xbyak::util::xmm13,
    Xbyak::util::xmm14,
    Xbyak::util::xmm15,
});

constexpr size_t ABI_SHADOW_SPACE = 0x20;

#else

// System V x86-64 ABI
const Xbyak::Reg ABI_RETURN = Xbyak::util::rax;
const Xbyak::Reg ABI_PARAM1 = Xbyak::util::rdi;
const Xbyak::Reg ABI_PARAM2 = Xbyak::util::rsi;
const Xbyak::Reg ABI_PARAM3 = Xbyak::util::rdx;
const Xbyak::Reg ABI_PARAM4 = Xbyak::util::rcx;

const BitSet32 ABI_ALL_CALLER_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rcx,
    Xbyak::util::rdx,
    Xbyak::util::rdi,
    Xbyak::util::rsi,
    Xbyak::util::r8,
    Xbyak::util::r9,
    Xbyak::util::r10,
    Xbyak::util::r11,
    // XMMs
    Xbyak::util::xmm0,
    Xbyak::util::xmm1,
    Xbyak::util::xmm2,
    Xbyak::util::xmm3,
    Xbyak::util::xmm4,
    Xbyak::util::xmm5,
    Xbyak::util::xmm6,
    Xbyak::util::xmm7,
    Xbyak::util::xmm8,
    Xbyak::util::xmm9,
    Xbyak::util::xmm10,
    Xbyak::util::xmm11,
    Xbyak::util::xmm12,
    Xbyak::util::xmm13,
    Xbyak::util::xmm14,
    Xbyak::util::xmm15,
});

const BitSet32 ABI_ALL_CALLEE_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rbx,
    Xbyak::util::rbp,
    Xbyak::util::r12,
    Xbyak::util::r13,
    Xbyak::util::r14,
    Xbyak::util::r15,
});

constexpr size_t ABI_SHADOW_SPACE = 0;

#endif

inline void ABI_CalculateFrameSize(BitSet32 regs, size_t rsp_alignment, size_t needed_frame_size,
                                   s32* out_subtraction, s32* out_xmm_offset) {
    int count = (regs & ABI_ALL_GPRS).Count();
    rsp_alignment -= count * 8;
    size_t subtraction = 0;
    int xmm_count = (regs & ABI_ALL_XMMS).Count();
    if (xmm_count) {
        // If we have any XMMs to save, we must align the stack here.
        subtraction = rsp_alignment & 0xF;
    }
    subtraction += 0x10 * xmm_count;
    size_t xmm_base_subtraction = subtraction;
    subtraction += needed_frame_size;
    subtraction += ABI_SHADOW_SPACE;
    // Final alignment.
    rsp_alignment -= subtraction;
    subtraction += rsp_alignment & 0xF;

    *out_subtraction = (s32)subtraction;
    *out_xmm_offset = (s32)(subtraction - xmm_base_subtraction);
}

inline size_t ABI_PushRegistersAndAdjustStack(Xbyak::CodeGenerator& code, BitSet32 regs,
                                              size_t rsp_alignment, size_t needed_frame_size = 0) {
    s32 subtraction, xmm_offset;
    ABI_CalculateFrameSize(regs, rsp_alignment, needed_frame_size, &subtraction, &xmm_offset);

    for (int reg_index : (regs & ABI_ALL_GPRS)) {
        code.push(IndexToReg64(reg_index));
    }

    if (subtraction != 0) {
        code.sub(code.rsp, subtraction);
    }

    for (int reg_index : (regs & ABI_ALL_XMMS)) {
        code.movaps(code.xword[code.rsp + xmm_offset], IndexToXmm(reg_index));
        xmm_offset += 0x10;
    }

    return ABI_SHADOW_SPACE;
}

inline void ABI_PopRegistersAndAdjustStack(Xbyak::CodeGenerator& code, BitSet32 regs,
                                           size_t rsp_alignment, size_t needed_frame_size = 0) {
    s32 subtraction, xmm_offset;
    ABI_CalculateFrameSize(regs, rsp_alignment, needed_frame_size, &subtraction, &xmm_offset);

    for (int reg_index : (regs & ABI_ALL_XMMS)) {
        code.movaps(IndexToXmm(reg_index), code.xword[code.rsp + xmm_offset]);
        xmm_offset += 0x10;
    }

    if (subtraction != 0) {
        code.add(code.rsp, subtraction);
    }

    // GPRs need to be popped in reverse order
    for (int reg_index = 15; reg_index >= 0; reg_index--) {
        if (regs[reg_index]) {
            code.pop(IndexToReg64(reg_index));
        }
    }
}

} // namespace Common::X64
