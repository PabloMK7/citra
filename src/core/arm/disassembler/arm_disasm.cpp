// Copyright 2006 The Android Open Source Project

#include <stdio.h>
#include <string.h>

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

const char *opcode_names[] = {
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

static const char* cond_to_str(int cond) {
    return cond_names[cond];
}

char *ARM_Disasm::disasm(uint32_t addr, uint32_t insn, char *result)
{
    static char   buf[80];
    char          *ptr;

    ptr = result ? result : buf;
    Opcode opcode = decode(insn);
    switch (opcode) {
        case OP_INVALID:
            sprintf(ptr, "Invalid");
            return ptr;
        case OP_UNDEFINED:
            sprintf(ptr, "Undefined");
            return ptr;
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
            return disasm_alu(opcode, insn, ptr);
        case OP_B:
        case OP_BL:
            return disasm_branch(addr, opcode, insn, ptr);
        case OP_BKPT:
            return disasm_bkpt(insn, ptr);
        case OP_BLX:
            // not supported yet
            break;
        case OP_BX:
            return disasm_bx(insn, ptr);
        case OP_CDP:
            sprintf(ptr, "cdp");
            return ptr;
        case OP_CLZ:
            return disasm_clz(insn, ptr);
        case OP_LDC:
            sprintf(ptr, "ldc");
            return ptr;
        case OP_LDM:
        case OP_STM:
            return disasm_memblock(opcode, insn, ptr);
        case OP_LDR:
        case OP_LDRB:
        case OP_LDRBT:
        case OP_LDRT:
        case OP_STR:
        case OP_STRB:
        case OP_STRBT:
        case OP_STRT:
            return disasm_mem(insn, ptr);
        case OP_LDRH:
        case OP_LDRSB:
        case OP_LDRSH:
        case OP_STRH:
            return disasm_memhalf(insn, ptr);
        case OP_MCR:
        case OP_MRC:
            return disasm_mcr(opcode, insn, ptr);
        case OP_MLA:
            return disasm_mla(opcode, insn, ptr);
        case OP_MRS:
            return disasm_mrs(insn, ptr);
        case OP_MSR:
            return disasm_msr(insn, ptr);
        case OP_MUL:
            return disasm_mul(opcode, insn, ptr);
        case OP_PLD:
            return disasm_pld(insn, ptr);
        case OP_STC:
            sprintf(ptr, "stc");
            return ptr;
        case OP_SWI:
            return disasm_swi(insn, ptr);
        case OP_SWP:
        case OP_SWPB:
            return disasm_swp(opcode, insn, ptr);
        case OP_UMLAL:
        case OP_UMULL:
        case OP_SMLAL:
        case OP_SMULL:
            return disasm_umlal(opcode, insn, ptr);
        default:
            sprintf(ptr, "Error");
            return ptr;
    }
    return NULL;
}

char *ARM_Disasm::disasm_alu(Opcode opcode, uint32_t insn, char *ptr)
{
    static const uint8_t kNoOperand1 = 1;
    static const uint8_t kNoDest = 2;
    static const uint8_t kNoSbit = 4;

    char rn_str[20];
    char rd_str[20];
    uint8_t flags = 0;
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t is_immed = (insn >> 25) & 0x1;
    uint8_t bit_s = (insn >> 20) & 1;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t immed = insn & 0xff;

    const char *opname = opcode_names[opcode];
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
        sprintf(rn_str, "r%d, ", rn);
    }

    // The following instructions do not write the result register (rd):
    // tst, teq, cmp, cmn.
    rd_str[0] = 0;
    if ((flags & kNoDest) == 0) {
        sprintf(rd_str, "r%d, ", rd);
    }

    const char *sbit_str = "";
    if (bit_s && !(flags & kNoSbit))
        sbit_str = "s";

    if (is_immed) {
        sprintf(ptr, "%s%s%s\t%s%s#%u  ; 0x%x",
                opname, cond_to_str(cond), sbit_str, rd_str, rn_str, immed, immed);
        return ptr;
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
        sprintf(ptr, "%s%s%s\t%s%sr%d",
                opname, cond_to_str(cond), sbit_str, rd_str, rn_str, rm);
        return ptr;
    }

    const char *shift_name = shift_names[shift_type];
    if (shift_is_reg) {
        sprintf(ptr, "%s%s%s\t%s%sr%d, %s r%d",
                opname, cond_to_str(cond), sbit_str, rd_str, rn_str, rm,
                shift_name, rs);
        return ptr;
    }
    if (shift_amount == 0) {
        if (shift_type == 3) {
            sprintf(ptr, "%s%s%s\t%s%sr%d, RRX",
                    opname, cond_to_str(cond), sbit_str, rd_str, rn_str, rm);
            return ptr;
        }
        shift_amount = 32;
    }
    sprintf(ptr, "%s%s%s\t%s%sr%d, %s #%u",
            opname, cond_to_str(cond), sbit_str, rd_str, rn_str, rm,
            shift_name, shift_amount);
    return ptr;
}

