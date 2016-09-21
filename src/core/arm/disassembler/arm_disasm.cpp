// Copyright 2006 The Android Open Source Project

#include <string>
#include <unordered_set>
#include "common/common_types.h"
#include "common/string_util.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/arm/skyeye_common/armsupp.h"

static const char* cond_names[] = {"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
                                   "hi", "ls", "ge", "lt", "gt", "le", "",   "RESERVED"};

static const char* opcode_names[] = {
    "invalid",   "undefined", "adc",    "add",     "and",    "b",      "bl",     "bic",
    "bkpt",      "blx",       "bx",     "cdp",     "clrex",  "clz",    "cmn",    "cmp",
    "eor",       "ldc",       "ldm",    "ldr",     "ldrb",   "ldrbt",  "ldrex",  "ldrexb",
    "ldrexd",    "ldrexh",    "ldrh",   "ldrsb",   "ldrsh",  "ldrt",   "mcr",    "mla",
    "mov",       "mrc",       "mrs",    "msr",     "mul",    "mvn",    "nop",    "orr",
    "pkh",       "pld",       "qadd16", "qadd8",   "qasx",   "qsax",   "qsub16", "qsub8",
    "rev",       "rev16",     "revsh",  "rsb",     "rsc",    "sadd16", "sadd8",  "sasx",
    "sbc",       "sel",       "sev",    "shadd16", "shadd8", "shasx",  "shsax",  "shsub16",
    "shsub8",    "smlad",     "smlal",  "smlald",  "smlsd",  "smlsld", "smmla",  "smmls",
    "smmul",     "smuad",     "smull",  "smusd",   "ssat",   "ssat16", "ssax",   "ssub16",
    "ssub8",     "stc",       "stm",    "str",     "strb",   "strbt",  "strex",  "strexb",
    "strexd",    "strexh",    "strh",   "strt",    "sub",    "swi",    "swp",    "swpb",
    "sxtab",     "sxtab16",   "sxtah",  "sxtb",    "sxtb16", "sxth",   "teq",    "tst",
    "uadd16",    "uadd8",     "uasx",   "uhadd16", "uhadd8", "uhasx",  "uhsax",  "uhsub16",
    "uhsub8",    "umlal",     "umull",  "uqadd16", "uqadd8", "uqasx",  "uqsax",  "uqsub16",
    "uqsub8",    "usad8",     "usada8", "usat",    "usat16", "usax",   "usub16", "usub8",
    "uxtab",     "uxtab16",   "uxtah",  "uxtb",    "uxtb16", "uxth",   "wfe",    "wfi",
    "yield",

    "undefined", "adc",       "add",    "and",     "asr",    "b",      "bic",    "bkpt",
    "bl",        "blx",       "bx",     "cmn",     "cmp",    "eor",    "ldmia",  "ldr",
    "ldrb",      "ldrh",      "ldrsb",  "ldrsh",   "lsl",    "lsr",    "mov",    "mul",
    "mvn",       "neg",       "orr",    "pop",     "push",   "ror",    "sbc",    "stmia",
    "str",       "strb",      "strh",   "sub",     "swi",    "tst",

    nullptr};

// Indexed by the shift type (bits 6-5)
static const char* shift_names[] = {"LSL", "LSR", "ASR", "ROR"};

static const char* cond_to_str(u32 cond) {
    return cond_names[cond];
}

