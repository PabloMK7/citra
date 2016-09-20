// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>
#include <nihstro/shader_bytecode.h>
#include "common/bit_set.h"
#include "common/common_types.h"
#include "common/x64/emitter.h"
#include "video_core/shader/shader.h"

using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::SwizzlePattern;

namespace Pica {

namespace Shader {

/// Memory allocated for each compiled shader (64Kb)
constexpr size_t MAX_SHADER_SIZE = 1024 * 64;

/**
 * This class implements the shader JIT compiler. It recompiles a Pica shader program into x86_64
 * code that can be executed on the host machine directly.
 */
class JitShader : public Gen::XCodeBlock {
public:
    JitShader();

    void Run(const ShaderSetup& setup, UnitState<false>& state, unsigned offset) const {
        program(&setup, &state, code_ptr[offset]);
    }

    void Compile();

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
    void Compile_Block(unsigned end);
    void Compile_NextInstr();

    void Compile_SwizzleSrc(Instruction instr, unsigned src_num, SourceRegister src_reg,
                            Gen::X64Reg dest);
    void Compile_DestEnable(Instruction instr, Gen::X64Reg dest);

    /**
     * Compiles a `MUL src1, src2` operation, properly handling the PICA semantics when multiplying
     * zero by inf. Clobbers `src2` and `scratch`.
     */
    void Compile_SanitizedMul(Gen::X64Reg src1, Gen::X64Reg src2, Gen::X64Reg scratch);

    void Compile_EvaluateCondition(Instruction instr);
    void Compile_UniformCondition(Instruction instr);

    /**
     * Emits the code to conditionally return from a subroutine envoked by the `CALL` instruction.
     */
    void Compile_Return();

    BitSet32 PersistentCallerSavedRegs();

    /**
     * Assertion evaluated at compile-time, but only triggered if executed at runtime.
     * @param msg Message to be logged if the assertion fails.
     */
    void Compile_Assert(bool condition, const char* msg);

    /**
     * Analyzes the entire shader program for `CALL` instructions before emitting any code,
     * identifying the locations where a return needs to be inserted.
     */
    void FindReturnOffsets();

    /// Mapping of Pica VS instructions to pointers in the emitted code
    std::array<const u8*, 1024> code_ptr;

    /// Offsets in code where a return needs to be inserted
    std::vector<unsigned> return_offsets;

    unsigned program_counter = 0; ///< Offset of the next instruction to decode
    bool looping = false;         ///< True if compiling a loop, used to check for nested loops

    /// Branches that need to be fixed up once the entire shader program is compiled
    std::vector<std::pair<Gen::FixupBranch, unsigned>> fixup_branches;

    using CompiledShader = void(const void* setup, void* state, const u8* start_addr);
    CompiledShader* program = nullptr;
};

} // Shader

} // Pica
