/*
    arm1176JZF-S_mmu.h - ARM1176JZF-S Memory Management Unit emulation.

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

#ifndef _ARM1176JZF_S_MMU_H_
#define _ARM1176JZF_S_MMU_H_

#if 0
typedef struct arm1176jzf-s_mmu_s
{
    tlb_t i_tlb;
    cache_t i_cache;

    tlb_t d_tlb;
    cache_t d_cache;
    wb_t wb_t;
} arm1176jzf-s_mmu_t;
#endif
extern mmu_ops_t arm1176jzf_s_mmu_ops;

ARMword
arm1176jzf_s_mmu_mrc (ARMul_State *state, ARMword instr, ARMword *value);
#endif /*_ARM1176JZF_S_MMU_H_*/
