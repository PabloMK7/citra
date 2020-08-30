// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <nihstro/shader_bytecode.h>
#include <smmintrin.h>
#include <xmmintrin.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "common/x64/cpu_detect.h"
#include "common/x64/xbyak_abi.h"
#include "common/x64/xbyak_util.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_jit_x64_compiler.h"

using namespace Common::X64;
using namespace Xbyak::util;
using Xbyak::Label;
using Xbyak::Reg32;
using Xbyak::Reg64;
using Xbyak::Xmm;

namespace Pica::Shader {

typedef void (JitShader::*JitFunction)(Instruction instr);

const JitFunction instr_table[64] = {
    &JitShader::Compile_ADD,    // add
    &JitShader::Compile_DP3,    // dp3
    &JitShader::Compile_DP4,    // dp4
    &JitShader::Compile_DPH,    // dph
    nullptr,                    // unknown
    &JitShader::Compile_EX2,    // ex2
    &JitShader::Compile_LG2,    // lg2
    nullptr,                    // unknown
    &JitShader::Compile_MUL,    // mul
    &JitShader::Compile_SGE,    // sge
    &JitShader::Compile_SLT,    // slt
    &JitShader::Compile_FLR,    // flr
    &JitShader::Compile_MAX,    // max
    &JitShader::Compile_MIN,    // min
    &JitShader::Compile_RCP,    // rcp
    &JitShader::Compile_RSQ,    // rsq
    nullptr,                    // unknown
    nullptr,                    // unknown
    &JitShader::Compile_MOVA,   // mova
    &JitShader::Compile_MOV,    // mov
    nullptr,                    // unknown
    nullptr,                    // unknown
    nullptr,                    // unknown
    nullptr,                    // unknown
    &JitShader::Compile_DPH,    // dphi
    nullptr,                    // unknown
    &JitShader::Compile_SGE,    // sgei
    &JitShader::Compile_SLT,    // slti
    nullptr,                    // unknown
    nullptr,                    // unknown
    nullptr,                    // unknown
    nullptr,                    // unknown
    nullptr,                    // unknown
    &JitShader::Compile_NOP,    // nop
    &JitShader::Compile_END,    // end
    &JitShader::Compile_BREAKC, // breakc
    &JitShader::Compile_CALL,   // call
    &JitShader::Compile_CALLC,  // callc
    &JitShader::Compile_CALLU,  // callu
    &JitShader::Compile_IF,     // ifu
    &JitShader::Compile_IF,     // ifc
    &JitShader::Compile_LOOP,   // loop
    &JitShader::Compile_EMIT,   // emit
    &JitShader::Compile_SETE,   // sete
    &JitShader::Compile_JMP,    // jmpc
    &JitShader::Compile_JMP,    // jmpu
    &JitShader::Compile_CMP,    // cmp
    &JitShader::Compile_CMP,    // cmp
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // madi
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
    &JitShader::Compile_MAD,    // mad
};

// The following is used to alias some commonly used registers. Generally, RAX-RDX and XMM0-XMM3 can
// be used as scratch registers within a compiler function. The other registers have designated
// purposes, as documented below:

/// Pointer to the uniform memory
constexpr Reg64 UNIFORMS = r9;
/// The two 32-bit VS address offset registers set by the MOVA instruction
constexpr Reg64 ADDROFFS_REG_0 = r10;
constexpr Reg64 ADDROFFS_REG_1 = r11;
/// VS loop count register (Multiplied by 16)
constexpr Reg32 LOOPCOUNT_REG = r12d;
/// Current VS loop iteration number (we could probably use LOOPCOUNT_REG, but this quicker)
constexpr Reg32 LOOPCOUNT = esi;
/// Number to increment LOOPCOUNT_REG by on each loop iteration (Multiplied by 16)
constexpr Reg32 LOOPINC = edi;
/// Result of the previous CMP instruction for the X-component comparison
constexpr Reg64 COND0 = r13;
/// Result of the previous CMP instruction for the Y-component comparison
constexpr Reg64 COND1 = r14;
/// Pointer to the UnitState instance for the current VS unit
constexpr Reg64 STATE = r15;
/// SIMD scratch register
constexpr Xmm SCRATCH = xmm0;
/// Loaded with the first swizzled source register, otherwise can be used as a scratch register
constexpr Xmm SRC1 = xmm1;
/// Loaded with the second swizzled source register, otherwise can be used as a scratch register
constexpr Xmm SRC2 = xmm2;
/// Loaded with the third swizzled source register, otherwise can be used as a scratch register
constexpr Xmm SRC3 = xmm3;
/// Additional scratch register
constexpr Xmm SCRATCH2 = xmm4;
/// Constant vector of [1.0f, 1.0f, 1.0f, 1.0f], used to efficiently set a vector to one
constexpr Xmm ONE = xmm14;
/// Constant vector of [-0.f, -0.f, -0.f, -0.f], used to efficiently negate a vector with XOR
constexpr Xmm NEGBIT = xmm15;

// State registers that must not be modified by external functions calls
// Scratch registers, e.g., SRC1 and SCRATCH, have to be saved on the side if needed
static const std::bitset<32> persistent_regs = BuildRegSet({
    // Pointers to register blocks
    UNIFORMS,
    STATE,
    // Cached registers
    ADDROFFS_REG_0,
    ADDROFFS_REG_1,
    LOOPCOUNT_REG,
    COND0,
    COND1,
    // Constants
    ONE,
    NEGBIT,
    // Loop variables
    LOOPCOUNT,
    LOOPINC,
});

/// Raw constant for the source register selector that indicates no swizzling is performed
static const u8 NO_SRC_REG_SWIZZLE = 0x1b;
/// Raw constant for the destination register enable mask that indicates all components are enabled
static const u8 NO_DEST_REG_MASK = 0xf;

static void LogCritical(const char* msg) {
    LOG_CRITICAL(HW_GPU, "{}", msg);
}

void JitShader::Compile_Assert(bool condition, const char* msg) {
    if (!condition) {
        mov(ABI_PARAM1, reinterpret_cast<std::size_t>(msg));
        CallFarFunction(*this, LogCritical);
    }
}

/**
 * Loads and swizzles a source register into the specified XMM register.
 * @param instr VS instruction, used for determining how to load the source register
 * @param src_num Number indicating which source register to load (1 = src1, 2 = src2, 3 = src3)
 * @param src_reg SourceRegister object corresponding to the source register to load
 * @param dest Destination XMM register to store the loaded, swizzled source register
 */
void JitShader::Compile_SwizzleSrc(Instruction instr, unsigned src_num, SourceRegister src_reg,
                                   Xmm dest) {
    Reg64 src_ptr;
    std::size_t src_offset;

    if (src_reg.GetRegisterType() == RegisterType::FloatUniform) {
        src_ptr = UNIFORMS;
        src_offset = Uniforms::GetFloatUniformOffset(src_reg.GetIndex());
    } else {
        src_ptr = STATE;
        src_offset = UnitState::InputOffset(src_reg);
    }

    int src_offset_disp = (int)src_offset;
    ASSERT_MSG(src_offset == src_offset_disp, "Source register offset too large for int type");

    unsigned operand_desc_id;

    const bool is_inverted =
        (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));

