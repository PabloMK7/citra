/*
    armvfp.c - ARM VFPv3 emulation unit
    Copyright (C) 2003 Skyeye Develop Group
    for help please send mail to <skyeye-developer@lists.gro.clinux.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Note: this file handles interface with arm core and vfp registers */

#include "common/common.h"

#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/vfp/asm_vfp.h"
#include "core/arm/skyeye_common/vfp/vfp.h"

unsigned VFPInit(ARMul_State* state)
{
    state->VFP[VFP_OFFSET(VFP_FPSID)] = VFP_FPSID_IMPLMEN<<24 | VFP_FPSID_SW<<23 | VFP_FPSID_SUBARCH<<16 |
                                        VFP_FPSID_PARTNUM<<8 | VFP_FPSID_VARIANT<<4 | VFP_FPSID_REVISION;
    state->VFP[VFP_OFFSET(VFP_FPEXC)] = 0;
    state->VFP[VFP_OFFSET(VFP_FPSCR)] = 0;

    return 0;
}

unsigned VFPMRC(ARMul_State* state, unsigned type, u32 instr, u32* value)
{
    /* MRC<c> <coproc>,<opc1>,<Rt>,<CRn>,<CRm>{,<opc2>} */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int OPC_1 = BITS(instr, 21, 23);
    int Rt = BITS(instr, 12, 15);
    int CRn = BITS(instr, 16, 19);
    int CRm = BITS(instr, 0, 3);
    int OPC_2 = BITS(instr, 5, 7);

    /* TODO check access permission */

    /* CRn/opc1 CRm/opc2 */

    if (CoProc == 10 || CoProc == 11)
    {
        if (OPC_1 == 0x0 && CRm == 0 && (OPC_2 & 0x3) == 0)
        {
            /* VMOV r to s */
            /* Transfering Rt is not mandatory, as the value of interest is pointed by value */
            VMOVBRS(state, BIT(instr, 20), Rt, BIT(instr, 7)|CRn<<1, value);
            return ARMul_DONE;
        }

        if (OPC_1 == 0x7 && CRm == 0 && OPC_2 == 0)
        {
            VMRS(state, CRn, Rt, value);
            return ARMul_DONE;
        }
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, OPC_1 %x, Rt %x, CRn %x, CRm %x, OPC_2 %x\n",
          instr, CoProc, OPC_1, Rt, CRn, CRm, OPC_2);

    return ARMul_CANT;
}

unsigned VFPMCR(ARMul_State* state, unsigned type, u32 instr, u32 value)
{
    /* MCR<c> <coproc>,<opc1>,<Rt>,<CRn>,<CRm>{,<opc2>} */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int OPC_1 = BITS(instr, 21, 23);
    int Rt = BITS(instr, 12, 15);
    int CRn = BITS(instr, 16, 19);
    int CRm = BITS(instr, 0, 3);
    int OPC_2 = BITS(instr, 5, 7);

    /* TODO check access permission */

    /* CRn/opc1 CRm/opc2 */
    if (CoProc == 10 || CoProc == 11)
    {
        if (OPC_1 == 0x0 && CRm == 0 && (OPC_2 & 0x3) == 0)
        {
            /* VMOV s to r */
            /* Transfering Rt is not mandatory, as the value of interest is pointed by value */
            VMOVBRS(state, BIT(instr, 20), Rt, BIT(instr, 7)|CRn<<1, &value);
            return ARMul_DONE;
        }

        if (OPC_1 == 0x7 && CRm == 0 && OPC_2 == 0)
        {
            VMSR(state, CRn, Rt);
            return ARMul_DONE;
        }

        if ((OPC_1 & 0x4) == 0 && CoProc == 11 && CRm == 0)
        {
            VFP_DEBUG_UNIMPLEMENTED(VMOVBRC);
            return ARMul_DONE;
        }

        if (CoProc == 11 && CRm == 0)
        {
            VFP_DEBUG_UNIMPLEMENTED(VMOVBCR);
            return ARMul_DONE;
        }
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, OPC_1 %x, Rt %x, CRn %x, CRm %x, OPC_2 %x\n",
          instr, CoProc, OPC_1, Rt, CRn, CRm, OPC_2);

    return ARMul_CANT;
}

