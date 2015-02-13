/* Copyright (C)
* 2011 - Michael.Kang blackfin.kang@gmail.com
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#pragma once

#include "core/arm/skyeye_common/armdefs.h"

void switch_mode(ARMul_State* core, uint32_t mode);

/* FIXME, we temporarily think thumb instruction is always 16 bit */
static inline u32 GET_INST_SIZE(ARMul_State* core) {
    return core->TFlag? 2 : 4;
}

/**
* @brief Read R15 and forced R15 to wold align, used address calculation
*
* @param core
* @param Rn
*
* @return 
*/
static inline addr_t CHECK_READ_REG15_WA(ARMul_State* core, int Rn) {
    return (Rn == 15)? ((core->Reg[15] & ~0x3) + GET_INST_SIZE(core) * 2) : core->Reg[Rn];
}

/**
* @brief Read R15, used to data processing with pc
*
* @param core
* @param Rn
*
* @return 
*/
static inline u32 CHECK_READ_REG15(ARMul_State* core, int Rn) {
    return (Rn == 15)? ((core->Reg[15] & ~0x1) + GET_INST_SIZE(core) * 2) : core->Reg[Rn];
}