char *ARM_Disasm::disasm_branch(uint32_t addr, Opcode opcode, uint32_t insn, char *ptr)
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
    sprintf(ptr, "%s%s\t0x%x", opname, cond_to_str(cond), addr);
    return ptr;
}

char *ARM_Disasm::disasm_bx(uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rn = insn & 0xf;
    sprintf(ptr, "bx%s\tr%d", cond_to_str(cond), rn);
    return ptr;
}

char *ARM_Disasm::disasm_bkpt(uint32_t insn, char *ptr)
{
    uint32_t immed = (((insn >> 8) & 0xfff) << 4) | (insn & 0xf);
    sprintf(ptr, "bkpt\t#%d", immed);
    return ptr;
}

char *ARM_Disasm::disasm_clz(uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t rm = insn & 0xf;
    sprintf(ptr, "clz%s\tr%d, r%d", cond_to_str(cond), rd, rm);
    return ptr;
}

char *ARM_Disasm::disasm_memblock(Opcode opcode, uint32_t insn, char *ptr)
{
    char tmp_reg[10], tmp_list[80];

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
            sprintf(tmp_reg, "%sr%d", comma, ii);
            strcat(tmp_list, tmp_reg);
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

    sprintf(ptr, "%s%s%s\tr%d%s, {%s}%s",
            opname, cond_to_str(cond), addr_mode, rn, bang, tmp_list, carret);
    return ptr;
}

char *ARM_Disasm::disasm_mem(uint32_t insn, char *ptr)
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
                sprintf(ptr, "%s%s%s\tr%d, [r%d]",
                        opname, cond_to_str(cond), byte, rd, rn);
            } else {
                sprintf(ptr, "%s%s%s\tr%d, [r%d, #%s%u]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, offset, bang);
            }
        } else {
            const char *transfer = "";
            if (write_back)
                transfer = "t";
            sprintf(ptr, "%s%s%s%s\tr%d, [r%d], #%s%u",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, offset);
        }
        return ptr;
    }

    uint8_t rm = insn & 0xf;
    uint8_t shift_type = (insn >> 5) & 0x3;
    uint8_t shift_amount = (insn >> 7) & 0x1f;

    const char *shift_name = shift_names[shift_type];

    if (is_pre) {
        if (shift_amount == 0) {
            if (shift_type == 0) {
                sprintf(ptr, "%s%s%s\tr%d, [r%d, %sr%d]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, rm, bang);
                return ptr;
            }
            if (shift_type == 3) {
                sprintf(ptr, "%s%s%s\tr%d, [r%d, %sr%d, RRX]%s",
                        opname, cond_to_str(cond), byte, rd, rn, minus, rm, bang);
                return ptr;
            }
            shift_amount = 32;
        }
        sprintf(ptr, "%s%s%s\tr%d, [r%d, %sr%d, %s #%u]%s",
                opname, cond_to_str(cond), byte, rd, rn, minus, rm,
                shift_name, shift_amount, bang);
        return ptr;
    }

    const char *transfer = "";
    if (write_back)
        transfer = "t";

    if (shift_amount == 0) {
        if (shift_type == 0) {
            sprintf(ptr, "%s%s%s%s\tr%d, [r%d], %sr%d",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
            return ptr;
        }
        if (shift_type == 3) {
            sprintf(ptr, "%s%s%s%s\tr%d, [r%d], %sr%d, RRX",
                    opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm);
            return ptr;
        }
        shift_amount = 32;
    }

    sprintf(ptr, "%s%s%s%s\tr%d, [r%d], %sr%d, %s #%u",
            opname, cond_to_str(cond), byte, transfer, rd, rn, minus, rm,
            shift_name, shift_amount);
    return ptr;
}

char *ARM_Disasm::disasm_memhalf(uint32_t insn, char *ptr)
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
                sprintf(ptr, "%s%sh\tr%d, [r%d]", opname, cond_to_str(cond), rd, rn);
            } else {
                sprintf(ptr, "%s%sh\tr%d, [r%d, #%s%u]%s",
                        opname, cond_to_str(cond), rd, rn, minus, offset, bang);
            }
        } else {
            sprintf(ptr, "%s%sh\tr%d, [r%d], #%s%u",
                    opname, cond_to_str(cond), rd, rn, minus, offset);
        }
        return ptr;
    }

    if (is_pre) {
        sprintf(ptr, "%s%sh\tr%d, [r%d, %sr%d]%s",
                opname, cond_to_str(cond), rd, rn, minus, rm, bang);
    } else {
        sprintf(ptr, "%s%sh\tr%d, [r%d], %sr%d",
                opname, cond_to_str(cond), rd, rn, minus, rm);
    }
    return ptr;
}