    unsigned address_register_index;
    unsigned offset_src;

    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD ||
        instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI) {
        operand_desc_id = instr.mad.operand_desc_id;
        offset_src = is_inverted ? 3 : 2;
        address_register_index = instr.mad.address_register_index;
    } else {
        operand_desc_id = instr.common.operand_desc_id;
        offset_src = is_inverted ? 2 : 1;
        address_register_index = instr.common.address_register_index;
    }

    if (src_num == offset_src && address_register_index != 0) {
        switch (address_register_index) {
        case 1: // address offset 1
            movaps(dest, xword[src_ptr + ADDROFFS_REG_0 + src_offset_disp]);
            break;
        case 2: // address offset 2
            movaps(dest, xword[src_ptr + ADDROFFS_REG_1 + src_offset_disp]);
            break;
        case 3: // address offset 3
            movaps(dest, xword[src_ptr + LOOPCOUNT_REG.cvt64() + src_offset_disp]);
            break;
        default:
            UNREACHABLE();
            break;
        }
    } else {
        // Load the source
        movaps(dest, xword[src_ptr + src_offset_disp]);
    }

    SwizzlePattern swiz = {(*swizzle_data)[operand_desc_id]};

    // Generate instructions for source register swizzling as needed
    u8 sel = swiz.GetRawSelector(src_num);
    if (sel != NO_SRC_REG_SWIZZLE) {
        // Selector component order needs to be reversed for the SHUFPS instruction
        sel = ((sel & 0xc0) >> 6) | ((sel & 3) << 6) | ((sel & 0xc) << 2) | ((sel & 0x30) >> 2);

        // Shuffle inputs for swizzle
        shufps(dest, dest, sel);
    }

    // If the source register should be negated, flip the negative bit using XOR
    const bool negate[] = {swiz.negate_src1, swiz.negate_src2, swiz.negate_src3};
    if (negate[src_num - 1]) {
        xorps(dest, NEGBIT);
    }
}

