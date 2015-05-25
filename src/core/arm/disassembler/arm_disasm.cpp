// Copyright 2006 The Android Open Source Project

#include <string>

#include "common/string_util.h"
#include "core/arm/disassembler/arm_disasm.h"

static const char *cond_names[] = {
    "eq",
    "ne",
    "cs",
    "cc",
    "mi",
    "pl",
    "vs",
    "vc",
    "hi",
    "ls",
    "ge",
    "lt",
    "gt",
    "le",
    "",
    "RESERVED"
};

static const char *opcode_names[] = {
    "invalid",
    "undefined",
    "adc",
    "add",
    "and",
    "b",
    "bl",
    "bic",
    "bkpt",
    "blx",
    "bx",
    "cdp",
    "clz",
    "cmn",
    "cmp",
    "eor",
    "ldc",
    "ldm",
    "ldr",
    "ldrb",
    "ldrbt",
    "ldrh",
    "ldrsb",
    "ldrsh",
    "ldrt",
    "mcr",
    "mla",
    "mov",
    "mrc",
    "mrs",
    "msr",
    "mul",
    "mvn",
    "orr",
    "pld",
    "rsb",
    "rsc",
    "sbc",
    "smlal",
    "smull",
    "stc",
    "stm",
    "str",
    "strb",
    "strbt",
    "strh",
    "strt",
    "sub",
    "swi",
    "swp",
    "swpb",
    "teq",
    "tst",
    "umlal",
    "umull",

    "undefined",
    "adc",
    "add",
    "and",
    "asr",
    "b",
    "bic",
    "bkpt",
    "bl",
    "blx",
    "bx",
    "cmn",
    "cmp",
    "eor",
    "ldmia",
    "ldr",
    "ldrb",
    "ldrh",
    "ldrsb",
    "ldrsh",
    "lsl",
    "lsr",
    "mov",
    "mul",
    "mvn",
    "neg",
    "orr",
    "pop",
    "push",
    "ror",
    "sbc",
    "stmia",
    "str",
    "strb",
    "strh",
    "sub",
    "swi",
    "tst",

    NULL
};

// Indexed by the shift type (bits 6-5)
static const char *shift_names[] = {
    "LSL",
    "LSR",
    "ASR",
    "ROR"
};

static const char* cond_to_str(uint32_t cond) {
    return cond_names[cond];
}

std::string ARM_Disasm::Disassemble(uint32_t addr, uint32_t insn)
{
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
        case OP_PLD:
            return DisassemblePLD(insn);
        case OP_STC:
            return "stc";
        case OP_SWI:
            return DisassembleSWI(insn);
        case OP_SWP:
        case OP_SWPB:
            return DisassembleSWP(opcode, insn);
        case OP_UMLAL:
        case OP_UMULL:
        case OP_SMLAL:
        case OP_SMULL:
            return DisassembleUMLAL(opcode, insn);
        default:
            return "Error";
    }
    return NULL;
}

