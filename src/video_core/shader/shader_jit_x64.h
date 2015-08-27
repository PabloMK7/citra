// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <nihstro/shader_bytecode.h>

#include "common/x64/emitter.h"

#include "video_core/pica.h"

#include "shader.h"

using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::SwizzlePattern;

namespace Pica {

namespace Shader {

using CompiledShader = void(void* registers);

/**
 * This class implements the shader JIT compiler. It recompiles a Pica shader program into x86_64
 * code that can be executed on the host machine directly.
 */
class JitCompiler : public Gen::XCodeBlock {
public:
    JitCompiler();

    CompiledShader* Compile();

    void Clear();

    void Compile_ADD(Instruction instr);
    void Compile_DP3(Instruction instr);
    void Compile_DP4(Instruction instr);
    void Compile_DPH(Instruction instr);
    void Compile_EX2(Instruction instr);
    void Compile_LG2(Instruction instr);
    void Compile_MUL(Instruction instr);
    void Compile_SGE(Instruction instr);
    void Compile_SLT(Instruction instr);
    void Compile_FLR(Instruction instr);
    void Compile_MAX(Instruction instr);
    void Compile_MIN(Instruction instr);
    void Compile_RCP(Instruction instr);
    void Compile_RSQ(Instruction instr);
    void Compile_MOVA(Instruction instr);
    void Compile_MOV(Instruction instr);
    void Compile_NOP(Instruction instr);
    void Compile_END(Instruction instr);
    void Compile_CALL(Instruction instr);
    void Compile_CALLC(Instruction instr);
    void Compile_CALLU(Instruction instr);
    void Compile_IF(Instruction instr);
    void Compile_LOOP(Instruction instr);
    void Compile_JMP(Instruction instr);
    void Compile_CMP(Instruction instr);
    void Compile_MAD(Instruction instr);

private:
    void Compile_Block(unsigned stop);
    void Compile_NextInstr(unsigned* offset);

    void Compile_SwizzleSrc(Instruction instr, unsigned src_num, SourceRegister src_reg, Gen::X64Reg dest);
    void Compile_DestEnable(Instruction instr, Gen::X64Reg dest);

    /**
     * Compiles a `MUL src1, src2` operation, properly handling the PICA semantics when multiplying
     * zero by inf. Clobbers `src2` and `scratch`.
     */
    void Compile_SanitizedMul(Gen::X64Reg src1, Gen::X64Reg src2, Gen::X64Reg scratch);

    void Compile_EvaluateCondition(Instruction instr);
    void Compile_UniformCondition(Instruction instr);

    void Compile_PushCallerSavedXMM();
    void Compile_PopCallerSavedXMM();

    /// Pointer to the variable that stores the current Pica code offset. Used to handle nested code blocks.
    unsigned* offset_ptr = nullptr;

    /// Set to true if currently in a loop, used to check for the existence of nested loops
    bool looping = false;
};

} // Shader

} // Pica