std::string ARM_Disasm::Disassemble(u32 addr, u32 insn) {
    Opcode opcode = Decode(insn);
    switch (opcode) {
    case OP_INVALID:
        return "Invalid";
    case OP_UNDEFINED:
        return "Undefined";
    case OP_ADC:
    case OP_ADD:
    case OP_AND:
    case OP_BIC:
    case OP_CMN:
    case OP_CMP:
    case OP_EOR:
    case OP_MOV:
    case OP_MVN:
    case OP_ORR:
    case OP_RSB:
    case OP_RSC:
    case OP_SBC:
    case OP_SUB:
    case OP_TEQ:
    case OP_TST:
        return DisassembleALU(opcode, insn);
    case OP_B:
    case OP_BL:
        return DisassembleBranch(addr, opcode, insn);
    case OP_BKPT:
        return DisassembleBKPT(insn);
    case OP_BLX:
        // not supported yet
        break;
    case OP_BX:
        return DisassembleBX(insn);
    case OP_CDP:
        return "cdp";
    case OP_CLREX:
        return "clrex";
    case OP_CLZ:
        return DisassembleCLZ(insn);
    case OP_LDC:
        return "ldc";
    case OP_LDM:
    case OP_STM:
        return DisassembleMemblock(opcode, insn);
    case OP_LDR:
    case OP_LDRB:
    case OP_LDRBT:
    case OP_LDRT:
    case OP_STR:
    case OP_STRB:
    case OP_STRBT:
    case OP_STRT:
        return DisassembleMem(insn);
    case OP_LDREX:
    case OP_LDREXB:
    case OP_LDREXD:
    case OP_LDREXH:
    case OP_STREX:
    case OP_STREXB:
    case OP_STREXD:
    case OP_STREXH:
        return DisassembleREX(opcode, insn);
    case OP_LDRH:
    case OP_LDRSB:
    case OP_LDRSH:
    case OP_STRH:
        return DisassembleMemHalf(insn);
    case OP_MCR:
    case OP_MRC:
        return DisassembleMCR(opcode, insn);
    case OP_MLA:
        return DisassembleMLA(opcode, insn);
    case OP_MRS:
        return DisassembleMRS(insn);
    case OP_MSR:
        return DisassembleMSR(insn);
    case OP_MUL:
        return DisassembleMUL(opcode, insn);
    case OP_NOP:
    case OP_SEV:
    case OP_WFE:
    case OP_WFI:
    case OP_YIELD:
        return DisassembleNoOperands(opcode, insn);
    case OP_PKH:
        return DisassemblePKH(insn);
    case OP_PLD:
        return DisassemblePLD(insn);
    case OP_QADD16:
    case OP_QADD8:
    case OP_QASX:
    case OP_QSAX:
    case OP_QSUB16:
    case OP_QSUB8:
    case OP_SADD16:
    case OP_SADD8:
    case OP_SASX:
    case OP_SHADD16:
    case OP_SHADD8:
    case OP_SHASX:
    case OP_SHSAX:
    case OP_SHSUB16:
    case OP_SHSUB8:
    case OP_SSAX:
    case OP_SSUB16:
    case OP_SSUB8:
    case OP_UADD16:
    case OP_UADD8:
    case OP_UASX:
    case OP_UHADD16:
    case OP_UHADD8:
    case OP_UHASX:
    case OP_UHSAX:
    case OP_UHSUB16:
    case OP_UHSUB8:
    case OP_UQADD16:
    case OP_UQADD8:
    case OP_UQASX:
    case OP_UQSAX:
    case OP_UQSUB16:
    case OP_UQSUB8:
    case OP_USAX:
    case OP_USUB16:
    case OP_USUB8:
        return DisassembleParallelAddSub(opcode, insn);
    case OP_REV:
    case OP_REV16:
    case OP_REVSH:
        return DisassembleREV(opcode, insn);
    case OP_SEL:
        return DisassembleSEL(insn);
    case OP_SMLAD:
    case OP_SMLALD:
    case OP_SMLSD:
    case OP_SMLSLD:
    case OP_SMMLA:
    case OP_SMMLS:
    case OP_SMMUL:
    case OP_SMUAD:
    case OP_SMUSD:
    case OP_USAD8:
    case OP_USADA8:
        return DisassembleMediaMulDiv(opcode, insn);
    case OP_SSAT:
    case OP_SSAT16:
    case OP_USAT:
    case OP_USAT16:
        return DisassembleSAT(opcode, insn);
    case OP_STC:
        return "stc";
    case OP_SWI:
        return DisassembleSWI(insn);
    case OP_SWP:
    case OP_SWPB:
        return DisassembleSWP(opcode, insn);
    case OP_SXTAB:
    case OP_SXTAB16:
    case OP_SXTAH:
    case OP_SXTB:
    case OP_SXTB16:
    case OP_SXTH:
    case OP_UXTAB:
    case OP_UXTAB16:
    case OP_UXTAH:
    case OP_UXTB:
    case OP_UXTB16:
    case OP_UXTH:
        return DisassembleXT(opcode, insn);
    case OP_UMLAL:
    case OP_UMULL:
    case OP_SMLAL:
    case OP_SMULL:
        return DisassembleUMLAL(opcode, insn);
    default:
        return "Error";
    }
    return nullptr;
}

std::string ARM_Disasm::DisassembleALU(Opcode opcode, u32 insn) {
    static const u8 kNoOperand1 = 1;
    static const u8 kNoDest = 2;
    static const u8 kNoSbit = 4;

    std::string rn_str;
    std::string rd_str;

    u8 flags = 0;
    u8 cond = (insn >> 28) & 0xf;
    u8 is_immed = (insn >> 25) & 0x1;
    u8 bit_s = (insn >> 20) & 1;
    u8 rn = (insn >> 16) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u8 immed = insn & 0xff;

    const char* opname = opcode_names[opcode];
    switch (opcode) {
    case OP_CMN:
    case OP_CMP:
    case OP_TEQ:
    case OP_TST:
        flags = kNoDest | kNoSbit;
        break;
    case OP_MOV:
    case OP_MVN:
        flags = kNoOperand1;
        break;
    default:
        break;
    }

    // The "mov" instruction ignores the first operand (rn).
    rn_str[0] = 0;
    if ((flags & kNoOperand1) == 0) {
        rn_str = Common::StringFromFormat("r%d, ", rn);
    }

    // The following instructions do not write the result register (rd):
    // tst, teq, cmp, cmn.
    rd_str[0] = 0;
    if ((flags & kNoDest) == 0) {
        rd_str = Common::StringFromFormat("r%d, ", rd);
    }

    const char* sbit_str = "";
    if (bit_s && !(flags & kNoSbit))
        sbit_str = "s";

    if (is_immed) {
        return Common::StringFromFormat("%s%s%s\t%s%s#%u  ; 0x%x", opname, cond_to_str(cond),
                                        sbit_str, rd_str.c_str(), rn_str.c_str(), immed, immed);
    }

    u8 shift_is_reg = (insn >> 4) & 1;
    u8 rotate = (insn >> 8) & 0xf;
    u8 rm = insn & 0xf;
    u8 shift_type = (insn >> 5) & 0x3;
    u8 rs = (insn >> 8) & 0xf;
    u8 shift_amount = (insn >> 7) & 0x1f;
    u32 rotated_val = immed;
    u8 rotate2 = rotate << 1;
    rotated_val = (rotated_val >> rotate2) | (rotated_val << (32 - rotate2));

    if (!shift_is_reg && shift_type == 0 && shift_amount == 0) {
        return Common::StringFromFormat("%s%s%s\t%s%sr%d", opname, cond_to_str(cond), sbit_str,
                                        rd_str.c_str(), rn_str.c_str(), rm);
    }

    const char* shift_name = shift_names[shift_type];
    if (shift_is_reg) {
        return Common::StringFromFormat("%s%s%s\t%s%sr%d, %s r%d", opname, cond_to_str(cond),
                                        sbit_str, rd_str.c_str(), rn_str.c_str(), rm, shift_name,
                                        rs);
    }
    if (shift_amount == 0) {
        if (shift_type == 3) {
            return Common::StringFromFormat("%s%s%s\t%s%sr%d, RRX", opname, cond_to_str(cond),
                                            sbit_str, rd_str.c_str(), rn_str.c_str(), rm);
        }
        shift_amount = 32;
    }
    return Common::StringFromFormat("%s%s%s\t%s%sr%d, %s #%u", opname, cond_to_str(cond), sbit_str,
                                    rd_str.c_str(), rn_str.c_str(), rm, shift_name, shift_amount);
}