std::string ARM_Disasm::DisassembleALU(Opcode opcode, uint32_t insn)
{
    static const uint8_t kNoOperand1 = 1;
    static const uint8_t kNoDest = 2;
    static const uint8_t kNoSbit = 4;

    std::string rn_str;
    std::string rd_str;

    uint8_t flags = 0;
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t is_immed = (insn >> 25) & 0x1;
    uint8_t bit_s = (insn >> 20) & 1;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t immed = insn & 0xff;

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

    const char *sbit_str = "";
    if (bit_s && !(flags & kNoSbit))
        sbit_str = "s";

    if (is_immed) {
        return Common::StringFromFormat("%s%s%s\t%s%s#%u  ; 0x%x",
                opname, cond_to_str(cond), sbit_str, rd_str.c_str(), rn_str.c_str(), immed, immed);
    }

    uint8_t shift_is_reg = (insn >> 4) & 1;
    uint8_t rotate = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t shift_type = (insn >> 5) & 0x3;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t shift_amount = (insn >> 7) & 0x1f;
    uint32_t rotated_val = immed;
    uint8_t rotate2 = rotate << 1;
    rotated_val = (rotated_val >> rotate2) | (rotated_val << (32 - rotate2));

    if (!shift_is_reg && shift_type == 0 && shift_amount == 0) {
        return Common::StringFromFormat("%s%s%s\t%s%sr%d",
                opname, cond_to_str(cond), sbit_str, rd_str.c_str(), rn_str.c_str(), rm);
    }

    const char *shift_name = shift_names[shift_type];
    if (shift_is_reg) {
        return Common::StringFromFormat("%s%s%s\t%s%sr%d, %s r%d",
                opname, cond_to_str(cond), sbit_str, rd_str.c_str(), rn_str.c_str(), rm,
                shift_name, rs);
    }
    if (shift_amount == 0) {
        if (shift_type == 3) {
            return Common::StringFromFormat("%s%s%s\t%s%sr%d, RRX",
                    opname, cond_to_str(cond), sbit_str, rd_str.c_str(), rn_str.c_str(), rm);
        }
        shift_amount = 32;
    }
    return Common::StringFromFormat("%s%s%s\t%s%sr%d, %s #%u",
            opname, cond_to_str(cond), sbit_str, rd_str.c_str(), rn_str.c_str(), rm,
            shift_name, shift_amount);
}

std::string ARM_Disasm::DisassembleBranch(uint32_t addr, Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint32_t offset = insn & 0xffffff;
    // Sign-extend the 24-bit offset
    if ((offset >> 23) & 1)
        offset |= 0xff000000;

    // Pre-compute the left-shift and the prefetch offset
    offset <<= 2;
    offset += 8;
    addr += offset;
    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\t0x%x", opname, cond_to_str(cond), addr);
}

std::string ARM_Disasm::DisassembleBX(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rn = insn & 0xf;
    return Common::StringFromFormat("bx%s\tr%d", cond_to_str(cond), rn);
}

std::string ARM_Disasm::DisassembleBKPT(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint32_t immed = (((insn >> 8) & 0xfff) << 4) | (insn & 0xf);
    return Common::StringFromFormat("bkpt%s\t#%d", cond_to_str(cond), immed);
}

std::string ARM_Disasm::DisassembleCLZ(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t rm = insn & 0xf;
    return Common::StringFromFormat("clz%s\tr%d, r%d", cond_to_str(cond), rd, rm);
}

std::string ARM_Disasm::DisassembleMemblock(Opcode opcode, uint32_t insn)
{
    std::string tmp_list;

    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t write_back = (insn >> 21) & 0x1;
    uint8_t bit_s = (insn >> 22) & 0x1;
    uint8_t is_up = (insn >> 23) & 0x1;
    uint8_t is_pre = (insn >> 24) & 0x1;
    uint8_t rn = (insn >> 16) & 0xf;
    uint16_t reg_list = insn & 0xffff;

    const char *opname = opcode_names[opcode];

    const char *bang = "";
    if (write_back)
        bang = "!";

    const char *carret = "";
    if (bit_s)
        carret = "^";

    const char *comma = "";
    tmp_list[0] = 0;
    for (int ii = 0; ii < 16; ++ii) {
        if (reg_list & (1 << ii)) {
            tmp_list += Common::StringFromFormat("%sr%d", comma, ii);
            comma = ",";
        }
    }

    const char *addr_mode = "";
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

    return Common::StringFromFormat("%s%s%s\tr%d%s, {%s}%s",
            opname, cond_to_str(cond), addr_mode, rn, bang, tmp_list.c_str(), carret);
}

