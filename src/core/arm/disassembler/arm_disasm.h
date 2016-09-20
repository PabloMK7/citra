// Copyright 2006 The Android Open Source Project

#pragma once

#include <string>
#include "common/common_types.h"

// Note: this list of opcodes must match the list used to initialize
// the opflags[] array in opcode.cpp.
enum Opcode {
    OP_INVALID,
    OP_UNDEFINED,
    OP_ADC,
    OP_ADD,
    OP_AND,
    OP_B,
    OP_BL,
    OP_BIC,
    OP_BKPT,
    OP_BLX,
    OP_BX,
    OP_CDP,
    OP_CLREX,
    OP_CLZ,
    OP_CMN,
    OP_CMP,
    OP_EOR,
    OP_LDC,
    OP_LDM,
    OP_LDR,
    OP_LDRB,
    OP_LDRBT,
    OP_LDREX,
    OP_LDREXB,
    OP_LDREXD,
    OP_LDREXH,
    OP_LDRH,
    OP_LDRSB,
    OP_LDRSH,
    OP_LDRT,
    OP_MCR,
    OP_MLA,
    OP_MOV,
    OP_MRC,
    OP_MRS,
    OP_MSR,
    OP_MUL,
    OP_MVN,
    OP_NOP,
    OP_ORR,
    OP_PKH,
    OP_PLD,
    OP_QADD16,
    OP_QADD8,
    OP_QASX,
    OP_QSAX,
    OP_QSUB16,
    OP_QSUB8,
    OP_REV,
    OP_REV16,
    OP_REVSH,
    OP_RSB,
    OP_RSC,
    OP_SADD16,
    OP_SADD8,
    OP_SASX,
    OP_SBC,
    OP_SEL,
    OP_SEV,
    OP_SHADD16,
    OP_SHADD8,
    OP_SHASX,
    OP_SHSAX,
    OP_SHSUB16,
    OP_SHSUB8,
    OP_SMLAD,
    OP_SMLAL,
    OP_SMLALD,
    OP_SMLSD,
    OP_SMLSLD,
    OP_SMMLA,
    OP_SMMLS,
    OP_SMMUL,
    OP_SMUAD,
    OP_SMULL,
    OP_SMUSD,
    OP_SSAT,
    OP_SSAT16,
    OP_SSAX,
    OP_SSUB16,
    OP_SSUB8,
    OP_STC,
    OP_STM,
    OP_STR,
    OP_STRB,
    OP_STRBT,
    OP_STREX,
    OP_STREXB,
    OP_STREXD,
    OP_STREXH,
    OP_STRH,
    OP_STRT,
    OP_SUB,
    OP_SWI,
    OP_SWP,
    OP_SWPB,
    OP_SXTAB,
    OP_SXTAB16,
    OP_SXTAH,
    OP_SXTB,
    OP_SXTB16,
    OP_SXTH,
    OP_TEQ,
    OP_TST,
    OP_UADD16,
    OP_UADD8,
    OP_UASX,
    OP_UHADD16,
    OP_UHADD8,
    OP_UHASX,
    OP_UHSAX,
    OP_UHSUB16,
    OP_UHSUB8,
    OP_UMLAL,
    OP_UMULL,
    OP_UQADD16,
    OP_UQADD8,
    OP_UQASX,
    OP_UQSAX,
    OP_UQSUB16,
    OP_UQSUB8,
    OP_USAD8,
    OP_USADA8,
    OP_USAT,
    OP_USAT16,
    OP_USAX,
    OP_USUB16,
    OP_USUB8,
    OP_UXTAB,
    OP_UXTAB16,
    OP_UXTAH,
    OP_UXTB,
    OP_UXTB16,
    OP_UXTH,
    OP_WFE,
    OP_WFI,
    OP_YIELD,