std::string ARM_Disasm::DisassembleBranch(u32 addr, Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u32 offset = insn & 0xffffff;
    // Sign-extend the 24-bit offset
    if ((offset >> 23) & 1)
        offset |= 0xff000000;

    // Pre-compute the left-shift and the prefetch offset
    offset <<= 2;
    offset += 8;
    addr += offset;
    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\t0x%x", opname, cond_to_str(cond), addr);
}

std::string ARM_Disasm::DisassembleBX(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rn = insn & 0xf;
    return Common::StringFromFormat("bx%s\tr%d", cond_to_str(cond), rn);
}

std::string ARM_Disasm::DisassembleBKPT(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u32 immed = (((insn >> 8) & 0xfff) << 4) | (insn & 0xf);
    return Common::StringFromFormat("bkpt%s\t#%d", cond_to_str(cond), immed);
}

std::string ARM_Disasm::DisassembleCLZ(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u8 rm = insn & 0xf;
    return Common::StringFromFormat("clz%s\tr%d, r%d", cond_to_str(cond), rd, rm);
}

std::string ARM_Disasm::DisassembleMediaMulDiv(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rd = BITS(insn, 16, 19);
    u32 ra = BITS(insn, 12, 15);
    u32 rm = BITS(insn, 8, 11);
    u32 m = BIT(insn, 5);
    u32 rn = BITS(insn, 0, 3);

    std::string cross = "";
    if (m) {
        if (opcode == OP_SMMLA || opcode == OP_SMMUL || opcode == OP_SMMLS)
            cross = "r";
        else
            cross = "x";
    }

    std::string ext_reg = "";
    std::unordered_set<Opcode, std::hash<int>> with_ext_reg = {OP_SMLAD, OP_SMLSD, OP_SMMLA,
                                                               OP_SMMLS, OP_USADA8};
    if (with_ext_reg.find(opcode) != with_ext_reg.end())
        ext_reg = Common::StringFromFormat(", r%u", ra);

    std::string rd_low = "";
    if (opcode == OP_SMLALD || opcode == OP_SMLSLD)
        rd_low = Common::StringFromFormat("r%u, ", ra);

    return Common::StringFromFormat("%s%s%s\t%sr%u, r%u, r%u%s", opcode_names[opcode],
                                    cross.c_str(), cond_to_str(cond), rd_low.c_str(), rd, rn, rm,
                                    ext_reg.c_str());
}

std::string ARM_Disasm::DisassembleMemblock(Opcode opcode, u32 insn) {
    std::string tmp_list;

    u8 cond = (insn >> 28) & 0xf;
    u8 write_back = (insn >> 21) & 0x1;
    u8 bit_s = (insn >> 22) & 0x1;
    u8 is_up = (insn >> 23) & 0x1;
    u8 is_pre = (insn >> 24) & 0x1;
    u8 rn = (insn >> 16) & 0xf;
    u16 reg_list = insn & 0xffff;

    const char* opname = opcode_names[opcode];

    const char* bang = "";
    if (write_back)
        bang = "!";

    const char* carret = "";
    if (bit_s)
        carret = "^";

    const char* comma = "";
    tmp_list[0] = 0;
    for (int ii = 0; ii < 16; ++ii) {
        if (reg_list & (1 << ii)) {
            tmp_list += Common::StringFromFormat("%sr%d", comma, ii);
            comma = ",";
        }
    }

    const char* addr_mode = "";
    if (is_pre) {
        if (is_up) {
            addr_mode = "ib";
        } else {
            addr_mode = "db";
        }
    } else {
        if (is_up) {
            addr_mode = "ia";
        } else {
            addr_mode = "da";
        }
    }

    return Common::StringFromFormat("%s%s%s\tr%d%s, {%s}%s", opname, cond_to_str(cond), addr_mode,
                                    rn, bang, tmp_list.c_str(), carret);
}