unsigned VFPMRRC(ARMul_State* state, unsigned type, u32 instr, u32* value1, u32* value2)
{
    /* MCRR<c> <coproc>,<opc1>,<Rt>,<Rt2>,<CRm> */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int OPC_1 = BITS(instr, 4, 7);
    int Rt = BITS(instr, 12, 15);
    int Rt2 = BITS(instr, 16, 19);
    int CRm = BITS(instr, 0, 3);

    if (CoProc == 10 || CoProc == 11)
    {
        if (CoProc == 10 && (OPC_1 & 0xD) == 1)
        {
            VMOVBRRSS(state, BIT(instr, 20), Rt, Rt2, BIT(instr, 5)<<4|CRm, value1, value2);
            return ARMul_DONE;
        }

        if (CoProc == 11 && (OPC_1 & 0xD) == 1)
        {
            /* Transfering Rt and Rt2 is not mandatory, as the value of interest is pointed by value1 and value2 */
            VMOVBRRD(state, BIT(instr, 20), Rt, Rt2, BIT(instr, 5)<<4|CRm, value1, value2);
            return ARMul_DONE;
        }
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, OPC_1 %x, Rt %x, Rt2 %x, CRm %x\n",
          instr, CoProc, OPC_1, Rt, Rt2, CRm);

    return ARMul_CANT;
}

unsigned VFPMCRR(ARMul_State* state, unsigned type, u32 instr, u32 value1, u32 value2)
{
    /* MCRR<c> <coproc>,<opc1>,<Rt>,<Rt2>,<CRm> */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int OPC_1 = BITS(instr, 4, 7);
    int Rt = BITS(instr, 12, 15);
    int Rt2 = BITS(instr, 16, 19);
    int CRm = BITS(instr, 0, 3);

    /* TODO check access permission */

    /* CRn/opc1 CRm/opc2 */

    if (CoProc == 11 || CoProc == 10)
    {
        if (CoProc == 10 && (OPC_1 & 0xD) == 1)
        {
            VMOVBRRSS(state, BIT(instr, 20), Rt, Rt2, BIT(instr, 5)<<4|CRm, &value1, &value2);
            return ARMul_DONE;
        }

        if (CoProc == 11 && (OPC_1 & 0xD) == 1)
        {
            /* Transfering Rt and Rt2 is not mandatory, as the value of interest is pointed by value1 and value2 */
            VMOVBRRD(state, BIT(instr, 20), Rt, Rt2, BIT(instr, 5)<<4|CRm, &value1, &value2);
            return ARMul_DONE;
        }
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, OPC_1 %x, Rt %x, Rt2 %x, CRm %x\n",
          instr, CoProc, OPC_1, Rt, Rt2, CRm);

    return ARMul_CANT;
}

unsigned VFPSTC(ARMul_State* state, unsigned type, u32 instr, u32 * value)
{
    /* STC{L}<c> <coproc>,<CRd>,[<Rn>],<option> */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int CRd = BITS(instr, 12, 15);
    int Rn = BITS(instr, 16, 19);
    int imm8 = BITS(instr, 0, 7);
    int P = BIT(instr, 24);
    int U = BIT(instr, 23);
    int D = BIT(instr, 22);
    int W = BIT(instr, 21);

    /* TODO check access permission */

    /* VSTM */
    if ( (P|U|D|W) == 0 ) {
        LOG_ERROR(Core_ARM11, "In %s, UNDEFINED\n", __FUNCTION__);
        exit(-1);
    }
    if (CoProc == 10 || CoProc == 11) {
#if 1
        if (P == 0 && U == 0 && W == 0) {
            LOG_ERROR(Core_ARM11, "VSTM Related encodings\n");
            exit(-1);
        }
        if (P == U && W == 1) {
            LOG_ERROR(Core_ARM11, "UNDEFINED\n");
            exit(-1);
        }
#endif

        if (P == 1 && W == 0)
        {
            return VSTR(state, type, instr, value);
        }

        if (P == 1 && U == 0 && W == 1 && Rn == 0xD)
        {
            return VPUSH(state, type, instr, value);
        }

        return VSTM(state, type, instr, value);
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, CRd %x, Rn %x, imm8 %x, P %x, U %x, D %x, W %x\n",
          instr, CoProc, CRd, Rn, imm8, P, U, D, W);

    return ARMul_CANT;
}