void JitShader::Compile_DestEnable(Instruction instr, Xmm src) {
    DestRegister dest;
    unsigned operand_desc_id;
    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD ||
        instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI) {
        operand_desc_id = instr.mad.operand_desc_id;
        dest = instr.mad.dest.Value();
    } else {
        operand_desc_id = instr.common.operand_desc_id;
        dest = instr.common.dest.Value();
    }

    SwizzlePattern swiz = {(*swizzle_data)[operand_desc_id]};

    std::size_t dest_offset_disp = UnitState::OutputOffset(dest);

    // If all components are enabled, write the result to the destination register
    if (swiz.dest_mask == NO_DEST_REG_MASK) {
        // Store dest back to memory
        movaps(xword[STATE + dest_offset_disp], src);

    } else {
        // Not all components are enabled, so mask the result when storing to the destination
        // register...
        movaps(SCRATCH, xword[STATE + dest_offset_disp]);

        if (Common::GetCPUCaps().sse4_1) {
            u8 mask = ((swiz.dest_mask & 1) << 3) | ((swiz.dest_mask & 8) >> 3) |
                      ((swiz.dest_mask & 2) << 1) | ((swiz.dest_mask & 4) >> 1);
            blendps(SCRATCH, src, mask);
        } else {
            movaps(SCRATCH2, src);
            unpckhps(SCRATCH2, SCRATCH); // Unpack X/Y components of source and destination
            unpcklps(SCRATCH, src);      // Unpack Z/W components of source and destination

            // Compute selector to selectively copy source components to destination for SHUFPS
            // instruction
            u8 sel = ((swiz.DestComponentEnabled(0) ? 1 : 0) << 0) |
                     ((swiz.DestComponentEnabled(1) ? 3 : 2) << 2) |
                     ((swiz.DestComponentEnabled(2) ? 0 : 1) << 4) |
                     ((swiz.DestComponentEnabled(3) ? 2 : 3) << 6);
            shufps(SCRATCH, SCRATCH2, sel);
        }

        // Store dest back to memory
        movaps(xword[STATE + dest_offset_disp], SCRATCH);
    }
}

void JitShader::Compile_SanitizedMul(Xmm src1, Xmm src2, Xmm scratch) {
    // 0 * inf and inf * 0 in the PICA should return 0 instead of NaN. This can be implemented by
    // checking for NaNs before and after the multiplication.  If the multiplication result is NaN
    // where neither source was, this NaN was generated by a 0 * inf multiplication, and so the
    // result should be transformed to 0 to match PICA fp rules.

    // Set scratch to mask of (src1 != NaN and src2 != NaN)
    movaps(scratch, src1);
    cmpordps(scratch, src2);

    mulps(src1, src2);

    // Set src2 to mask of (result == NaN)
    movaps(src2, src1);
    cmpunordps(src2, src2);

    // Clear components where scratch != src2 (i.e. if result is NaN where neither source was NaN)
    xorps(scratch, src2);
    andps(src1, scratch);
}

void JitShader::Compile_EvaluateCondition(Instruction instr) {
    // Note: NXOR is used below to check for equality
    switch (instr.flow_control.op) {
    case Instruction::FlowControlType::Or:
        mov(eax, COND0);
        mov(ebx, COND1);
        xor_(eax, (instr.flow_control.refx.Value() ^ 1));
        xor_(ebx, (instr.flow_control.refy.Value() ^ 1));
        or_(eax, ebx);
        break;

    case Instruction::FlowControlType::And:
        mov(eax, COND0);
        mov(ebx, COND1);
        xor_(eax, (instr.flow_control.refx.Value() ^ 1));
        xor_(ebx, (instr.flow_control.refy.Value() ^ 1));
        and_(eax, ebx);
        break;

    case Instruction::FlowControlType::JustX:
        mov(eax, COND0);
        xor_(eax, (instr.flow_control.refx.Value() ^ 1));
        break;

    case Instruction::FlowControlType::JustY:
        mov(eax, COND1);
        xor_(eax, (instr.flow_control.refy.Value() ^ 1));
        break;
    }
}

void JitShader::Compile_UniformCondition(Instruction instr) {
    std::size_t offset = Uniforms::GetBoolUniformOffset(instr.flow_control.bool_uniform_id);
    cmp(byte[UNIFORMS + offset], 0);
}

std::bitset<32> JitShader::PersistentCallerSavedRegs() {
    return persistent_regs & ABI_ALL_CALLER_SAVED;
}

