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

// Note that for the 3DS, a Thumb instruction will only ever be
// two bytes in size. Thus we don't need to worry about ThumbEE
// or Thumb-2 where instructions can be 4 bytes in length.
static inline u32 GET_INST_SIZE(ARMul_State* core) {
    return core->TFlag? 2 : 4;
}

/**
 * Checks if the PC is being read, and if so, word-aligns it.
 * Used with address calculations.
 *
 * @param core The ARM CPU state instance.
 * @param Rn   The register being read.
 *
 * @return If the PC is being read, then the word-aligned PC value is returned.
 *         If the PC is not being read, then the value stored in the register is returned.
 */
static inline u32 CHECK_READ_REG15_WA(ARMul_State* core, int Rn) {
    return (Rn == 15) ? ((core->Reg[15] & ~0x3) + GET_INST_SIZE(core) * 2) : core->Reg[Rn];
}

/**
 * Reads the PC. Used for data processing operations that use the PC.
 *
 * @param core The ARM CPU state instance.
 * @param Rn   The register being read.
 *
 * @return If the PC is being read, then the incremented PC value is returned.
 *         If the PC is not being read, then the values stored in the register is returned.
 */
static inline u32 CHECK_READ_REG15(ARMul_State* core, int Rn) {
    return (Rn == 15) ? ((core->Reg[15] & ~0x1) + GET_INST_SIZE(core) * 2) : core->Reg[Rn];
}
