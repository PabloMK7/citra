/* 
    vfp/vfp.h - ARM VFPv3 emulation unit - vfp interface
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

#ifndef __VFP_H__
#define __VFP_H__

#define DBG(...) //DEBUG_LOG(ARM11, __VA_ARGS__)

#define vfpdebug //printf

#include "core/arm/interpreter/vfp/vfp_helper.h" /* for references to cdp SoftFloat functions */

unsigned VFPInit (ARMul_State *state);
unsigned VFPMRC (ARMul_State * state, unsigned type, ARMword instr, ARMword * value);
unsigned VFPMCR (ARMul_State * state, unsigned type, ARMword instr, ARMword value);
unsigned VFPMRRC (ARMul_State * state, unsigned type, ARMword instr, ARMword * value1, ARMword * value2);
unsigned VFPMCRR (ARMul_State * state, unsigned type, ARMword instr, ARMword value1, ARMword value2);
unsigned VFPSTC (ARMul_State * state, unsigned type, ARMword instr, ARMword * value);
unsigned VFPLDC (ARMul_State * state, unsigned type, ARMword instr, ARMword value);
unsigned VFPCDP (ARMul_State * state, unsigned type, ARMword instr);

/* FPSID Information */
#define VFP_FPSID_IMPLMEN 0 	/* should be the same as cp15 0 c0 0*/
#define VFP_FPSID_SW 0
#define VFP_FPSID_SUBARCH 0x2 	/* VFP version. Current is v3 (not strict) */
#define VFP_FPSID_PARTNUM 0x1
#define VFP_FPSID_VARIANT 0x1
#define VFP_FPSID_REVISION 0x1

/* FPEXC Flags */
#define VFP_FPEXC_EX 1<<31
#define VFP_FPEXC_EN 1<<30

/* FPSCR Flags */
#define VFP_FPSCR_NFLAG 1<<31
#define VFP_FPSCR_ZFLAG 1<<30
#define VFP_FPSCR_CFLAG 1<<29
#define VFP_FPSCR_VFLAG 1<<28

#define VFP_FPSCR_AHP 1<<26 	/* Alternative Half Precision */
#define VFP_FPSCR_DN 1<<25 	/* Default NaN */
#define VFP_FPSCR_FZ 1<<24 	/* Flush-to-zero */
#define VFP_FPSCR_RMODE 3<<22 	/* Rounding Mode */
#define VFP_FPSCR_STRIDE 3<<20 	/* Stride (vector) */
#define VFP_FPSCR_LEN 7<<16 	/* Stride (vector) */

#define VFP_FPSCR_IDE 1<<15	/* Input Denormal exc */
#define VFP_FPSCR_IXE 1<<12	/* Inexact exc */
#define VFP_FPSCR_UFE 1<<11	/* Undeflow exc */
#define VFP_FPSCR_OFE 1<<10	/* Overflow exc */
#define VFP_FPSCR_DZE 1<<9	/* Division by Zero exc */
#define VFP_FPSCR_IOE 1<<8	/* Invalid Operation exc */

#define VFP_FPSCR_IDC 1<<7	/* Input Denormal cum exc */
#define VFP_FPSCR_IXC 1<<4	/* Inexact cum exc */
#define VFP_FPSCR_UFC 1<<3	/* Undeflow cum exc */
#define VFP_FPSCR_OFC 1<<2	/* Overflow cum exc */
#define VFP_FPSCR_DZC 1<<1	/* Division by Zero cum exc */
#define VFP_FPSCR_IOC 1<<0	/* Invalid Operation cum exc */

/* Inline instructions. Note: Used in a cpp file as well */
#ifdef __cplusplus
 extern "C" {
#endif
int32_t vfp_get_float(ARMul_State * state, unsigned int reg);
void vfp_put_float(ARMul_State * state, int32_t val, unsigned int reg);
uint64_t vfp_get_double(ARMul_State * state, unsigned int reg);
void vfp_put_double(ARMul_State * state, uint64_t val, unsigned int reg);
void vfp_raise_exceptions(ARMul_State * state, uint32_t exceptions, uint32_t inst, uint32_t fpscr);
u32 vfp_single_cpdo(ARMul_State* state, u32 inst, u32 fpscr);
u32 vfp_double_cpdo(ARMul_State* state, u32 inst, u32 fpscr);

/* MRC */
inline void VMRS(ARMul_State * state, ARMword reg, ARMword Rt, ARMword *value);
inline void VMOVBRS(ARMul_State * state, ARMword to_arm, ARMword t, ARMword n, ARMword *value);
inline void VMOVBRRD(ARMul_State * state, ARMword to_arm, ARMword t, ARMword t2, ARMword n, ARMword *value1, ARMword *value2);
inline void VMOVI(ARMul_State * state, ARMword single, ARMword d, ARMword imm);
inline void VMOVR(ARMul_State * state, ARMword single, ARMword d, ARMword imm);
/* MCR */
inline void VMSR(ARMul_State * state, ARMword reg, ARMword Rt);
/* STC */
inline int VSTM(ARMul_State * state, int type, ARMword instr, ARMword* value);
inline int VPUSH(ARMul_State * state, int type, ARMword instr, ARMword* value);
inline int VSTR(ARMul_State * state, int type, ARMword instr, ARMword* value);
/* LDC */
inline int VLDM(ARMul_State * state, int type, ARMword instr, ARMword value);
inline int VPOP(ARMul_State * state, int type, ARMword instr, ARMword value);
inline int VLDR(ARMul_State * state, int type, ARMword instr, ARMword value);

#ifdef __cplusplus
 }
#endif

#endif