std::string ARM_Disasm::DisassembleMem(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 is_reg = (insn >> 25) & 0x1;
    u8 is_load = (insn >> 20) & 0x1;
    u8 write_back = (insn >> 21) & 0x1;
    u8 is_byte = (insn >> 22) & 0x1;
    u8 is_up = (insn >> 23) & 0x1;
    u8 is_pre = (insn >> 24) & 0x1;
    u8 rn = (insn >> 16) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u16 offset = insn & 0xfff;

    const char* opname = "ldr";
    if (!is_load)
        opname = "str";

    const char* bang = "";
    if (write_back)
        bang = "!";

    const char* minus = "";
    if (is_up == 0)
        minus = "-";

    const char* byte = "";
    if (is_byte)
        byte = "b";

    if (is_reg == 0) {
        if (is_pre) {
            if (offset == 0) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d]", opname, cond_to_str(cond),
                                                byte, rd, rn);
            } else {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, #%s%u]%s", opname,
                                                cond_to_str(cond), byte, rd, rn, minus, offset,
                                                bang);
            }
        } else {
            const char* transfer = "";
            if (write_back)
                transfer = "t";

            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], #%s%u", opname,
                                            cond_to_str(cond), byte, transfer, rd, rn, minus,
                                            offset);
        }
    }

    u8 rm = insn & 0xf;
    u8 shift_type = (insn >> 5) & 0x3;
    u8 shift_amount = (insn >> 7) & 0x1f;

    const char* shift_name = shift_names[shift_type];

    if (is_pre) {
        if (shift_amount == 0) {
            if (shift_type == 0) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d]%s", opname,
                                                cond_to_str(cond), byte, rd, rn, minus, rm, bang);
            }
            if (shift_type == 3) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d, RRX]%s", opname,
                                                cond_to_str(cond), byte, rd, rn, minus, rm, bang);
            }
            shift_amount = 32;
        }
        return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d, %s #%u]%s", opname,
                                        cond_to_str(cond), byte, rd, rn, minus, rm, shift_name,
                                        shift_amount, bang);
    }

    const char* transfer = "";
    if (write_back)
        transfer = "t";

    if (shift_amount == 0) {
        if (shift_type == 0) {
            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d", opname,
                                            cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
        }
        if (shift_type == 3) {
            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d, RRX", opname,
                                            cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
        }
        shift_amount = 32;
    }

    return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d, %s #%u", opname,
                                    cond_to_str(cond), byte, transfer, rd, rn, minus, rm,
                                    shift_name, shift_amount);
}

std::string ARM_Disasm::DisassembleMemHalf(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 is_load = (insn >> 20) & 0x1;
    u8 write_back = (insn >> 21) & 0x1;
    u8 is_immed = (insn >> 22) & 0x1;
    u8 is_up = (insn >> 23) & 0x1;
    u8 is_pre = (insn >> 24) & 0x1;
    u8 rn = (insn >> 16) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u8 bits_65 = (insn >> 5) & 0x3;
    u8 rm = insn & 0xf;
    u8 offset = (((insn >> 8) & 0xf) << 4) | (insn & 0xf);

    const char* opname = "ldr";
    if (is_load == 0)
        opname = "str";

    const char* width = "";
    if (bits_65 == 1)
        width = "h";
    else if (bits_65 == 2)
        width = "sb";
    else
        width = "sh";

    const char* bang = "";
    if (write_back)
        bang = "!";
    const char* minus = "";
    if (is_up == 0)
        minus = "-";

    if (is_immed) {
        if (is_pre) {
            if (offset == 0) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d]", opname, cond_to_str(cond),
                                                width, rd, rn);
            } else {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, #%s%u]%s", opname,
                                                cond_to_str(cond), width, rd, rn, minus, offset,
                                                bang);
            }
        } else {
            return Common::StringFromFormat("%s%s%s\tr%d, [r%d], #%s%u", opname, cond_to_str(cond),
                                            width, rd, rn, minus, offset);
        }
    }

    if (is_pre) {
        return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d]%s", opname, cond_to_str(cond),
                                        width, rd, rn, minus, rm, bang);
    } else {
        return Common::StringFromFormat("%s%s%s\tr%d, [r%d], %sr%d", opname, cond_to_str(cond),
                                        width, rd, rn, minus, rm);
    }
}

std::string ARM_Disasm::DisassembleMCR(Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 crn = (insn >> 16) & 0xf;
    u8 crd = (insn >> 12) & 0xf;
    u8 cpnum = (insn >> 8) & 0xf;
    u8 opcode2 = (insn >> 5) & 0x7;
    u8 crm = insn & 0xf;

    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\t%d, 0, r%d, cr%d, cr%d, {%d}", opname, cond_to_str(cond),
                                    cpnum, crd, crn, crm, opcode2);
}

std::string ARM_Disasm::DisassembleMLA(Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rd = (insn >> 16) & 0xf;
    u8 rn = (insn >> 12) & 0xf;
    u8 rs = (insn >> 8) & 0xf;
    u8 rm = insn & 0xf;
    u8 bit_s = (insn >> 20) & 1;

    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d, r%d", opname, cond_to_str(cond),
                                    bit_s ? "s" : "", rd, rm, rs, rn);
}

std::string ARM_Disasm::DisassembleUMLAL(Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rdhi = (insn >> 16) & 0xf;
    u8 rdlo = (insn >> 12) & 0xf;
    u8 rs = (insn >> 8) & 0xf;
    u8 rm = insn & 0xf;
    u8 bit_s = (insn >> 20) & 1;

    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d, r%d", opname, cond_to_str(cond),
                                    bit_s ? "s" : "", rdlo, rdhi, rm, rs);
}

std::string ARM_Disasm::DisassembleMUL(Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rd = (insn >> 16) & 0xf;
    u8 rs = (insn >> 8) & 0xf;
    u8 rm = insn & 0xf;
    u8 bit_s = (insn >> 20) & 1;

    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d", opname, cond_to_str(cond),
                                    bit_s ? "s" : "", rd, rm, rs);
}

std::string ARM_Disasm::DisassembleMRS(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u8 ps = (insn >> 22) & 1;

    return Common::StringFromFormat("mrs%s\tr%d, %s", cond_to_str(cond), rd, ps ? "spsr" : "cpsr");
}

