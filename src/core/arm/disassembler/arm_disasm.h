// Copyright 2006 The Android Open Source Project

#pragma once

#include <cstdint>
#include <string>

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
    OP_CLZ,
    OP_CMN,
    OP_CMP,
    OP_EOR,
    OP_LDC,
    OP_LDM,
    OP_LDR,
    OP_LDRB,
    OP_LDRBT,
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
    OP_ORR,
    OP_PLD,
    OP_RSB,
    OP_RSC,
    OP_SBC,
    OP_SMLAL,
    OP_SMULL,
    OP_STC,
    OP_STM,
    OP_STR,
    OP_STRB,
    OP_STRBT,
    OP_STRH,
    OP_STRT,
    OP_SUB,
    OP_SWI,
    OP_SWP,
    OP_SWPB,
    OP_TEQ,
    OP_TST,
    OP_UMLAL,
    OP_UMULL,

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

    OP_END                // must be last
};

class ARM_Disasm {
 public:
  static std::string Disassemble(uint32_t addr, uint32_t insn);
  static Opcode Decode(uint32_t insn);

 private:
  static Opcode Decode00(uint32_t insn);
  static Opcode Decode01(uint32_t insn);
  static Opcode Decode10(uint32_t insn);
  static Opcode Decode11(uint32_t insn);
  static Opcode DecodeMUL(uint32_t insn);
  static Opcode DecodeLDRH(uint32_t insn);
  static Opcode DecodeALU(uint32_t insn);

  static std::string DisassembleALU(Opcode opcode, uint32_t insn);
  static std::string DisassembleBranch(uint32_t addr, Opcode opcode, uint32_t insn);
  static std::string DisassembleBX(uint32_t insn);
  static std::string DisassembleBKPT(uint32_t insn);
  static std::string DisassembleCLZ(uint32_t insn);
  static std::string DisassembleMemblock(Opcode opcode, uint32_t insn);
  static std::string DisassembleMem(uint32_t insn);
  static std::string DisassembleMemHalf(uint32_t insn);
  static std::string DisassembleMCR(Opcode opcode, uint32_t insn);
  static std::string DisassembleMLA(Opcode opcode, uint32_t insn);
  static std::string DisassembleUMLAL(Opcode opcode, uint32_t insn);
  static std::string DisassembleMUL(Opcode opcode, uint32_t insn);
  static std::string DisassembleMRS(uint32_t insn);
  static std::string DisassembleMSR(uint32_t insn);
  static std::string DisassemblePLD(uint32_t insn);
  static std::string DisassembleSWI(uint32_t insn);
  static std::string DisassembleSWP(Opcode opcode, uint32_t insn);
};