unsigned VFPLDC(ARMul_State* state, unsigned type, u32 instr, u32 value)
{
    /* LDC{L}<c> <coproc>,<CRd>,[<Rn>] */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int CRd = BITS(instr, 12, 15);
    int Rn = BITS(instr, 16, 19);
    int imm8 = BITS(instr, 0, 7);
    int P = BIT(instr, 24);
    int U = BIT(instr, 23);
    int D = BIT(instr, 22);
    int W = BIT(instr, 21);

    /* TODO check access permission */

    if ( (P|U|D|W) == 0 ) {
        LOG_ERROR(Core_ARM11, "In %s, UNDEFINED\n", __FUNCTION__);
        exit(-1);
    }
    if (CoProc == 10 || CoProc == 11)
    {
        if (P == 1 && W == 0)
        {
            return VLDR(state, type, instr, value);
        }

        if (P == 0 && U == 1 && W == 1 && Rn == 0xD)
        {
            return VPOP(state, type, instr, value);
        }

        return VLDM(state, type, instr, value);
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x, CoProc %x, CRd %x, Rn %x, imm8 %x, P %x, U %x, D %x, W %x\n",
          instr, CoProc, CRd, Rn, imm8, P, U, D, W);

    return ARMul_CANT;
}

unsigned VFPCDP(ARMul_State* state, unsigned type, u32 instr)
{
    /* CDP<c> <coproc>,<opc1>,<CRd>,<CRn>,<CRm>,<opc2> */
    int CoProc = BITS(instr, 8, 11); /* 10 or 11 */
    int OPC_1 = BITS(instr, 20, 23);
    int CRd = BITS(instr, 12, 15);
    int CRn = BITS(instr, 16, 19);
    int CRm = BITS(instr, 0, 3);
    int OPC_2 = BITS(instr, 5, 7);

    /* TODO check access permission */

    /* CRn/opc1 CRm/opc2 */

    if (CoProc == 10 || CoProc == 11)
    {
        if ((OPC_1 & 0xB) == 0xB && BITS(instr, 4, 7) == 0)
        {
            unsigned int single = BIT(instr, 8) == 0;
            unsigned int d      = (single ? BITS(instr, 12,15)<<1 | BIT(instr, 22) : BITS(instr, 12,15) | BIT(instr, 22)<<4);
            unsigned int imm;
            instr = BITS(instr, 16, 19) << 4 | BITS(instr, 0, 3); // FIXME dirty workaround to get a correct imm

            if (single)
                imm = BIT(instr, 7)<<31 | (BIT(instr, 6)==0)<<30 | (BIT(instr, 6) ? 0x1f : 0)<<25 | BITS(instr, 0, 5)<<19;
            else
                imm = BIT(instr, 7)<<31 | (BIT(instr, 6)==0)<<30 | (BIT(instr, 6) ? 0xff : 0)<<22 | BITS(instr, 0, 5)<<16;

            VMOVI(state, single, d, imm);
            return ARMul_DONE;
        }

        if ((OPC_1 & 0xB) == 0xB && CRn == 0 && (OPC_2 & 0x6) == 0x2)
        {
            unsigned int single = BIT(instr, 8) == 0;
            unsigned int d      = (single ? BITS(instr, 12,15)<<1 | BIT(instr, 22) : BITS(instr, 12,15) | BIT(instr, 22)<<4);
            unsigned int m      = (single ? BITS(instr,  0, 3)<<1 | BIT(instr,  5) : BITS(instr,  0, 3) | BIT(instr, 5)<<4);
            VMOVR(state, single, d, m);
            return ARMul_DONE;
        }

        int exceptions = 0;
        if (CoProc == 10)
            exceptions = vfp_single_cpdo(state, instr, state->VFP[VFP_OFFSET(VFP_FPSCR)]);
        else
            exceptions = vfp_double_cpdo(state, instr, state->VFP[VFP_OFFSET(VFP_FPSCR)]);

        vfp_raise_exceptions(state, exceptions, instr, state->VFP[VFP_OFFSET(VFP_FPSCR)]);

        return ARMul_DONE;
    }
    LOG_WARNING(Core_ARM11, "Can't identify %x\n", instr);
    return ARMul_CANT;
}