std::string ARM_Disasm::DisassembleMSR(u32 insn) {
    char flags[8];
    int flag_index = 0;
    u8 cond = (insn >> 28) & 0xf;
    u8 is_immed = (insn >> 25) & 0x1;
    u8 pd = (insn >> 22) & 1;
    u8 mask = (insn >> 16) & 0xf;

    if (mask & 1)
        flags[flag_index++] = 'c';
    if (mask & 2)
        flags[flag_index++] = 'x';
    if (mask & 4)
        flags[flag_index++] = 's';
    if (mask & 8)
        flags[flag_index++] = 'f';
    flags[flag_index] = 0;

    if (is_immed) {
        u32 immed = insn & 0xff;
        u8 rotate = (insn >> 8) & 0xf;
        u8 rotate2 = rotate << 1;
        u32 rotated_val = (immed >> rotate2) | (immed << (32 - rotate2));
        return Common::StringFromFormat("msr%s\t%s_%s, #0x%x", cond_to_str(cond),
                                        pd ? "spsr" : "cpsr", flags, rotated_val);
    }

    u8 rm = insn & 0xf;

    return Common::StringFromFormat("msr%s\t%s_%s, r%d", cond_to_str(cond), pd ? "spsr" : "cpsr",
                                    flags, rm);
}

std::string ARM_Disasm::DisassembleNoOperands(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    return Common::StringFromFormat("%s%s", opcode_names[opcode], cond_to_str(cond));
}

std::string ARM_Disasm::DisassembleParallelAddSub(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rn = BITS(insn, 16, 19);
    u32 rd = BITS(insn, 12, 15);
    u32 rm = BITS(insn, 0, 3);

    return Common::StringFromFormat("%s%s\tr%u, r%u, r%u", opcode_names[opcode], cond_to_str(cond),
                                    rd, rn, rm);
}

std::string ARM_Disasm::DisassemblePKH(u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rn = BITS(insn, 16, 19);
    u32 rd = BITS(insn, 12, 15);
    u32 imm5 = BITS(insn, 7, 11);
    u32 tb = BIT(insn, 6);
    u32 rm = BITS(insn, 0, 3);

    std::string suffix = tb ? "tb" : "bt";
    std::string shift = "";

    if (tb && imm5 == 0)
        imm5 = 32;

    if (imm5 > 0) {
        shift = tb ? ", ASR" : ", LSL";
        shift += " #" + std::to_string(imm5);
    }

    return Common::StringFromFormat("pkh%s%s\tr%u, r%u, r%u%s", suffix.c_str(), cond_to_str(cond),
                                    rd, rn, rm, shift.c_str());
}

std::string ARM_Disasm::DisassemblePLD(u32 insn) {
    u8 is_reg = (insn >> 25) & 0x1;
    u8 is_up = (insn >> 23) & 0x1;
    u8 rn = (insn >> 16) & 0xf;

    const char* minus = "";
    if (is_up == 0)
        minus = "-";

    if (is_reg) {
        u8 rm = insn & 0xf;
        return Common::StringFromFormat("pld\t[r%d, %sr%d]", rn, minus, rm);
    }

    u16 offset = insn & 0xfff;
    if (offset == 0) {
        return Common::StringFromFormat("pld\t[r%d]", rn);
    } else {
        return Common::StringFromFormat("pld\t[r%d, #%s%u]", rn, minus, offset);
    }
}

std::string ARM_Disasm::DisassembleREV(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rd = BITS(insn, 12, 15);
    u32 rm = BITS(insn, 0, 3);

    return Common::StringFromFormat("%s%s\tr%u, r%u", opcode_names[opcode], cond_to_str(cond), rd,
                                    rm);
}

std::string ARM_Disasm::DisassembleREX(Opcode opcode, u32 insn) {
    u32 rn = BITS(insn, 16, 19);
    u32 rd = BITS(insn, 12, 15);
    u32 rt = BITS(insn, 0, 3);
    u32 cond = BITS(insn, 28, 31);

    switch (opcode) {
    case OP_STREX:
    case OP_STREXB:
    case OP_STREXH:
        return Common::StringFromFormat("%s%s\tr%d, r%d, [r%d]", opcode_names[opcode],
                                        cond_to_str(cond), rd, rt, rn);
    case OP_STREXD:
        return Common::StringFromFormat("%s%s\tr%d, r%d, r%d, [r%d]", opcode_names[opcode],
                                        cond_to_str(cond), rd, rt, rt + 1, rn);

    // for LDREX instructions, rd corresponds to Rt from reference manual
    case OP_LDREX:
    case OP_LDREXB:
    case OP_LDREXH:
        return Common::StringFromFormat("%s%s\tr%d, [r%d]", opcode_names[opcode], cond_to_str(cond),
                                        rd, rn);
    case OP_LDREXD:
        return Common::StringFromFormat("%s%s\tr%d, r%d, [r%d]", opcode_names[opcode],
                                        cond_to_str(cond), rd, rd + 1, rn);
    default:
        return opcode_names[OP_UNDEFINED];
    }
}

std::string ARM_Disasm::DisassembleSAT(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 sat_imm = BITS(insn, 16, 20);
    u32 rd = BITS(insn, 12, 15);
    u32 imm5 = BITS(insn, 7, 11);
    u32 sh = BIT(insn, 6);
    u32 rn = BITS(insn, 0, 3);

    std::string shift_part = "";
    bool opcode_has_shift = (opcode == OP_SSAT) || (opcode == OP_USAT);
    if (opcode_has_shift && !(sh == 0 && imm5 == 0)) {
        if (sh == 0)
            shift_part += ", LSL #";
        else
            shift_part += ", ASR #";

        if (imm5 == 0)
            imm5 = 32;
        shift_part += std::to_string(imm5);
    }

    if (opcode == OP_SSAT || opcode == OP_SSAT16)
        sat_imm++;

    return Common::StringFromFormat("%s%s\tr%u, #%u, r%u%s", opcode_names[opcode],
                                    cond_to_str(cond), rd, sat_imm, rn, shift_part.c_str());
}

std::string ARM_Disasm::DisassembleSEL(u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rn = BITS(insn, 16, 19);
    u32 rd = BITS(insn, 12, 15);
    u32 rm = BITS(insn, 0, 3);

    return Common::StringFromFormat("%s%s\tr%u, r%u, r%u", opcode_names[OP_SEL], cond_to_str(cond),
                                    rd, rn, rm);
}

