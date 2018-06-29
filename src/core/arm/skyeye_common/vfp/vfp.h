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

#pragma once

#include "core/arm/skyeye_common/vfp/vfp_helper.h" /* for references to cdp SoftFloat functions */

#define VFP_DEBUG_UNTESTED(x) LOG_TRACE(Core_ARM11, "in func {}, " #x " untested", __FUNCTION__);
#define CHECK_VFP_ENABLED
#define CHECK_VFP_CDP_RET vfp_raise_exceptions(cpu, ret, inst_cream->instr, cpu->VFP[VFP_FPSCR]);

void VFPInit(ARMul_State* state);

s32 vfp_get_float(ARMul_State* state, u32 reg);
void vfp_put_float(ARMul_State* state, s32 val, u32 reg);
u64 vfp_get_double(ARMul_State* state, u32 reg);
void vfp_put_double(ARMul_State* state, u64 val, u32 reg);
void vfp_raise_exceptions(ARMul_State* state, u32 exceptions, u32 inst, u32 fpscr);
u32 vfp_single_cpdo(ARMul_State* state, u32 inst, u32 fpscr);
u32 vfp_double_cpdo(ARMul_State* state, u32 inst, u32 fpscr);

void VMOVBRS(ARMul_State* state, u32 to_arm, u32 t, u32 n, u32* value);
void VMOVBRRD(ARMul_State* state, u32 to_arm, u32 t, u32 t2, u32 n, u32* value1, u32* value2);
void VMOVBRRSS(ARMul_State* state, u32 to_arm, u32 t, u32 t2, u32 n, u32* value1, u32* value2);
void VMOVI(ARMul_State* state, u32 single, u32 d, u32 imm);
void VMOVR(ARMul_State* state, u32 single, u32 d, u32 imm);