/* ----------- MRC ------------ */
void VMOVBRS(ARMul_State* state, ARMword to_arm, ARMword t, ARMword n, ARMword* value)
{
    if (to_arm)
    {
        *value = state->ExtReg[n];
    }
    else
    {
        state->ExtReg[n] = *value;
    }
}
void VMRS(ARMul_State* state, ARMword reg, ARMword Rt, ARMword* value)
{
    if (reg == 1)
    {
        if (Rt != 15)
        {
            *value = state->VFP[VFP_OFFSET(VFP_FPSCR)];
        }
        else
        {
            *value = state->VFP[VFP_OFFSET(VFP_FPSCR)] ;
        }
    }
    else
    {
        switch (reg)
        {
            case 0:
                *value = state->VFP[VFP_OFFSET(VFP_FPSID)];
                break;
            case 6:
                /* MVFR1, VFPv3 only ? */
                LOG_TRACE(Core_ARM11, "\tr%d <= MVFR1 unimplemented\n", Rt);
                break;
            case 7:
                /* MVFR0, VFPv3 only? */
                LOG_TRACE(Core_ARM11, "\tr%d <= MVFR0 unimplemented\n", Rt);
                break;
            case 8:
                *value = state->VFP[VFP_OFFSET(VFP_FPEXC)];
                break;
            default:
                LOG_TRACE(Core_ARM11, "\tSUBARCHITECTURE DEFINED\n");
                break;
        }
    }
}
void VMOVBRRD(ARMul_State* state, ARMword to_arm, ARMword t, ARMword t2, ARMword n, ARMword* value1, ARMword* value2)
{
    if (to_arm)
    {
        *value2 = state->ExtReg[n*2+1];
        *value1 = state->ExtReg[n*2];
    }
    else
    {
        state->ExtReg[n*2+1] = *value2;
        state->ExtReg[n*2] = *value1;
    }
}
void VMOVBRRSS(ARMul_State* state, ARMword to_arm, ARMword t, ARMword t2, ARMword n, ARMword* value1, ARMword* value2)
{
    if (to_arm)
    {
        *value1 = state->ExtReg[n+0];
        *value2 = state->ExtReg[n+1];
    }
    else
    {
        state->ExtReg[n+0] = *value1;
        state->ExtReg[n+1] = *value2;
    }
}

/* ----------- MCR ------------ */
void VMSR(ARMul_State* state, ARMword reg, ARMword Rt)
{
    if (reg == 1)
    {
        state->VFP[VFP_OFFSET(VFP_FPSCR)] = state->Reg[Rt];
    }
    else if (reg == 8)
    {
        state->VFP[VFP_OFFSET(VFP_FPEXC)] = state->Reg[Rt];
    }
}

/* Memory operation are not inlined, as old Interpreter and Fast interpreter
   don't have the same memory operation interface.
   Old interpreter framework does one access to coprocessor per data, and
   handles already data write, as well as address computation,
   which is not the case for Fast interpreter. Therefore, implementation
   of vfp instructions in old interpreter and fast interpreter are separate. */