std::string ARM_Disasm::DisassembleMem(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t is_reg = (insn >> 25) & 0x1;
    uint8_t is_load = (insn >> 20) & 0x1;
    uint8_t write_back = (insn >> 21) & 0x1;
    uint8_t is_byte = (insn >> 22) & 0x1;
    uint8_t is_up = (insn >> 23) & 0x1;
    uint8_t is_pre = (insn >> 24) & 0x1;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint16_t offset = insn & 0xfff;

    const char *opname = "ldr";
    if (!is_load)
        opname = "str";

    const char *bang = "";
    if (write_back)
        bang = "!";

    const char *minus = "";
    if (is_up == 0)
        minus = "-";

    const char *byte = "";
    if (is_byte)
        byte = "b";

    if (is_reg == 0) {
        if (is_pre) {
            if (offset == 0) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d]",
                        opname, cond_to_str(cond), byte, rd, rn);
            } else {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, #%s%u]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, offset, bang);
            }
        } else {
            const char *transfer = "";
            if (write_back)
                transfer = "t";

            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], #%s%u",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, offset);
        }
    }

    uint8_t rm = insn & 0xf;
    uint8_t shift_type = (insn >> 5) & 0x3;
    uint8_t shift_amount = (insn >> 7) & 0x1f;

    const char *shift_name = shift_names[shift_type];

    if (is_pre) {
        if (shift_amount == 0) {
            if (shift_type == 0) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, rm, bang);
            }
            if (shift_type == 3) {
                return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d, RRX]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, rm, bang);
            }
            shift_amount = 32;
        }
        return Common::StringFromFormat("%s%s%s\tr%d, [r%d, %sr%d, %s #%u]%s",
                opname, cond_to_str(cond), byte, rd, rn, minus, rm,
                shift_name, shift_amount, bang);
    }

    const char *transfer = "";
    if (write_back)
        transfer = "t";

    if (shift_amount == 0) {
        if (shift_type == 0) {
            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
        }
        if (shift_type == 3) {
            return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d, RRX",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
        }
        shift_amount = 32;
    }

    return Common::StringFromFormat("%s%s%s%s\tr%d, [r%d], %sr%d, %s #%u",
            opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm,
            shift_name, shift_amount);
}

std::string ARM_Disasm::DisassembleMemHalf(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t is_load = (insn >> 20) & 0x1;
    uint8_t write_back = (insn >> 21) & 0x1;
    uint8_t is_immed = (insn >> 22) & 0x1;
    uint8_t is_up = (insn >> 23) & 0x1;
    uint8_t is_pre = (insn >> 24) & 0x1;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t bits_65 = (insn >> 5) & 0x3;
    uint8_t rm = insn & 0xf;
    uint8_t offset = (((insn >> 8) & 0xf) << 4) | (insn & 0xf);

    const char *opname = "ldr";
    if (is_load == 0)
        opname = "str";

    const char *width = "";
    if (bits_65 == 1)
        width = "h";
    else if (bits_65 == 2)
        width = "sb";
    else
        width = "sh";

    const char *bang = "";
    if (write_back)
        bang = "!";
    const char *minus = "";
    if (is_up == 0)
        minus = "-";

    if (is_immed) {
        if (is_pre) {
            if (offset == 0) {
                return Common::StringFromFormat("%s%sh\tr%d, [r%d]", opname, cond_to_str(cond), rd, rn);
            } else {
                return Common::StringFromFormat("%s%sh\tr%d, [r%d, #%s%u]%s",
                        opname, cond_to_str(cond), rd, rn, minus, offset, bang);
            }
        } else {
            return Common::StringFromFormat("%s%sh\tr%d, [r%d], #%s%u",
                    opname, cond_to_str(cond), rd, rn, minus, offset);
        }
    }

    if (is_pre) {
        return Common::StringFromFormat("%s%sh\tr%d, [r%d, %sr%d]%s",
                opname, cond_to_str(cond), rd, rn, minus, rm, bang);
    } else {
        return Common::StringFromFormat("%s%sh\tr%d, [r%d], %sr%d",
                opname, cond_to_str(cond), rd, rn, minus, rm);
    }
}

std::string ARM_Disasm::DisassembleMCR(Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t crn = (insn >> 16) & 0xf;
    uint8_t crd = (insn >> 12) & 0xf;
    uint8_t cpnum = (insn >> 8) & 0xf;
    uint8_t opcode2 = (insn >> 5) & 0x7;
    uint8_t crm = insn & 0xf;

    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\t%d, 0, r%d, cr%d, cr%d, {%d}",
            opname, cond_to_str(cond), cpnum, crd, crn, crm, opcode2);
}

