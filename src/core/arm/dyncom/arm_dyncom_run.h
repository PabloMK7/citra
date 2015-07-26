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

#include "core/arm/skyeye_common/armstate.h"

/**
 * Checks if the PC is being read, and if so, word-aligns it.
 * Used with address calculations.
 *
 * @param cpu The ARM CPU state instance.
 * @param Rn   The register being read.
 *
 * @return If the PC is being read, then the word-aligned PC value is returned.
 *         If the PC is not being read, then the value stored in the register is returned.
 */
static inline u32 CHECK_READ_REG15_WA(ARMul_State* cpu, int Rn) {
    return (Rn == 15) ? ((cpu->Reg[15] & ~0x3) + cpu->GetInstructionSize() * 2) : cpu->Reg[Rn];
}

/**
 * Reads the PC. Used for data processing operations that use the PC.
 *
 * @param cpu The ARM CPU state instance.
 * @param Rn   The register being read.
 *
 * @return If the PC is being read, then the incremented PC value is returned.
 *         If the PC is not being read, then the values stored in the register is returned.
 */
static inline u32 CHECK_READ_REG15(ARMul_State* cpu, int Rn) {
    return (Rn == 15) ? ((cpu->Reg[15] & ~0x1) + cpu->GetInstructionSize() * 2) : cpu->Reg[Rn];
}