std::string ARM_Disasm::DisassembleSWI(u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u32 sysnum = insn & 0x00ffffff;

    return Common::StringFromFormat("swi%s 0x%x", cond_to_str(cond), sysnum);
}

std::string ARM_Disasm::DisassembleSWP(Opcode opcode, u32 insn) {
    u8 cond = (insn >> 28) & 0xf;
    u8 rn = (insn >> 16) & 0xf;
    u8 rd = (insn >> 12) & 0xf;
    u8 rm = insn & 0xf;

    const char* opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\tr%d, r%d, [r%d]", opname, cond_to_str(cond), rd, rm, rn);
}

std::string ARM_Disasm::DisassembleXT(Opcode opcode, u32 insn) {
    u32 cond = BITS(insn, 28, 31);
    u32 rn = BITS(insn, 16, 19);
    u32 rd = BITS(insn, 12, 15);
    u32 rotate = BITS(insn, 10, 11);
    u32 rm = BITS(insn, 0, 3);

    std::string rn_part = "";
    static std::unordered_set<Opcode, std::hash<int>> extend_with_add = {
        OP_SXTAB, OP_SXTAB16, OP_SXTAH, OP_UXTAB, OP_UXTAB16, OP_UXTAH};
    if (extend_with_add.find(opcode) != extend_with_add.end())
        rn_part = ", r" + std::to_string(rn);

    std::string rotate_part = "";
    if (rotate != 0)
        rotate_part = ", ROR #" + std::to_string(rotate << 3);

    return Common::StringFromFormat("%s%s\tr%u%s, r%u%s", opcode_names[opcode], cond_to_str(cond),
                                    rd, rn_part.c_str(), rm, rotate_part.c_str());
}

Opcode ARM_Disasm::Decode(u32 insn) {
    u32 bits27_26 = (insn >> 26) & 0x3;
    switch (bits27_26) {
    case 0x0:
        return Decode00(insn);
    case 0x1:
        return Decode01(insn);
    case 0x2:
        return Decode10(insn);
    case 0x3:
        return Decode11(insn);
    }
    return OP_INVALID;
}

Opcode ARM_Disasm::Decode00(u32 insn) {
    u8 bit25 = (insn >> 25) & 0x1;
    u8 bit4 = (insn >> 4) & 0x1;
    if (bit25 == 0 && bit4 == 1) {
        if ((insn & 0x0ffffff0) == 0x012fff10) {
            // Bx instruction
            return OP_BX;
        }
        if ((insn & 0x0ff000f0) == 0x01600010) {
            // Clz instruction
            return OP_CLZ;
        }
        if ((insn & 0xfff000f0) == 0xe1200070) {
            // Bkpt instruction
            return OP_BKPT;
        }
        u32 bits7_4 = (insn >> 4) & 0xf;
        if (bits7_4 == 0x9) {
            u32 bit24 = BIT(insn, 24);
            if (bit24) {
                return DecodeSyncPrimitive(insn);
            }
            // One of the multiply instructions
            return DecodeMUL(insn);
        }

        u8 bit7 = (insn >> 7) & 0x1;
        if (bit7 == 1) {
            // One of the load/store halfword/byte instructions
            return DecodeLDRH(insn);
        }
    }

    u32 op1 = BITS(insn, 20, 24);
    if (bit25 && (op1 == 0x12 || op1 == 0x16)) {
        // One of the MSR (immediate) and hints instructions
        return DecodeMSRImmAndHints(insn);
    }

    // One of the data processing instructions
    return DecodeALU(insn);
}

Opcode ARM_Disasm::Decode01(u32 insn) {
    u8 is_reg = (insn >> 25) & 0x1;
    u8 bit4 = (insn >> 4) & 0x1;
    if (is_reg == 1 && bit4 == 1)
        return DecodeMedia(insn);
    u8 is_load = (insn >> 20) & 0x1;
    u8 is_byte = (insn >> 22) & 0x1;
    if ((insn & 0xfd70f000) == 0xf550f000) {
        // Pre-load
        return OP_PLD;
    }
    if (insn == 0xf57ff01f) {
        // Clear-Exclusive
        return OP_CLREX;
    }
    if (is_load) {
        if (is_byte) {
            // Load byte
            return OP_LDRB;
        }
        // Load word
        return OP_LDR;
    }
    if (is_byte) {
        // Store byte
        return OP_STRB;
    }
    // Store word
    return OP_STR;
}

Opcode ARM_Disasm::Decode10(u32 insn) {
    u8 bit25 = (insn >> 25) & 0x1;
    if (bit25 == 0) {
        // LDM/STM
        u8 is_load = (insn >> 20) & 0x1;
        if (is_load)
            return OP_LDM;
        return OP_STM;
    }

    // Branch with link
    if ((insn >> 24) & 1)
        return OP_BL;

    return OP_B;
}

Opcode ARM_Disasm::Decode11(u32 insn) {
    u8 bit25 = (insn >> 25) & 0x1;
    if (bit25 == 0) {
        // LDC, SDC
        u8 is_load = (insn >> 20) & 0x1;
        if (is_load) {
            // LDC
            return OP_LDC;
        }
        // STC
        return OP_STC;
    }

    u8 bit24 = (insn >> 24) & 0x1;
    if (bit24 == 0x1) {
        // SWI
        return OP_SWI;
    }

    u8 bit4 = (insn >> 4) & 0x1;
    u8 cpnum = (insn >> 8) & 0xf;

    if (cpnum == 15) {
        // Special case for coprocessor 15
        u8 opcode = (insn >> 21) & 0x7;
        if (bit4 == 0 || opcode != 0) {
            // This is an unexpected bit pattern.  Create an undefined
            // instruction in case this is ever executed.
            return OP_UNDEFINED;
        }

        // MRC, MCR
        u8 is_mrc = (insn >> 20) & 0x1;
        if (is_mrc)
            return OP_MRC;
        return OP_MCR;
    }

    if (bit4 == 0) {
        // CDP
        return OP_CDP;
    }
    // MRC, MCR
    u8 is_mrc = (insn >> 20) & 0x1;
    if (is_mrc)
        return OP_MRC;
    return OP_MCR;
}