char *ARM_Disasm::disasm_mcr(Opcode opcode, uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t crn = (insn >> 16) & 0xf;
    uint8_t crd = (insn >> 12) & 0xf;
    uint8_t cpnum = (insn >> 8) & 0xf;
    uint8_t opcode2 = (insn >> 5) & 0x7;
    uint8_t crm = insn & 0xf;

    const char *opname = opcode_names[opcode];
    sprintf(ptr, "%s%s\t%d, 0, r%d, cr%d, cr%d, {%d}",
            opname, cond_to_str(cond), cpnum, crd, crn, crm, opcode2);
    return ptr;
}

char *ARM_Disasm::disasm_mla(Opcode opcode, uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 16) & 0xf;
    uint8_t rn = (insn >> 12) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    sprintf(ptr, "%s%s%s\tr%d, r%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rd, rm, rs, rn);
    return ptr;
}

char *ARM_Disasm::disasm_umlal(Opcode opcode, uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rdhi = (insn >> 16) & 0xf;
    uint8_t rdlo = (insn >> 12) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    sprintf(ptr, "%s%s%s\tr%d, r%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rdlo, rdhi, rm, rs);
    return ptr;
}

char *ARM_Disasm::disasm_mul(Opcode opcode, uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 16) & 0xf;
    uint8_t rs = (insn >> 8) & 0xf;
    uint8_t rm = insn & 0xf;
    uint8_t bit_s = (insn >> 20) & 1;

    const char *opname = opcode_names[opcode];
    sprintf(ptr, "%s%s%s\tr%d, r%d, r%d",
            opname, cond_to_str(cond), bit_s ? "s" : "", rd, rm, rs);
    return ptr;
}

char *ARM_Disasm::disasm_mrs(uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t ps = (insn >> 22) & 1;

    sprintf(ptr, "mrs%s\tr%d, %s", cond_to_str(cond), rd, ps ? "spsr" : "cpsr");
    return ptr;
}

char *ARM_Disasm::disasm_msr(uint32_t insn, char *ptr)
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
        sprintf(ptr, "msr%s\t%s_%s, #0x%x",
                cond_to_str(cond), pd ? "spsr" : "cpsr", flags, rotated_val);
        return ptr;
    }

    uint8_t rm = insn & 0xf;

    sprintf(ptr, "msr%s\t%s_%s, r%d",
            cond_to_str(cond), pd ? "spsr" : "cpsr", flags, rm);
    return ptr;
}

char *ARM_Disasm::disasm_pld(uint32_t insn, char *ptr)
{
    uint8_t is_reg = (insn >> 25) & 0x1;
    uint8_t is_up = (insn >> 23) & 0x1;
    uint8_t rn = (insn >> 16) & 0xf;

    const char *minus = "";
    if (is_up == 0)
        minus = "-";

    if (is_reg) {
        uint8_t rm = insn & 0xf;
        sprintf(ptr, "pld\t[r%d, %sr%d]", rn, minus, rm);
        return ptr;
    }

    uint16_t offset = insn & 0xfff;
    if (offset == 0) {
        sprintf(ptr, "pld\t[r%d]", rn);
    } else {
        sprintf(ptr, "pld\t[r%d, #%s%u]", rn, minus, offset);
    }
    return ptr;
}

char *ARM_Disasm::disasm_swi(uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint32_t sysnum = insn & 0x00ffffff;

    sprintf(ptr, "swi%s 0x%x", cond_to_str(cond), sysnum);
    return ptr;
}

char *ARM_Disasm::disasm_swp(Opcode opcode, uint32_t insn, char *ptr)
{
    uint8_t cond = (insn >> 28) & 0xf;
    uint8_t rn = (insn >> 16) & 0xf;
    uint8_t rd = (insn >> 12) & 0xf;
    uint8_t rm = insn & 0xf;

    const char *opname = opcode_names[opcode];
    sprintf(ptr, "%s%s\tr%d, r%d, [r%d]", opname, cond_to_str(cond), rd, rm, rn);
    return ptr;
}

Opcode ARM_Disasm::decode(uint32_t insn) {
    uint32_t bits27_26 = (insn >> 26) & 0x3;
    switch (bits27_26) {
        case 0x0:
            return decode00(insn);
        case 0x1:
            return decode01(insn);
        case 0x2:
            return decode10(insn);
        case 0x3:
            return decode11(insn);
    }
    return OP_INVALID;
}

Opcode ARM_Disasm::decode00(uint32_t insn) {
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
            return decode_mul(insn);
        }

        uint8_t bit7 = (insn >> 7) & 0x1;
        if (bit7 == 1) {
            // One of the load/store halfword/byte instructions
            return decode_ldrh(insn);
        }
    }

    // One of the data processing instructions
    return decode_alu(insn);
}

Opcode ARM_Disasm::decode01(uint32_t insn) {
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

Opcode ARM_Disasm::decode10(uint32_t insn) {
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

Opcode ARM_Disasm::decode11(uint32_t insn) {
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

Opcode ARM_Disasm::decode_mul(uint32_t insn) {
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

Opcode ARM_Disasm::decode_ldrh(uint32_t insn) {
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

Opcode ARM_Disasm::decode_alu(uint32_t insn) {
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