std::string ARM_Disasm::DisassembleMLA(Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 16) & 0xf;
    uint8_t rn = (insn >> 12) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rd, rm, rs, rn);
}

std::string ARM_Disasm::DisassembleUMLAL(Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rdhi = (insn >> 16) & 0xf;
    uint8_t rdlo = (insn >> 12) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rdlo, rdhi, rm, rs);
}

std::string ARM_Disasm::DisassembleMUL(Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 16) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s%s\tr%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rd, rm, rs);
}

std::string ARM_Disasm::DisassembleMRS(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t ps = (insn >> 22) & 1;

    return Common::StringFromFormat("mrs%s\tr%d, %s", cond_to_str(cond), rd, ps ? "spsr" : "cpsr");
}

std::string ARM_Disasm::DisassembleMSR(uint32_t insn)
{
    char flags[8];
    int flag_index = 0;
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t is_immed = (insn >> 25) & 0x1;
    uint8_t pd = (insn >> 22) & 1;
    uint8_t mask = (insn >> 16) & 0xf;

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
        uint32_t immed = insn & 0xff;
        uint8_t rotate = (insn >> 8) & 0xf;
        uint8_t rotate2 = rotate << 1;
        uint32_t rotated_val = (immed >> rotate2) | (immed << (32 - rotate2));
        return Common::StringFromFormat("msr%s\t%s_%s, #0x%x",
                cond_to_str(cond), pd ? "spsr" : "cpsr", flags, rotated_val);
    }

    uint8_t rm = insn & 0xf;

    return Common::StringFromFormat("msr%s\t%s_%s, r%d",
            cond_to_str(cond), pd ? "spsr" : "cpsr", flags, rm);
}

std::string ARM_Disasm::DisassemblePLD(uint32_t insn)
{
    uint8_t is_reg = (insn >> 25) & 0x1;
    uint8_t is_up = (insn >> 23) & 0x1;
    uint8_t rn = (insn >> 16) & 0xf;

    const char *minus = "";
    if (is_up == 0)
        minus = "-";

    if (is_reg) {
        uint8_t rm = insn & 0xf;
        return Common::StringFromFormat("pld\t[r%d, %sr%d]", rn, minus, rm);
    }

    uint16_t offset = insn & 0xfff;
    if (offset == 0) {
        return Common::StringFromFormat("pld\t[r%d]", rn);
    } else {
        return Common::StringFromFormat("pld\t[r%d, #%s%u]", rn, minus, offset);
    }
}

std::string ARM_Disasm::DisassembleSWI(uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint32_t sysnum = insn & 0x00ffffff;

    return Common::StringFromFormat("swi%s 0x%x", cond_to_str(cond), sysnum);
}

std::string ARM_Disasm::DisassembleSWP(Opcode opcode, uint32_t insn)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t rm = insn & 0xf;

    const char *opname = opcode_names[opcode];
    return Common::StringFromFormat("%s%s\tr%d, r%d, [r%d]", opname, cond_to_str(cond), rd, rm, rn);
}

Opcode ARM_Disasm::Decode(uint32_t insn) {
    uint32_t bits27_26 = (insn >> 26) & 0x3;
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

Opcode ARM_Disasm::Decode00(uint32_t insn) {
    uint8_t bit25 = (insn >> 25) & 0x1;
    uint8_t bit4 = (insn >> 4) & 0x1;
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
        uint32_t bits7_4 = (insn >> 4) & 0xf;
        if (bits7_4 == 0x9) {
            if ((insn & 0x0ff00ff0) == 0x01000090) {
                // Swp instruction
                uint8_t bit22 = (insn >> 22) & 0x1;
                if (bit22)
                    return OP_SWPB;
                return OP_SWP;
            }
            // One of the multiply instructions
            return DecodeMUL(insn);
        }

        uint8_t bit7 = (insn >> 7) & 0x1;
        if (bit7 == 1) {
            // One of the load/store halfword/byte instructions
            return DecodeLDRH(insn);
        }
    }

    // One of the data processing instructions
    return DecodeALU(insn);
}