Opcode ARM_Disasm::DecodeSyncPrimitive(u32 insn) {
    u32 op = BITS(insn, 20, 23);
    u32 bit22 = BIT(insn, 22);
    switch (op) {
    case 0x0:
        if (bit22)
            return OP_SWPB;
        return OP_SWP;
    case 0x8:
        return OP_STREX;
    case 0x9:
        return OP_LDREX;
    case 0xA:
        return OP_STREXD;
    case 0xB:
        return OP_LDREXD;
    case 0xC:
        return OP_STREXB;
    case 0xD:
        return OP_LDREXB;
    case 0xE:
        return OP_STREXH;
    case 0xF:
        return OP_LDREXH;
    default:
        return OP_UNDEFINED;
    }
}

Opcode ARM_Disasm::DecodeParallelAddSub(u32 insn) {
    u32 op1 = BITS(insn, 20, 21);
    u32 op2 = BITS(insn, 5, 7);
    u32 is_unsigned = BIT(insn, 22);

    if (op1 == 0x0 || op2 == 0x5 || op2 == 0x6)
        return OP_UNDEFINED;

    // change op1 range from [1, 3] to range [0, 2]
    op1--;

    // change op2 range from [0, 4] U {7} to range [0, 5]
    if (op2 == 0x7)
        op2 = 0x5;

    static std::vector<Opcode> opcodes = {
        // op1 = 0
        OP_SADD16, OP_UADD16, OP_SASX, OP_UASX, OP_SSAX, OP_USAX, OP_SSUB16, OP_USUB16, OP_SADD8,
        OP_UADD8, OP_SSUB8, OP_USUB8,
        // op1 = 1
        OP_QADD16, OP_UQADD16, OP_QASX, OP_UQASX, OP_QSAX, OP_UQSAX, OP_QSUB16, OP_UQSUB16,
        OP_QADD8, OP_UQADD8, OP_QSUB8, OP_UQSUB8,
        // op1 = 2
        OP_SHADD16, OP_UHADD16, OP_SHASX, OP_UHASX, OP_SHSAX, OP_UHSAX, OP_SHSUB16, OP_UHSUB16,
        OP_SHADD8, OP_UHADD8, OP_SHSUB8, OP_UHSUB8};

    u32 opcode_index = op1 * 12 + op2 * 2 + is_unsigned;
    return opcodes[opcode_index];
}

Opcode ARM_Disasm::DecodePackingSaturationReversal(u32 insn) {
    u32 op1 = BITS(insn, 20, 22);
    u32 a = BITS(insn, 16, 19);
    u32 op2 = BITS(insn, 5, 7);

    switch (op1) {
    case 0x0:
        if (BIT(op2, 0) == 0)
            return OP_PKH;
        if (op2 == 0x3 && a != 0xf)
            return OP_SXTAB16;
        if (op2 == 0x3 && a == 0xf)
            return OP_SXTB16;
        if (op2 == 0x5)
            return OP_SEL;
        break;
    case 0x2:
        if (BIT(op2, 0) == 0)
            return OP_SSAT;
        if (op2 == 0x1)
            return OP_SSAT16;
        if (op2 == 0x3 && a != 0xf)
            return OP_SXTAB;
        if (op2 == 0x3 && a == 0xf)
            return OP_SXTB;
        break;
    case 0x3:
        if (op2 == 0x1)
            return OP_REV;
        if (BIT(op2, 0) == 0)
            return OP_SSAT;
        if (op2 == 0x3 && a != 0xf)
            return OP_SXTAH;
        if (op2 == 0x3 && a == 0xf)
            return OP_SXTH;
        if (op2 == 0x5)
            return OP_REV16;
        break;
    case 0x4:
        if (op2 == 0x3 && a != 0xf)
            return OP_UXTAB16;
        if (op2 == 0x3 && a == 0xf)
            return OP_UXTB16;
        break;
    case 0x6:
        if (BIT(op2, 0) == 0)
            return OP_USAT;
        if (op2 == 0x1)
            return OP_USAT16;
        if (op2 == 0x3 && a != 0xf)
            return OP_UXTAB;
        if (op2 == 0x3 && a == 0xf)
            return OP_UXTB;
        break;
    case 0x7:
        if (BIT(op2, 0) == 0)
            return OP_USAT;
        if (op2 == 0x3 && a != 0xf)
            return OP_UXTAH;
        if (op2 == 0x3 && a == 0xf)
            return OP_UXTH;
        if (op2 == 0x5)
            return OP_REVSH;
        break;
    default:
        break;
    }

    return OP_UNDEFINED;
}