void JitShader::Compile_ADD(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    addps(SRC1, SRC2);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_DP3(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);

    Compile_SanitizedMul(SRC1, SRC2, SCRATCH);

    movaps(SRC2, SRC1);
    shufps(SRC2, SRC2, _MM_SHUFFLE(1, 1, 1, 1));

    movaps(SRC3, SRC1);
    shufps(SRC3, SRC3, _MM_SHUFFLE(2, 2, 2, 2));

    shufps(SRC1, SRC1, _MM_SHUFFLE(0, 0, 0, 0));
    addps(SRC1, SRC2);
    addps(SRC1, SRC3);

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_DP4(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);

    Compile_SanitizedMul(SRC1, SRC2, SCRATCH);

    haddps(SRC1, SRC1);
    haddps(SRC1, SRC1);

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_DPH(Instruction instr) {
    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::DPHI) {
        Compile_SwizzleSrc(instr, 1, instr.common.src1i, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2i, SRC2);
    } else {
        Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    }

    if (Common::GetCPUCaps().sse4_1) {
        // Set 4th component to 1.0
        blendps(SRC1, ONE, 0b1000);
    } else {
        // Set 4th component to 1.0
        movaps(SCRATCH, SRC1);
        unpckhps(SCRATCH, ONE);  // XYZW, 1111 -> Z1__
        unpcklpd(SRC1, SCRATCH); // XYZW, Z1__ -> XYZ1
    }

    Compile_SanitizedMul(SRC1, SRC2, SCRATCH);

    haddps(SRC1, SRC1);
    haddps(SRC1, SRC1);

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_EX2(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    call(exp2_subroutine);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_LG2(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    call(log2_subroutine);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_MUL(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    Compile_SanitizedMul(SRC1, SRC2, SCRATCH);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_SGE(Instruction instr) {
    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::SGEI) {
        Compile_SwizzleSrc(instr, 1, instr.common.src1i, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2i, SRC2);
    } else {
        Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    }

    cmpleps(SRC2, SRC1);
    andps(SRC2, ONE);

    Compile_DestEnable(instr, SRC2);
}

void JitShader::Compile_SLT(Instruction instr) {
    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::SLTI) {
        Compile_SwizzleSrc(instr, 1, instr.common.src1i, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2i, SRC2);
    } else {
        Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
        Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    }

    cmpltps(SRC1, SRC2);
    andps(SRC1, ONE);

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_FLR(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);

    if (Common::GetCPUCaps().sse4_1) {
        roundps(SRC1, SRC1, _MM_FROUND_FLOOR);
    } else {
        cvttps2dq(SRC1, SRC1);
        cvtdq2ps(SRC1, SRC1);
    }

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_MAX(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    // SSE semantics match PICA200 ones: In case of NaN, SRC2 is returned.
    maxps(SRC1, SRC2);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_MIN(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);
    // SSE semantics match PICA200 ones: In case of NaN, SRC2 is returned.
    minps(SRC1, SRC2);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_MOVA(Instruction instr) {
    SwizzlePattern swiz = {(*swizzle_data)[instr.common.operand_desc_id]};

    if (!swiz.DestComponentEnabled(0) && !swiz.DestComponentEnabled(1)) {
        return; // NoOp
    }

    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);

    // Convert floats to integers using truncation (only care about X and Y components)
    cvttps2dq(SRC1, SRC1);

    // Get result
    movq(rax, SRC1);

    // Handle destination enable
    if (swiz.DestComponentEnabled(0) && swiz.DestComponentEnabled(1)) {
        // Move and sign-extend low 32 bits
        movsxd(ADDROFFS_REG_0, eax);

        // Move and sign-extend high 32 bits
        shr(rax, 32);
        movsxd(ADDROFFS_REG_1, eax);

        // Multiply by 16 to be used as an offset later
        shl(ADDROFFS_REG_0, 4);
        shl(ADDROFFS_REG_1, 4);
    } else {
        if (swiz.DestComponentEnabled(0)) {
            // Move and sign-extend low 32 bits
            movsxd(ADDROFFS_REG_0, eax);

            // Multiply by 16 to be used as an offset later
            shl(ADDROFFS_REG_0, 4);
        } else if (swiz.DestComponentEnabled(1)) {
            // Move and sign-extend high 32 bits
            shr(rax, 32);
            movsxd(ADDROFFS_REG_1, eax);

            // Multiply by 16 to be used as an offset later
            shl(ADDROFFS_REG_1, 4);
        }
    }
}

void JitShader::Compile_MOV(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_RCP(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);

    // TODO(bunnei): RCPSS is a pretty rough approximation, this might cause problems if Pica
    // performs this operation more accurately. This should be checked on hardware.
    rcpss(SRC1, SRC1);
    shufps(SRC1, SRC1, _MM_SHUFFLE(0, 0, 0, 0)); // XYWZ -> XXXX

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_RSQ(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);

    // TODO(bunnei): RSQRTSS is a pretty rough approximation, this might cause problems if Pica
    // performs this operation more accurately. This should be checked on hardware.
    rsqrtss(SRC1, SRC1);
    shufps(SRC1, SRC1, _MM_SHUFFLE(0, 0, 0, 0)); // XYWZ -> XXXX

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_NOP(Instruction instr) {}

void JitShader::Compile_END(Instruction instr) {
    // Save conditional code
    mov(byte[STATE + offsetof(UnitState, conditional_code[0])], COND0.cvt8());
    mov(byte[STATE + offsetof(UnitState, conditional_code[1])], COND1.cvt8());

    // Save address/loop registers
    sar(ADDROFFS_REG_0, 4);
    sar(ADDROFFS_REG_1, 4);
    sar(LOOPCOUNT_REG, 4);
    mov(dword[STATE + offsetof(UnitState, address_registers[0])], ADDROFFS_REG_0.cvt32());
    mov(dword[STATE + offsetof(UnitState, address_registers[1])], ADDROFFS_REG_1.cvt32());
    mov(dword[STATE + offsetof(UnitState, address_registers[2])], LOOPCOUNT_REG);

    ABI_PopRegistersAndAdjustStack(*this, ABI_ALL_CALLEE_SAVED, 8, 16);
    ret();
}

void JitShader::Compile_BREAKC(Instruction instr) {
    Compile_Assert(looping, "BREAKC must be inside a LOOP");
    if (looping) {
        Compile_EvaluateCondition(instr);
        ASSERT(loop_break_label);
        jnz(*loop_break_label);
    }
}

void JitShader::Compile_CALL(Instruction instr) {
    // Push offset of the return
    push(qword, (instr.flow_control.dest_offset + instr.flow_control.num_instructions));

    // Call the subroutine
    call(instruction_labels[instr.flow_control.dest_offset]);

    // Skip over the return offset that's on the stack
    add(rsp, 8);
}

void JitShader::Compile_CALLC(Instruction instr) {
    Compile_EvaluateCondition(instr);
    Label b;
    jz(b);
    Compile_CALL(instr);
    L(b);
}

void JitShader::Compile_CALLU(Instruction instr) {
    Compile_UniformCondition(instr);
    Label b;
    jz(b);
    Compile_CALL(instr);
    L(b);
}

void JitShader::Compile_CMP(Instruction instr) {
    using Op = Instruction::Common::CompareOpType::Op;
    Op op_x = instr.common.compare_op.x;
    Op op_y = instr.common.compare_op.y;

    Compile_SwizzleSrc(instr, 1, instr.common.src1, SRC1);
    Compile_SwizzleSrc(instr, 2, instr.common.src2, SRC2);

    // SSE doesn't have greater-than (GT) or greater-equal (GE) comparison operators. You need to
    // emulate them by swapping the lhs and rhs and using LT and LE. NLT and NLE can't be used here
    // because they don't match when used with NaNs.
    static const u8 cmp[] = {CMP_EQ, CMP_NEQ, CMP_LT, CMP_LE, CMP_LT, CMP_LE};

    bool invert_op_x = (op_x == Op::GreaterThan || op_x == Op::GreaterEqual);
    Xmm lhs_x = invert_op_x ? SRC2 : SRC1;
    Xmm rhs_x = invert_op_x ? SRC1 : SRC2;

    if (op_x == op_y) {
        // Compare X-component and Y-component together
        cmpps(lhs_x, rhs_x, cmp[op_x]);
        movq(COND0, lhs_x);

        mov(COND1, COND0);
    } else {
        bool invert_op_y = (op_y == Op::GreaterThan || op_y == Op::GreaterEqual);
        Xmm lhs_y = invert_op_y ? SRC2 : SRC1;
        Xmm rhs_y = invert_op_y ? SRC1 : SRC2;

        // Compare X-component
        movaps(SCRATCH, lhs_x);
        cmpss(SCRATCH, rhs_x, cmp[op_x]);

        // Compare Y-component
        cmpps(lhs_y, rhs_y, cmp[op_y]);

        movq(COND0, SCRATCH);
        movq(COND1, lhs_y);
    }

    shr(COND0.cvt32(), 31); // ignores upper 32 bits in source
    shr(COND1, 63);
}

void JitShader::Compile_MAD(Instruction instr) {
    Compile_SwizzleSrc(instr, 1, instr.mad.src1, SRC1);

    if (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI) {
        Compile_SwizzleSrc(instr, 2, instr.mad.src2i, SRC2);
        Compile_SwizzleSrc(instr, 3, instr.mad.src3i, SRC3);
    } else {
        Compile_SwizzleSrc(instr, 2, instr.mad.src2, SRC2);
        Compile_SwizzleSrc(instr, 3, instr.mad.src3, SRC3);
    }

    Compile_SanitizedMul(SRC1, SRC2, SCRATCH);
    addps(SRC1, SRC3);

    Compile_DestEnable(instr, SRC1);
}

void JitShader::Compile_IF(Instruction instr) {
    Compile_Assert(instr.flow_control.dest_offset >= program_counter,
                   "Backwards if-statements not supported");
    Label l_else, l_endif;

    // Evaluate the "IF" condition
    if (instr.opcode.Value() == OpCode::Id::IFU) {
        Compile_UniformCondition(instr);
    } else if (instr.opcode.Value() == OpCode::Id::IFC) {
        Compile_EvaluateCondition(instr);
    }
    jz(l_else, T_NEAR);

    // Compile the code that corresponds to the condition evaluating as true
    Compile_Block(instr.flow_control.dest_offset);

    // If there isn't an "ELSE" condition, we are done here
    if (instr.flow_control.num_instructions == 0) {
        L(l_else);
        return;
    }

    jmp(l_endif, T_NEAR);

    L(l_else);
    // This code corresponds to the "ELSE" condition
    // Comple the code that corresponds to the condition evaluating as false
    Compile_Block(instr.flow_control.dest_offset + instr.flow_control.num_instructions);

    L(l_endif);
}

void JitShader::Compile_LOOP(Instruction instr) {
    Compile_Assert(instr.flow_control.dest_offset >= program_counter,
                   "Backwards loops not supported");
    Compile_Assert(!looping, "Nested loops not supported");

    looping = true;

    // This decodes the fields from the integer uniform at index instr.flow_control.int_uniform_id.
    // The Y (LOOPCOUNT_REG) and Z (LOOPINC) component are kept multiplied by 16 (Left shifted by
    // 4 bits) to be used as an offset into the 16-byte vector registers later
    std::size_t offset = Uniforms::GetIntUniformOffset(instr.flow_control.int_uniform_id);
    mov(LOOPCOUNT, dword[UNIFORMS + offset]);
    mov(LOOPCOUNT_REG, LOOPCOUNT);
    shr(LOOPCOUNT_REG, 4);
    and_(LOOPCOUNT_REG, 0xFF0); // Y-component is the start
    mov(LOOPINC, LOOPCOUNT);
    shr(LOOPINC, 12);
    and_(LOOPINC, 0xFF0);               // Z-component is the incrementer
    movzx(LOOPCOUNT, LOOPCOUNT.cvt8()); // X-component is iteration count
    add(LOOPCOUNT, 1);                  // Iteration count is X-component + 1

    Label l_loop_start;
    L(l_loop_start);

    loop_break_label = Xbyak::Label();
    Compile_Block(instr.flow_control.dest_offset + 1);

    add(LOOPCOUNT_REG, LOOPINC); // Increment LOOPCOUNT_REG by Z-component
    sub(LOOPCOUNT, 1);           // Increment loop count by 1
    jnz(l_loop_start);           // Loop if not equal
    L(*loop_break_label);
    loop_break_label.reset();

    looping = false;
}

void JitShader::Compile_JMP(Instruction instr) {
    if (instr.opcode.Value() == OpCode::Id::JMPC)
        Compile_EvaluateCondition(instr);
    else if (instr.opcode.Value() == OpCode::Id::JMPU)
        Compile_UniformCondition(instr);
    else
        UNREACHABLE();

    bool inverted_condition =
        (instr.opcode.Value() == OpCode::Id::JMPU) && (instr.flow_control.num_instructions & 1);

    Label& b = instruction_labels[instr.flow_control.dest_offset];
    if (inverted_condition) {
        jz(b, T_NEAR);
    } else {
        jnz(b, T_NEAR);
    }
}

static void Emit(GSEmitter* emitter, Common::Vec4<float24> (*output)[16]) {
    emitter->Emit(*output);
}

void JitShader::Compile_EMIT(Instruction instr) {
    Label have_emitter, end;
    mov(rax, qword[STATE + offsetof(UnitState, emitter_ptr)]);
    test(rax, rax);
    jnz(have_emitter);

    ABI_PushRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    mov(ABI_PARAM1, reinterpret_cast<std::size_t>("Execute EMIT on VS"));
    CallFarFunction(*this, LogCritical);
    ABI_PopRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    jmp(end);

    L(have_emitter);
    ABI_PushRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    mov(ABI_PARAM1, rax);
    mov(ABI_PARAM2, STATE);
    add(ABI_PARAM2, static_cast<Xbyak::uint32>(offsetof(UnitState, registers.output)));
    CallFarFunction(*this, Emit);
    ABI_PopRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    L(end);
}

void JitShader::Compile_SETE(Instruction instr) {
    Label have_emitter, end;
    mov(rax, qword[STATE + offsetof(UnitState, emitter_ptr)]);
    test(rax, rax);
    jnz(have_emitter);

    ABI_PushRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    mov(ABI_PARAM1, reinterpret_cast<std::size_t>("Execute SETEMIT on VS"));
    CallFarFunction(*this, LogCritical);
    ABI_PopRegistersAndAdjustStack(*this, PersistentCallerSavedRegs(), 0);
    jmp(end);

    L(have_emitter);
    mov(byte[rax + offsetof(GSEmitter, vertex_id)], instr.setemit.vertex_id);
    mov(byte[rax + offsetof(GSEmitter, prim_emit)], instr.setemit.prim_emit);
    mov(byte[rax + offsetof(GSEmitter, winding)], instr.setemit.winding);
    L(end);
}

void JitShader::Compile_Block(unsigned end) {
    while (program_counter < end) {
        Compile_NextInstr();
    }
}

void JitShader::Compile_Return() {
    // Peek return offset on the stack and check if we're at that offset
    mov(rax, qword[rsp + 8]);
    cmp(eax, (program_counter));

    // If so, jump back to before CALL
    Label b;
    jnz(b);
    ret();
    L(b);
}

void JitShader::Compile_NextInstr() {
    if (std::binary_search(return_offsets.begin(), return_offsets.end(), program_counter)) {
        Compile_Return();
    }

    L(instruction_labels[program_counter]);

    Instruction instr = {(*program_code)[program_counter++]};

    OpCode::Id opcode = instr.opcode.Value();
    auto instr_func = instr_table[static_cast<unsigned>(opcode)];

    if (instr_func) {
        // JIT the instruction!
        ((*this).*instr_func)(instr);
    } else {
        // Unhandled instruction
        LOG_CRITICAL(HW_GPU, "Unhandled instruction: 0x{:02x} (0x{:08x})",
                     static_cast<u32>(instr.opcode.Value().EffectiveOpCode()), instr.hex);
    }
}

void JitShader::FindReturnOffsets() {
    return_offsets.clear();

    for (std::size_t offset = 0; offset < program_code->size(); ++offset) {
        Instruction instr = {(*program_code)[offset]};

        switch (instr.opcode.Value()) {
        case OpCode::Id::CALL:
        case OpCode::Id::CALLC:
        case OpCode::Id::CALLU:
            return_offsets.push_back(instr.flow_control.dest_offset +
                                     instr.flow_control.num_instructions);
            break;
        default:
            break;
        }
    }

    // Sort for efficient binary search later
    std::sort(return_offsets.begin(), return_offsets.end());
}

void JitShader::Compile(const std::array<u32, MAX_PROGRAM_CODE_LENGTH>* program_code_,
                        const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>* swizzle_data_) {
    program_code = program_code_;
    swizzle_data = swizzle_data_;

    // Reset flow control state
    program = (CompiledShader*)getCurr();
    program_counter = 0;
    looping = false;
    instruction_labels.fill(Xbyak::Label());

    // Find all `CALL` instructions and identify return locations
    FindReturnOffsets();

    // The stack pointer is 8 modulo 16 at the entry of a procedure
    // We reserve 16 bytes and assign a dummy value to the first 8 bytes, to catch any potential
    // return checks (see Compile_Return) that happen in shader main routine.
    ABI_PushRegistersAndAdjustStack(*this, ABI_ALL_CALLEE_SAVED, 8, 16);
    mov(qword[rsp + 8], 0xFFFFFFFFFFFFFFFFULL);

    mov(UNIFORMS, ABI_PARAM1);
    mov(STATE, ABI_PARAM2);

    // Load address/loop registers
    movsxd(ADDROFFS_REG_0, dword[STATE + offsetof(UnitState, address_registers[0])]);
    movsxd(ADDROFFS_REG_1, dword[STATE + offsetof(UnitState, address_registers[1])]);
    mov(LOOPCOUNT_REG, dword[STATE + offsetof(UnitState, address_registers[2])]);
    shl(ADDROFFS_REG_0, 4);
    shl(ADDROFFS_REG_1, 4);
    shl(LOOPCOUNT_REG, 4);

    // Load conditional code
    mov(COND0, byte[STATE + offsetof(UnitState, conditional_code[0])]);
    mov(COND1, byte[STATE + offsetof(UnitState, conditional_code[1])]);

    // Used to set a register to one
    static const __m128 one = {1.f, 1.f, 1.f, 1.f};
    mov(rax, reinterpret_cast<std::size_t>(&one));
    movaps(ONE, xword[rax]);

    // Used to negate registers
    static const __m128 neg = {-0.f, -0.f, -0.f, -0.f};
    mov(rax, reinterpret_cast<std::size_t>(&neg));
    movaps(NEGBIT, xword[rax]);

    // Jump to start of the shader program
    jmp(ABI_PARAM3);

    // Compile entire program
    Compile_Block(static_cast<unsigned>(program_code->size()));

    // Free memory that's no longer needed
    program_code = nullptr;
    swizzle_data = nullptr;
    return_offsets.clear();
    return_offsets.shrink_to_fit();

    ready();

    ASSERT_MSG(getSize() <= MAX_SHADER_SIZE, "Compiled a shader that exceeds the allocated size!");
    LOG_DEBUG(HW_GPU, "Compiled shader size={}", getSize());
}

JitShader::JitShader() : Xbyak::CodeGenerator(MAX_SHADER_SIZE) {
    CompilePrelude();
}

void JitShader::CompilePrelude() {
    log2_subroutine = CompilePrelude_Log2();
    exp2_subroutine = CompilePrelude_Exp2();
}

Xbyak::Label JitShader::CompilePrelude_Log2() {
    Xbyak::Label subroutine;

    // SSE does not have a log instruction, thus we must approximate.
    // We perform this approximation first performaing a range reduction into the range [1.0, 2.0).
    // A minimax polynomial which was fit for the function log2(x) / (x - 1) is then evaluated.
    // We multiply the result by (x - 1) then restore the result into the appropriate range.

    // Coefficients for the minimax polynomial.
    // f(x) computes approximately log2(x) / (x - 1).
    // f(x) = c4 + x * (c3 + x * (c2 + x * (c1 + x * c0)).
    align(64);
    const void* c0 = getCurr();
    dd(0x3d74552f);
    const void* c1 = getCurr();
    dd(0xbeee7397);
    const void* c2 = getCurr();
    dd(0x3fbd96dd);
    const void* c3 = getCurr();
    dd(0xc02153f6);
    const void* c4 = getCurr();
    dd(0x4038d96c);

    align(16);
    const void* negative_infinity_vector = getCurr();
    dd(0xff800000);
    dd(0xff800000);
    dd(0xff800000);
    dd(0xff800000);
    const void* default_qnan_vector = getCurr();
    dd(0x7fc00000);
    dd(0x7fc00000);
    dd(0x7fc00000);
    dd(0x7fc00000);

    Xbyak::Label input_is_nan, input_is_zero, input_out_of_range;

    align(16);
    L(input_out_of_range);
    je(input_is_zero);
    movaps(SRC1, xword[rip + default_qnan_vector]);
    ret();
    L(input_is_zero);
    movaps(SRC1, xword[rip + negative_infinity_vector]);
    ret();

    align(16);
    L(subroutine);

    // Here we handle edge cases: input in {NaN, 0, -Inf, Negative}.
    xorps(SCRATCH, SCRATCH);
    ucomiss(SCRATCH, SRC1);
    jp(input_is_nan);
    jae(input_out_of_range);

    // Split input
    movd(eax, SRC1);
    mov(edx, eax);
    and_(eax, 0x7f800000);
    and_(edx, 0x007fffff);
    movss(SCRATCH, xword[rip + c0]); // Preload c0.
    or_(edx, 0x3f800000);
    movd(SRC1, edx);
    // SRC1 now contains the mantissa of the input.
    mulss(SCRATCH, SRC1);
    shr(eax, 23);
    sub(eax, 0x7f);
    cvtsi2ss(SCRATCH2, eax);
    // SCRATCH2 now contains the exponent of the input.

    // Complete computation of polynomial
    addss(SCRATCH, xword[rip + c1]);
    mulss(SCRATCH, SRC1);
    addss(SCRATCH, xword[rip + c2]);
    mulss(SCRATCH, SRC1);
    addss(SCRATCH, xword[rip + c3]);
    mulss(SCRATCH, SRC1);
    subss(SRC1, ONE);
    addss(SCRATCH, xword[rip + c4]);
    mulss(SCRATCH, SRC1);
    addss(SCRATCH2, SCRATCH);

    // Duplicate result across vector
    xorps(SRC1, SRC1); // break dependency chain
    movss(SRC1, SCRATCH2);
    L(input_is_nan);
    shufps(SRC1, SRC1, _MM_SHUFFLE(0, 0, 0, 0));

    ret();

    return subroutine;
}

Xbyak::Label JitShader::CompilePrelude_Exp2() {
    Xbyak::Label subroutine;

    // SSE does not have a exp instruction, thus we must approximate.
    // We perform this approximation first performaing a range reduction into the range [-0.5, 0.5).
    // A minimax polynomial which was fit for the function exp2(x) is then evaluated.
    // We then restore the result into the appropriate range.

    align(64);
    const void* input_max = getCurr();
    dd(0x43010000);
    const void* input_min = getCurr();
    dd(0xc2fdffff);
    const void* c0 = getCurr();
    dd(0x3c5dbe69);
    const void* half = getCurr();
    dd(0x3f000000);
    const void* c1 = getCurr();
    dd(0x3d5509f9);
    const void* c2 = getCurr();
    dd(0x3e773cc5);
    const void* c3 = getCurr();
    dd(0x3f3168b3);
    const void* c4 = getCurr();
    dd(0x3f800016);

    Xbyak::Label ret_label;

    align(16);
    L(subroutine);

    // Handle edge cases
    ucomiss(SRC1, SRC1);
    jp(ret_label);
    // Clamp to maximum range since we shift the value directly into the exponent.
    minss(SRC1, xword[rip + input_max]);
    maxss(SRC1, xword[rip + input_min]);

    // Decompose input
    movss(SCRATCH, SRC1);
    movss(SCRATCH2, xword[rip + c0]); // Preload c0.
    subss(SCRATCH, xword[rip + half]);
    cvtss2si(eax, SCRATCH);
    cvtsi2ss(SCRATCH, eax);
    // SCRATCH now contains input rounded to the nearest integer.
    add(eax, 0x7f);
    subss(SRC1, SCRATCH);
    // SRC1 contains input - round(input), which is in [-0.5, 0.5).
    mulss(SCRATCH2, SRC1);
    shl(eax, 23);
    movd(SCRATCH, eax);
    // SCRATCH contains 2^(round(input)).

    // Complete computation of polynomial.
    addss(SCRATCH2, xword[rip + c1]);
    mulss(SCRATCH2, SRC1);
    addss(SCRATCH2, xword[rip + c2]);
    mulss(SCRATCH2, SRC1);
    addss(SCRATCH2, xword[rip + c3]);
    mulss(SRC1, SCRATCH2);
    addss(SRC1, xword[rip + c4]);
    mulss(SRC1, SCRATCH);

    // Duplicate result across vector
    L(ret_label);
    shufps(SRC1, SRC1, _MM_SHUFFLE(0, 0, 0, 0));

    ret();

    return subroutine;
}

} // namespace Pica::Shader