/* ----------- STC ------------ */
int VSTR(ARMul_State* state, int type, ARMword instr, ARMword* value)
{
    static int i = 0;
    static int single_reg, add, d, n, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_reg = BIT(instr, 8) == 0; // Double precision
        add = BIT(instr, 23);
        imm32 = BITS(instr, 0,7)<<2; // may not be used
        d = single_reg ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); /* Base register */
        n = BITS(instr, 16, 19); // destination register

        i = 0;
        regs = 1;

        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_reg)
        {
            *value = state->ExtReg[d+i];
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            *value = state->ExtReg[d*2+i];
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}
int VPUSH(ARMul_State* state, int type, ARMword instr, ARMword* value)
{
    static int i = 0;
    static int single_regs, d, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_regs = BIT(instr, 8) == 0; // Single precision
        d = single_regs ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); // Base register
        imm32 = BITS(instr, 0,7)<<2; // may not be used
        regs = single_regs ? BITS(instr, 0, 7) : BITS(instr, 1, 7); // FSTMX if regs is odd

        state->Reg[R13] = state->Reg[R13] - imm32;

        i = 0;

        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_regs)
        {
            *value = state->ExtReg[d + i];
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            *value = state->ExtReg[d*2 + i];
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}
int VSTM(ARMul_State* state, int type, ARMword instr, ARMword* value)
{
    static int i = 0;
    static int single_regs, add, wback, d, n, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_regs = BIT(instr, 8) == 0; // Single precision
        add = BIT(instr, 23);
        wback = BIT(instr, 21); // write-back
        d = single_regs ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); // Base register
        n = BITS(instr, 16, 19); // destination register
        imm32 = BITS(instr, 0,7) * 4; // may not be used
        regs = single_regs ? BITS(instr, 0, 7) : BITS(instr, 0, 7)>>1; // FSTMX if regs is odd

        if (wback) {
            state->Reg[n] = (add ? state->Reg[n] + imm32 : state->Reg[n] - imm32);
        }

        i = 0;

        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_regs)
        {
            *value = state->ExtReg[d + i];
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            *value = state->ExtReg[d*2 + i];
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}