    // Define thumb opcodes
    OP_THUMB_UNDEFINED,
    OP_THUMB_ADC,
    OP_THUMB_ADD,
    OP_THUMB_AND,
    OP_THUMB_ASR,
    OP_THUMB_B,
    OP_THUMB_BIC,
    OP_THUMB_BKPT,
    OP_THUMB_BL,
    OP_THUMB_BLX,
    OP_THUMB_BX,
    OP_THUMB_CMN,
    OP_THUMB_CMP,
    OP_THUMB_EOR,
    OP_THUMB_LDMIA,
    OP_THUMB_LDR,
    OP_THUMB_LDRB,
    OP_THUMB_LDRH,
    OP_THUMB_LDRSB,
    OP_THUMB_LDRSH,
    OP_THUMB_LSL,
    OP_THUMB_LSR,
    OP_THUMB_MOV,
    OP_THUMB_MUL,
    OP_THUMB_MVN,
    OP_THUMB_NEG,
    OP_THUMB_ORR,
    OP_THUMB_POP,
    OP_THUMB_PUSH,
    OP_THUMB_ROR,
    OP_THUMB_SBC,
    OP_THUMB_STMIA,
    OP_THUMB_STR,
    OP_THUMB_STRB,
    OP_THUMB_STRH,
    OP_THUMB_SUB,
    OP_THUMB_SWI,
    OP_THUMB_TST,

    OP_END // must be last
};

class ARM_Disasm {
public:
    static std::string Disassemble(u32 addr, u32 insn);
    static Opcode Decode(u32 insn);

private:
    static Opcode Decode00(u32 insn);
    static Opcode Decode01(u32 insn);
    static Opcode Decode10(u32 insn);
    static Opcode Decode11(u32 insn);
    static Opcode DecodeSyncPrimitive(u32 insn);
    static Opcode DecodeParallelAddSub(u32 insn);
    static Opcode DecodePackingSaturationReversal(u32 insn);
    static Opcode DecodeMUL(u32 insn);
    static Opcode DecodeMSRImmAndHints(u32 insn);
    static Opcode DecodeMediaMulDiv(u32 insn);
    static Opcode DecodeMedia(u32 insn);
    static Opcode DecodeLDRH(u32 insn);
    static Opcode DecodeALU(u32 insn);

    static std::string DisassembleALU(Opcode opcode, u32 insn);
    static std::string DisassembleBranch(u32 addr, Opcode opcode, u32 insn);
    static std::string DisassembleBX(u32 insn);
    static std::string DisassembleBKPT(u32 insn);
    static std::string DisassembleCLZ(u32 insn);
    static std::string DisassembleMediaMulDiv(Opcode opcode, u32 insn);
    static std::string DisassembleMemblock(Opcode opcode, u32 insn);
    static std::string DisassembleMem(u32 insn);
    static std::string DisassembleMemHalf(u32 insn);
    static std::string DisassembleMCR(Opcode opcode, u32 insn);
    static std::string DisassembleMLA(Opcode opcode, u32 insn);
    static std::string DisassembleUMLAL(Opcode opcode, u32 insn);
    static std::string DisassembleMUL(Opcode opcode, u32 insn);
    static std::string DisassembleMRS(u32 insn);
    static std::string DisassembleMSR(u32 insn);
    static std::string DisassembleNoOperands(Opcode opcode, u32 insn);
    static std::string DisassembleParallelAddSub(Opcode opcode, u32 insn);
    static std::string DisassemblePKH(u32 insn);
    static std::string DisassemblePLD(u32 insn);
    static std::string DisassembleREV(Opcode opcode, u32 insn);
    static std::string DisassembleREX(Opcode opcode, u32 insn);
    static std::string DisassembleSAT(Opcode opcode, u32 insn);
    static std::string DisassembleSEL(u32 insn);
    static std::string DisassembleSWI(u32 insn);
    static std::string DisassembleSWP(Opcode opcode, u32 insn);
    static std::string DisassembleXT(Opcode opcode, u32 insn);
};