Opcode ARM_Disasm::Decode01(uint32_t insn) {
    uint8_t is_reg = (insn >> 25) & 0x1;
    uint8_t bit4 = (insn >> 4) & 0x1;
    if (is_reg == 1 && bit4 == 1)
        return OP_UNDEFINED;
    uint8_t is_load = (insn >> 20) & 0x1;
    uint8_t is_byte = (insn >> 22) & 0x1;
    if ((insn & 0xfd70f000) == 0xf550f000) {
        // Pre-load
        return OP_PLD;
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

Opcode ARM_Disasm::Decode10(uint32_t insn) {
    uint8_t bit25 = (insn >> 25) & 0x1;
    if (bit25 == 0) {
        // LDM/STM
        uint8_t is_load = (insn >> 20) & 0x1;
        if (is_load)
            return OP_LDM;
        return OP_STM;
    }
    // Branch or Branch with link
    uint8_t is_link = (insn >> 24) & 1;
    uint32_t offset = insn & 0xffffff;

    // Sign-extend the 24-bit offset
    if ((offset >> 23) & 1)
        offset |= 0xff000000;

    // Pre-compute the left-shift and the prefetch offset
    offset <<= 2;
    offset += 8;
    if (is_link == 0)
        return OP_B;
    return OP_BL;
}

Opcode ARM_Disasm::Decode11(uint32_t insn) {
    uint8_t bit25 = (insn >> 25) & 0x1;
    if (bit25 == 0) {
        // LDC, SDC
        uint8_t is_load = (insn >> 20) & 0x1;
        if (is_load) {
            // LDC
            return OP_LDC;
        }
        // STC
        return OP_STC;
    }

    uint8_t bit24 = (insn >> 24) & 0x1;
    if (bit24 == 0x1) {
        // SWI
        return OP_SWI;
    }

    uint8_t bit4 = (insn >> 4) & 0x1;
    uint8_t cpnum = (insn >> 8) & 0xf;

    if (cpnum == 15) {
        // Special case for coprocessor 15
        uint8_t opcode = (insn >> 21) & 0x7;
        if (bit4 == 0 || opcode != 0) {
            // This is an unexpected bit pattern.  Create an undefined
            // instruction in case this is ever executed.
            return OP_UNDEFINED;
        }

        // MRC, MCR
        uint8_t is_mrc = (insn >> 20) & 0x1;
        if (is_mrc)
            return OP_MRC;
        return OP_MCR;
    }

    if (bit4 == 0) {
        // CDP
        return OP_CDP;
    }
    // MRC, MCR
    uint8_t is_mrc = (insn >> 20) & 0x1;
    if (is_mrc)
        return OP_MRC;
    return OP_MCR;
}

Opcode ARM_Disasm::DecodeMUL(uint32_t insn) {
    uint8_t bit24 = (insn >> 24) & 0x1;
    if (bit24 != 0) {
        // This is an unexpected bit pattern.  Create an undefined
        // instruction in case this is ever executed.
        return OP_UNDEFINED;
    }
    uint8_t bit23 = (insn >> 23) & 0x1;
    uint8_t bit22_U = (insn >> 22) & 0x1;
    uint8_t bit21_A = (insn >> 21) & 0x1;
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

Opcode ARM_Disasm::DecodeLDRH(uint32_t insn) {
    uint8_t is_load = (insn >> 20) & 0x1;
    uint8_t bits_65 = (insn >> 5) & 0x3;
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

Opcode ARM_Disasm::DecodeALU(uint32_t insn) {
    uint8_t is_immed = (insn >> 25) & 0x1;
    uint8_t opcode = (insn >> 21) & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;
    uint8_t shift_is_reg = (insn >> 4) & 1;
    uint8_t bit7 = (insn >> 7) & 1;
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