/* ----------- LDC ------------ */
int VPOP(ARMul_State* state, int type, ARMword instr, ARMword value)
{
    static int i = 0;
    static int single_regs, d, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_regs = BIT(instr, 8) == 0; // Single precision
        d = single_regs ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); // Base register
        imm32 = BITS(instr, 0, 7)<<2; // may not be used
        regs = single_regs ? BITS(instr, 0, 7) : BITS(instr, 1, 7); // FLDMX if regs is odd

        state->Reg[R13] = state->Reg[R13] + imm32;

        i = 0;

        return ARMul_DONE;
    }
    else if (type == ARMul_TRANSFER)
    {
        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_regs)
        {
            state->ExtReg[d + i] = value;
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            state->ExtReg[d*2 + i] = value;
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}
int VLDR(ARMul_State* state, int type, ARMword instr, ARMword value)
{
    static int i = 0;
    static int single_reg, add, d, n, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_reg = BIT(instr, 8) == 0; // Double precision
        add = BIT(instr, 23);
        imm32 = BITS(instr, 0, 7)<<2; // may not be used
        d = single_reg ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); // Base register
        n = BITS(instr, 16, 19); // destination register

        i = 0;
        regs = 1;

        return ARMul_DONE;
    }
    else if (type == ARMul_TRANSFER)
    {
        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_reg)
        {
            state->ExtReg[d+i] = value;
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            state->ExtReg[d*2+i] = value;
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}
int VLDM(ARMul_State* state, int type, ARMword instr, ARMword value)
{
    static int i = 0;
    static int single_regs, add, wback, d, n, imm32, regs;
    if (type == ARMul_FIRST)
    {
        single_regs = BIT(instr, 8) == 0; // Single precision
        add = BIT(instr, 23);
        wback = BIT(instr, 21); // write-back
        d = single_regs ? BITS(instr, 12, 15)<<1|BIT(instr, 22) : BIT(instr, 22)<<4|BITS(instr, 12, 15); // Base register
        n = BITS(instr, 16, 19); // destination register
        imm32 = BITS(instr, 0, 7) * 4; // may not be used
        regs = single_regs ? BITS(instr, 0, 7) : BITS(instr, 0, 7)>>1; // FLDMX if regs is odd

        if (wback) {
            state->Reg[n] = (add ? state->Reg[n] + imm32 : state->Reg[n] - imm32);
        }

        i = 0;

        return ARMul_DONE;
    }
    else if (type == ARMul_DATA)
    {
        if (single_regs)
        {
            state->ExtReg[d + i] = value;
            i++;
            if (i < regs)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
        else
        {
            /* FIXME Careful of endianness, may need to rework this */
            state->ExtReg[d*2 + i] = value;
            i++;
            if (i < regs*2)
                return ARMul_INC;
            else
                return ARMul_DONE;
        }
    }

    return -1;
}

/* ----------- CDP ------------ */
void VMOVI(ARMul_State* state, ARMword single, ARMword d, ARMword imm)
{
    if (single)
    {
        state->ExtReg[d] = imm;
    }
    else
    {
        /* Check endian please */
        state->ExtReg[d*2+1] = imm;
        state->ExtReg[d*2] = 0;
    }
}
void VMOVR(ARMul_State* state, ARMword single, ARMword d, ARMword m)
{
    if (single)
    {
        state->ExtReg[d] = state->ExtReg[m];
    }
    else
    {
        /* Check endian please */
        state->ExtReg[d*2+1] = state->ExtReg[m*2+1];
        state->ExtReg[d*2] = state->ExtReg[m*2];
    }
}

/* Miscellaneous functions */
int32_t vfp_get_float(arm_core_t* state, unsigned int reg)
{
    LOG_TRACE(Core_ARM11, "VFP get float: s%d=[%08x]\n", reg, state->ExtReg[reg]);
    return state->ExtReg[reg];
}

void vfp_put_float(arm_core_t* state, int32_t val, unsigned int reg)
{
    LOG_TRACE(Core_ARM11, "VFP put float: s%d <= [%08x]\n", reg, val);
    state->ExtReg[reg] = val;
}

uint64_t vfp_get_double(arm_core_t* state, unsigned int reg)
{
    uint64_t result = ((uint64_t) state->ExtReg[reg*2+1])<<32 | state->ExtReg[reg*2];
    LOG_TRACE(Core_ARM11, "VFP get double: s[%d-%d]=[%016llx]\n", reg * 2 + 1, reg * 2, result);
    return result;
}

void vfp_put_double(arm_core_t* state, uint64_t val, unsigned int reg)
{
    LOG_TRACE(Core_ARM11, "VFP put double: s[%d-%d] <= [%08x-%08x]\n", reg * 2 + 1, reg * 2, (uint32_t)(val >> 32), (uint32_t)(val & 0xffffffff));
    state->ExtReg[reg*2] = (uint32_t) (val & 0xffffffff);
    state->ExtReg[reg*2+1] = (uint32_t) (val>>32);
}

/*
 * Process bitmask of exception conditions. (from vfpmodule.c)
 */
void vfp_raise_exceptions(ARMul_State* state, u32 exceptions, u32 inst, u32 fpscr)
{
    LOG_TRACE(Core_ARM11, "VFP: raising exceptions %08x\n", exceptions);

    if (exceptions == VFP_EXCEPTION_ERROR) {
        LOG_TRACE(Core_ARM11, "unhandled bounce %x\n", inst);
        exit(-1);
        return;
    }

    /*
     * If any of the status flags are set, update the FPSCR.
     * Comparison instructions always return at least one of
     * these flags set.
     */
    if (exceptions & (FPSCR_NFLAG|FPSCR_ZFLAG|FPSCR_CFLAG|FPSCR_VFLAG))
        fpscr &= ~(FPSCR_NFLAG|FPSCR_ZFLAG|FPSCR_CFLAG|FPSCR_VFLAG);

    fpscr |= exceptions;

    state->VFP[VFP_OFFSET(VFP_FPSCR)] = fpscr;
}