Opcode ARM_Disasm::DecodeMUL(u32 insn) {
    u8 bit24 = (insn >> 24) & 0x1;
    if (bit24 != 0) {
        // This is an unexpected bit pattern.  Create an undefined
        // instruction in case this is ever executed.
        return OP_UNDEFINED;
    }
    u8 bit23 = (insn >> 23) & 0x1;
    u8 bit22_U = (insn >> 22) & 0x1;
    u8 bit21_A = (insn >> 21) & 0x1;
    if (bit23 == 0) {
        // 32-bit multiply
        if (bit22_U != 0) {
            // This is an unexpected bit pattern.  Create an undefined
            // instruction in case this is ever executed.
            return OP_UNDEFINED;
        }
        if (bit21_A == 0)
            return OP_MUL;
        return OP_MLA;
    }
    // 64-bit multiply
    if (bit22_U == 0) {
        // Unsigned multiply long
        if (bit21_A == 0)
            return OP_UMULL;
        return OP_UMLAL;
    }
    // Signed multiply long
    if (bit21_A == 0)
        return OP_SMULL;
    return OP_SMLAL;
}

Opcode ARM_Disasm::DecodeMSRImmAndHints(u32 insn) {
    u32 op = BIT(insn, 22);
    u32 op1 = BITS(insn, 16, 19);
    u32 op2 = BITS(insn, 0, 7);

    if (op == 0 && op1 == 0) {
        switch (op2) {
        case 0x0:
            return OP_NOP;
        case 0x1:
            return OP_YIELD;
        case 0x2:
            return OP_WFE;
        case 0x3:
            return OP_WFI;
        case 0x4:
            return OP_SEV;
        default:
            return OP_UNDEFINED;
        }
    }

    return OP_MSR;
}

Opcode ARM_Disasm::DecodeMediaMulDiv(u32 insn) {
    u32 op1 = BITS(insn, 20, 22);
    u32 op2_h = BITS(insn, 6, 7);
    u32 a = BITS(insn, 12, 15);

    switch (op1) {
    case 0x0:
        if (op2_h == 0x0) {
            if (a != 0xf)
                return OP_SMLAD;
            else
                return OP_SMUAD;
        } else if (op2_h == 0x1) {
            if (a != 0xf)
                return OP_SMLSD;
            else
                return OP_SMUSD;
        }
        break;
    case 0x4:
        if (op2_h == 0x0)
            return OP_SMLALD;
        else if (op2_h == 0x1)
            return OP_SMLSLD;
        break;
    case 0x5:
        if (op2_h == 0x0) {
            if (a != 0xf)
                return OP_SMMLA;
            else
                return OP_SMMUL;
        } else if (op2_h == 0x3) {
            return OP_SMMLS;
        }
        break;
    default:
        break;
    }

    return OP_UNDEFINED;
}

Opcode ARM_Disasm::DecodeMedia(u32 insn) {
    u32 op1 = BITS(insn, 20, 24);
    u32 rd = BITS(insn, 12, 15);
    u32 op2 = BITS(insn, 5, 7);

    switch (BITS(op1, 3, 4)) {
    case 0x0:
        // unsigned and signed parallel addition and subtraction
        return DecodeParallelAddSub(insn);
    case 0x1:
        // Packing, unpacking, saturation, and reversal
        return DecodePackingSaturationReversal(insn);
    case 0x2:
        // Signed multiply, signed and unsigned divide
        return DecodeMediaMulDiv(insn);
    case 0x3:
        if (op2 == 0 && rd == 0xf)
            return OP_USAD8;
        if (op2 == 0 && rd != 0xf)
            return OP_USADA8;
        break;
    default:
        break;
    }

    return OP_UNDEFINED;
}

Opcode ARM_Disasm::DecodeLDRH(u32 insn) {
    u8 is_load = (insn >> 20) & 0x1;
    u8 bits_65 = (insn >> 5) & 0x3;
    if (is_load) {
        if (bits_65 == 0x1) {
            // Load unsigned halfword
            return OP_LDRH;
        } else if (bits_65 == 0x2) {
            // Load signed byte
            return OP_LDRSB;
        }
        // Signed halfword
        if (bits_65 != 0x3) {
            // This is an unexpected bit pattern.  Create an undefined
            // instruction in case this is ever executed.
            return OP_UNDEFINED;
        }
        // Load signed halfword
        return OP_LDRSH;
    }
    // Store halfword
    if (bits_65 != 0x1) {
        // This is an unexpected bit pattern.  Create an undefined
        // instruction in case this is ever executed.
        return OP_UNDEFINED;
    }
    // Store halfword
    return OP_STRH;
}

Opcode ARM_Disasm::DecodeALU(u32 insn) {
    u8 is_immed = (insn >> 25) & 0x1;
    u8 opcode = (insn >> 21) & 0xf;
    u8 bit_s = (insn >> 20) & 1;
    u8 shift_is_reg = (insn >> 4) & 1;
    u8 bit7 = (insn >> 7) & 1;
    if (!is_immed && shift_is_reg && (bit7 != 0)) {
        // This is an unexpected bit pattern.  Create an undefined
        // instruction in case this is ever executed.
        return OP_UNDEFINED;
    }
    switch (opcode) {
    case 0x0:
        return OP_AND;
    case 0x1:
        return OP_EOR;
    case 0x2:
        return OP_SUB;
    case 0x3:
        return OP_RSB;
    case 0x4:
        return OP_ADD;
    case 0x5:
        return OP_ADC;
    case 0x6:
        return OP_SBC;
    case 0x7:
        return OP_RSC;
    case 0x8:
        if (bit_s)
            return OP_TST;
        return OP_MRS;
    case 0x9:
        if (bit_s)
            return OP_TEQ;
        return OP_MSR;
    case 0xa:
        if (bit_s)
            return OP_CMP;
        return OP_MRS;
    case 0xb:
        if (bit_s)
            return OP_CMN;
        return OP_MSR;
    case 0xc:
        return OP_ORR;
    case 0xd:
        return OP_MOV;
    case 0xe:
        return OP_BIC;
    case 0xf:
        return OP_MVN;
    }
    // Unreachable
    return OP_INVALID;
}
