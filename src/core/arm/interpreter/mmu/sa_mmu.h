/*
    sa_mmu.h - StrongARM Memory Management Unit emulation.
    ARMulator extensions for SkyEye.
	<lyhost@263.net>

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

#ifndef _SA_MMU_H_
#define _SA_MMU_H_


/**
 *  The interface of read data from bus
 */
int bus_read(short size, int addr, uint32_t * value);

/**
 * The interface of write data from bus
 */
int bus_write(short size, int addr, uint32_t value);


typedef struct sa_mmu_s
{
	tlb_s i_tlb;
	cache_s i_cache;

	tlb_s d_tlb;
	cache_s main_d_cache;
	cache_s mini_d_cache;
	rb_s rb_t;
	wb_s wb_t;
} sa_mmu_t;

#define I_TLB() (&state->mmu.u.sa_mmu.i_tlb)
#define I_CACHE() (&state->mmu.u.sa_mmu.i_cache)

#define D_TLB() (&state->mmu.u.sa_mmu.d_tlb)
#define MAIN_D_CACHE() (&state->mmu.u.sa_mmu.main_d_cache)
#define MINI_D_CACHE() (&state->mmu.u.sa_mmu.mini_d_cache)
#define WB() (&state->mmu.u.sa_mmu.wb_t)
#define RB() (&state->mmu.u.sa_mmu.rb_t)

extern mmu_ops_t sa_mmu_ops;
#endif /*_SA_MMU_H_*/
