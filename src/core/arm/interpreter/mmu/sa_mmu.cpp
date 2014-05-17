/*
    armmmu.c - Memory Management Unit emulation.
    ARMulator extensions for the ARM7100 family.
    Copyright (C) 1999  Ben Williamson

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

#include <assert.h>
#include <string.h>

#include "core/arm/interpreter/armdefs.h"

/**
 *  The interface of read data from bus
 */
int bus_read(short size, int addr, uint32_t * value) {
    ERROR_LOG(ARM11, "unimplemented bus_read");
    return 0;
}

/**
 * The interface of write data from bus
 */
int bus_write(short size, int addr, uint32_t value) {
    ERROR_LOG(ARM11, "unimplemented bus_write");
    return 0;
}


typedef struct sa_mmu_desc_s
{
	int i_tlb;
	cache_desc_t i_cache;

	int d_tlb;
	cache_desc_t main_d_cache;
	cache_desc_t mini_d_cache;
	int rb;
	wb_desc_t wb;
} sa_mmu_desc_t;

static sa_mmu_desc_t sa11xx_mmu_desc = {
	32,
	{32, 32, 16, CACHE_WRITE_BACK},

	32,
	{32, 32, 8, CACHE_WRITE_BACK},
	{32, 2, 8, CACHE_WRITE_BACK},
	4,
	//{8, 4},  for word size 
	{8, 16},		//for byte size,   chy 2003-07-11
};

static fault_t sa_mmu_write (ARMul_State * state, ARMword va, ARMword data,
			     ARMword datatype);
static fault_t sa_mmu_read (ARMul_State * state, ARMword va, ARMword * data,
			    ARMword datatype);
static fault_t update_cache (ARMul_State * state, ARMword va, ARMword data,
			     ARMword datatype, cache_line_t * cache,
			     cache_s * cache_t, ARMword real_va);

void
mmu_wb_write_bytes (ARMul_State * state, wb_s * wb_t, ARMword pa,
		    ARMbyte * data, int n);
int
sa_mmu_init (ARMul_State * state)
{
	sa_mmu_desc_t *desc;
	cache_desc_t *c_desc;

	state->mmu.control = 0x70;
	state->mmu.translation_table_base = 0xDEADC0DE;
	state->mmu.domain_access_control = 0xDEADC0DE;
	state->mmu.fault_status = 0;
	state->mmu.fault_address = 0;
	state->mmu.process_id = 0;

	desc = &sa11xx_mmu_desc;
	if (mmu_tlb_init (I_TLB (), desc->i_tlb)) {
		ERROR_LOG(ARM11, "i_tlb init %d\n", -1);
		goto i_tlb_init_error;
	}

	c_desc = &desc->i_cache;
	if (mmu_cache_init (I_CACHE (), c_desc->width, c_desc->way,
			    c_desc->set, c_desc->w_mode)) {
		ERROR_LOG(ARM11, "i_cache init %d\n", -1);
		goto i_cache_init_error;
	}

	if (mmu_tlb_init (D_TLB (), desc->d_tlb)) {
		ERROR_LOG(ARM11, "d_tlb init %d\n", -1);
		goto d_tlb_init_error;
	}

	c_desc = &desc->main_d_cache;
	if (mmu_cache_init (MAIN_D_CACHE (), c_desc->width, c_desc->way,
			    c_desc->set, c_desc->w_mode)) {
		ERROR_LOG(ARM11, "main_d_cache init %d\n", -1);
		goto main_d_cache_init_error;
	}

	c_desc = &desc->mini_d_cache;
	if (mmu_cache_init (MINI_D_CACHE (), c_desc->width, c_desc->way,
			    c_desc->set, c_desc->w_mode)) {
		ERROR_LOG(ARM11, "mini_d_cache init %d\n", -1);
		goto mini_d_cache_init_error;
	}

	if (mmu_wb_init (WB (), desc->wb.num, desc->wb.nb)) {
		ERROR_LOG(ARM11, "wb init %d\n", -1);
		goto wb_init_error;
	}

	if (mmu_rb_init (RB (), desc->rb)) {
		ERROR_LOG(ARM11, "rb init %d\n", -1);
		goto rb_init_error;
	}
	return 0;

      rb_init_error:
	mmu_wb_exit (WB ());
      wb_init_error:
	mmu_cache_exit (MINI_D_CACHE ());
      mini_d_cache_init_error:
	mmu_cache_exit (MAIN_D_CACHE ());
      main_d_cache_init_error:
	mmu_tlb_exit (D_TLB ());
      d_tlb_init_error:
	mmu_cache_exit (I_CACHE ());
      i_cache_init_error:
	mmu_tlb_exit (I_TLB ());
      i_tlb_init_error:
	return -1;
}

void
sa_mmu_exit (ARMul_State * state)
{
	mmu_rb_exit (RB ());
	mmu_wb_exit (WB ());
	mmu_cache_exit (MINI_D_CACHE ());
	mmu_cache_exit (MAIN_D_CACHE ());
	mmu_tlb_exit (D_TLB ());
	mmu_cache_exit (I_CACHE ());
	mmu_tlb_exit (I_TLB ());
};


static fault_t
sa_mmu_load_instr (ARMul_State * state, ARMword va, ARMword * instr)
{
	fault_t fault;
	tlb_entry_t *tlb;
	cache_line_t *cache;
	int c;			//cache bit
	ARMword pa;		//physical addr

	static int debug_count = 0;	//used for debug

	DEBUG_LOG(ARM11, "va = %x\n", va);

	va = mmu_pid_va_map (va);
	if (MMU_Enabled) {
		/*align check */
		if ((va & (WORD_SIZE - 1)) && MMU_Aligned) {
			DEBUG_LOG(ARM11, "align\n");
			return ALIGNMENT_FAULT;
		}
		else
			va &= ~(WORD_SIZE - 1);

		/*translate tlb */
		fault = translate (state, va, I_TLB (), &tlb);
		if (fault) {
			DEBUG_LOG(ARM11, "translate\n");
			return fault;
		}

		/*check access */
		fault = check_access (state, va, tlb, 1);
		if (fault) {
			DEBUG_LOG(ARM11, "check_fault\n");
			return fault;
		}
	}

	/*search cache no matter MMU enabled/disabled */
	cache = mmu_cache_search (state, I_CACHE (), va);
	if (cache) {
		*instr = cache->data[va_cache_index (va, I_CACHE ())];
		return NO_FAULT;
	}

	/*if MMU disabled or C flag is set alloc cache */
	if (MMU_Disabled) {
		c = 1;
		pa = va;
	}
	else {
		c = tlb_c_flag (tlb);
		pa = tlb_va_to_pa (tlb, va);
	}

	if (c) {
		int index;

		debug_count++;
		cache = mmu_cache_alloc (state, I_CACHE (), va, pa);
		index = va_cache_index (va, I_CACHE ());
		*instr = cache->data[va_cache_index (va, I_CACHE ())];
	}
	else
		//*instr = mem_read_word (state, pa);
		bus_read(32, pa, instr);

	return NO_FAULT;
};



static fault_t
sa_mmu_read_byte (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	//ARMword temp,offset;
	fault_t fault;
	fault = sa_mmu_read (state, virt_addr, data, ARM_BYTE_TYPE);
	return fault;
}

static fault_t
sa_mmu_read_halfword (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	//ARMword temp,offset;
	fault_t fault;
	fault = sa_mmu_read (state, virt_addr, data, ARM_HALFWORD_TYPE);
	return fault;
}

static fault_t
sa_mmu_read_word (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	return sa_mmu_read (state, virt_addr, data, ARM_WORD_TYPE);
}




static fault_t
sa_mmu_read (ARMul_State * state, ARMword va, ARMword * data,
	     ARMword datatype)
{
	fault_t fault;
	rb_entry_t *rb;
	tlb_entry_t *tlb;
	cache_line_t *cache;
	ARMword pa, real_va, temp, offset;

	DEBUG_LOG(ARM11, "va = %x\n", va);

	va = mmu_pid_va_map (va);
	real_va = va;
	/*if MMU disabled, memory_read */
	if (MMU_Disabled) {
		//*data = mem_read_word(state, va);
		if (datatype == ARM_BYTE_TYPE)
			//*data = mem_read_byte (state, va);
			bus_read(8, va, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			//*data = mem_read_halfword (state, va);
			bus_read(16, va, data);
		else if (datatype == ARM_WORD_TYPE)
			//*data = mem_read_word (state, va);
			bus_read(32, va, data);
		else {
			printf ("SKYEYE:1 sa_mmu_read error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}

		return NO_FAULT;
	}

	/*align check */
	if (((va & 3) && (datatype == ARM_WORD_TYPE) && MMU_Aligned) ||
	    ((va & 1) && (datatype == ARM_HALFWORD_TYPE) && MMU_Aligned)) {
		DEBUG_LOG(ARM11, "align\n");
		return ALIGNMENT_FAULT;
	}			// else

	va &= ~(WORD_SIZE - 1);

	/*translate va to tlb */
	fault = translate (state, va, D_TLB (), &tlb);
	if (fault) {
		DEBUG_LOG(ARM11, "translate\n");
		return fault;
	}
	/*check access permission */
	fault = check_access (state, va, tlb, 1);
	if (fault)
		return fault;
	/*search in read buffer */
	rb = mmu_rb_search (RB (), va);
	if (rb) {
		if (rb->fault)
			return rb->fault;
		*data = rb->data[(va & (rb_masks[rb->type] - 1)) >> WORD_SHT];
		goto datatrans;
		//return 0;
	};
	/*search main cache */
	cache = mmu_cache_search (state, MAIN_D_CACHE (), va);
	if (cache) {
		*data = cache->data[va_cache_index (va, MAIN_D_CACHE ())];
		goto datatrans;
		//return 0;
	}
	/*search mini cache */
	cache = mmu_cache_search (state, MINI_D_CACHE (), va);
	if (cache) {
		*data = cache->data[va_cache_index (va, MINI_D_CACHE ())];
		goto datatrans;
		//return 0;
	}

	/*get phy_addr */
	pa = tlb_va_to_pa (tlb, va);
	if ((pa >= 0xe0000000) && (pa < 0xe8000000)) {
		if (tlb_c_flag (tlb)) {
			if (tlb_b_flag (tlb)) {
				mmu_cache_soft_flush (state, MAIN_D_CACHE (),
						      pa);
			}
			else {
				mmu_cache_soft_flush (state, MINI_D_CACHE (),
						      pa);
			}
		}
		return NO_FAULT;
	}

	/*if Buffer, drain Write Buffer first */
	if (tlb_b_flag (tlb))
		mmu_wb_drain_all (state, WB ());

	/*alloc cache or mem_read */
	if (tlb_c_flag (tlb) && MMU_DCacheEnabled) {
		cache_s *cache_t;

		if (tlb_b_flag (tlb))
			cache_t = MAIN_D_CACHE ();
		else
			cache_t = MINI_D_CACHE ();
		cache = mmu_cache_alloc (state, cache_t, va, pa);
		*data = cache->data[va_cache_index (va, cache_t)];
	}
	else {
		//*data = mem_read_word(state, pa);
		if (datatype == ARM_BYTE_TYPE)
			//*data = mem_read_byte (state, pa | (real_va & 3));
			bus_read(8, pa | (real_va & 3), data);
		else if (datatype == ARM_HALFWORD_TYPE)
			//*data = mem_read_halfword (state, pa | (real_va & 2));
			bus_read(16, pa | (real_va & 2), data);
		else if (datatype == ARM_WORD_TYPE)
			//*data = mem_read_word (state, pa);
			bus_read(32, pa, data);
		else {
			printf ("SKYEYE:2 sa_mmu_read error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}
		return NO_FAULT;
	}


      datatrans:
	if (datatype == ARM_HALFWORD_TYPE) {
		temp = *data;
		offset = (((ARMword) state->bigendSig * 2) ^ (real_va & 2)) << 3;	/* bit offset into the word */
		*data = (temp >> offset) & 0xffff;
	}
	else if (datatype == ARM_BYTE_TYPE) {
		temp = *data;
		offset = (((ARMword) state->bigendSig * 3) ^ (real_va & 3)) << 3;	/* bit offset into the word */
		*data = (temp >> offset & 0xffL);
	}
      end:
	return NO_FAULT;
}


static fault_t
sa_mmu_write_byte (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	return sa_mmu_write (state, virt_addr, data, ARM_BYTE_TYPE);
}

static fault_t
sa_mmu_write_halfword (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	return sa_mmu_write (state, virt_addr, data, ARM_HALFWORD_TYPE);
}

static fault_t
sa_mmu_write_word (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	return sa_mmu_write (state, virt_addr, data, ARM_WORD_TYPE);
}



static fault_t
sa_mmu_write (ARMul_State * state, ARMword va, ARMword data, ARMword datatype)
{
	tlb_entry_t *tlb;
	cache_line_t *cache;
	int b;
	ARMword pa, real_va;
	fault_t fault;

	DEBUG_LOG(ARM11, "va = %x, val = %x\n", va, data);
	va = mmu_pid_va_map (va);
	real_va = va;

	/*search instruction cache */
	cache = mmu_cache_search (state, I_CACHE (), va);
	if (cache) {
		update_cache (state, va, data, datatype, cache, I_CACHE (),
			      real_va);
	}

	if (MMU_Disabled) {
		//mem_write_word(state, va, data);
		if (datatype == ARM_BYTE_TYPE)
			//mem_write_byte (state, va, data);
			bus_write(8, va, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			//mem_write_halfword (state, va, data);
			bus_write(16, va, data);
		else if (datatype == ARM_WORD_TYPE)
			//mem_write_word (state, va, data);
			bus_write(32, va, data);
		else {
			printf ("SKYEYE:1 sa_mmu_write error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}

		return NO_FAULT;
	}
	/*align check */
	//if ((va & (WORD_SIZE - 1)) && MMU_Aligned){
	if (((va & 3) && (datatype == ARM_WORD_TYPE) && MMU_Aligned) ||
	    ((va & 1) && (datatype == ARM_HALFWORD_TYPE) && MMU_Aligned)) {
		DEBUG_LOG(ARM11, "align\n");
		return ALIGNMENT_FAULT;
	}			//else
	va &= ~(WORD_SIZE - 1);
	/*tlb translate */
	fault = translate (state, va, D_TLB (), &tlb);
	if (fault) {
		DEBUG_LOG(ARM11, "translate\n");
		return fault;
	}
	/*tlb check access */
	fault = check_access (state, va, tlb, 0);
	if (fault) {
		DEBUG_LOG(ARM11, "check_access\n");
		return fault;
	}
	/*search main cache */
	cache = mmu_cache_search (state, MAIN_D_CACHE (), va);
	if (cache) {
		update_cache (state, va, data, datatype, cache,
			      MAIN_D_CACHE (), real_va);
	}
	else {
		/*search mini cache */
		cache = mmu_cache_search (state, MINI_D_CACHE (), va);
		if (cache) {
			update_cache (state, va, data, datatype, cache,
				      MINI_D_CACHE (), real_va);
		}
	}

	if (!cache) {
		b = tlb_b_flag (tlb);
		pa = tlb_va_to_pa (tlb, va);
		if (b) {
			if (MMU_WBEnabled) {
				if (datatype == ARM_WORD_TYPE)
					mmu_wb_write_bytes (state, WB (), pa,
							    (ARMbyte*)&data, 4);
				else if (datatype == ARM_HALFWORD_TYPE)
					mmu_wb_write_bytes (state, WB (),
							    (pa |
							     (real_va & 2)),
							    (ARMbyte*)&data, 2);
				else if (datatype == ARM_BYTE_TYPE)
					mmu_wb_write_bytes (state, WB (),
							    (pa |
							     (real_va & 3)),
							    (ARMbyte*)&data, 1);

			}
			else {
				if (datatype == ARM_WORD_TYPE)
					//mem_write_word (state, pa, data);
					bus_write(32, pa, data);
				else if (datatype == ARM_HALFWORD_TYPE)
					/*
					mem_write_halfword (state,
							    (pa |
							     (real_va & 2)),
							    data);
					*/
					bus_write(16, pa | (real_va & 2), data);
				else if (datatype == ARM_BYTE_TYPE)
					/*
					mem_write_byte (state,
							(pa | (real_va & 3)),
							data);
					*/
					bus_write(8, pa | (real_va & 3), data);
			}
		}
		else {
			mmu_wb_drain_all (state, WB ());

			if (datatype == ARM_WORD_TYPE)
				//mem_write_word (state, pa, data);
				bus_write(32, pa, data);
			else if (datatype == ARM_HALFWORD_TYPE)
				/*
					mem_write_halfword (state,
						    (pa | (real_va & 2)),
						    data);
				*/
				bus_write(16, pa | (real_va & 2), data);
			else if (datatype == ARM_BYTE_TYPE)
				/*
				mem_write_byte (state, (pa | (real_va & 3)),
						data);
				*/
				bus_write(8, pa | (real_va & 3), data);
		}
	}
	return NO_FAULT;
}

static fault_t
update_cache (ARMul_State * state, ARMword va, ARMword data, ARMword datatype,
	      cache_line_t * cache, cache_s * cache_t, ARMword real_va)
{
	ARMword temp, offset;

	ARMword index = va_cache_index (va, cache_t);

	//cache->data[index] = data;

	if (datatype == ARM_WORD_TYPE)
		cache->data[index] = data;
	else if (datatype == ARM_HALFWORD_TYPE) {
		temp = cache->data[index];
		offset = (((ARMword) state->bigendSig * 2) ^ (real_va & 2)) << 3;	/* bit offset into the word */
		cache->data[index] =
			(temp & ~(0xffffL << offset)) | ((data & 0xffffL) <<
							 offset);
	}
	else if (datatype == ARM_BYTE_TYPE) {
		temp = cache->data[index];
		offset = (((ARMword) state->bigendSig * 3) ^ (real_va & 3)) << 3;	/* bit offset into the word */
		cache->data[index] =
			(temp & ~(0xffL << offset)) | ((data & 0xffL) <<
						       offset);
	}

	if (index < (cache_t->width >> (WORD_SHT + 1)))
		cache->tag |= TAG_FIRST_HALF_DIRTY;
	else
		cache->tag |= TAG_LAST_HALF_DIRTY;

	return NO_FAULT;
}

ARMword
sa_mmu_mrc (ARMul_State * state, ARMword instr, ARMword * value)
{
	mmu_regnum_t creg = (mmu_regnum_t)(BITS (16, 19) & 15);
	ARMword data;

	switch (creg) {
	case MMU_ID:
//              printf("mmu_mrc read ID     ");
		data = 0x41007100;	/* v3 */
		data = state->cpu->cpu_val;
		break;
	case MMU_CONTROL:
//              printf("mmu_mrc read CONTROL");
		data = state->mmu.control;
		break;
	case MMU_TRANSLATION_TABLE_BASE:
//              printf("mmu_mrc read TTB    ");
		data = state->mmu.translation_table_base;
		break;
	case MMU_DOMAIN_ACCESS_CONTROL:
//              printf("mmu_mrc read DACR   ");
		data = state->mmu.domain_access_control;
		break;
	case MMU_FAULT_STATUS:
//              printf("mmu_mrc read FSR    ");
		data = state->mmu.fault_status;
		break;
	case MMU_FAULT_ADDRESS:
//              printf("mmu_mrc read FAR    ");
		data = state->mmu.fault_address;
		break;
	case MMU_PID:
		data = state->mmu.process_id;
	default:
		printf ("mmu_mrc read UNKNOWN - reg %d\n", creg);
		data = 0;
		break;
	}
//      printf("\t\t\t\t\tpc = 0x%08x\n", state->Reg[15]);
	*value = data;
	return data;
}

void
sa_mmu_cache_ops (ARMul_State * state, ARMword instr, ARMword value)
{
	int CRm, OPC_2;

	CRm = BITS (0, 3);
	OPC_2 = BITS (5, 7);

	if (OPC_2 == 0 && CRm == 7) {
		mmu_cache_invalidate_all (state, I_CACHE ());
		mmu_cache_invalidate_all (state, MAIN_D_CACHE ());
		mmu_cache_invalidate_all (state, MINI_D_CACHE ());
		return;
	}

	if (OPC_2 == 0 && CRm == 5) {
		mmu_cache_invalidate_all (state, I_CACHE ());
		return;
	}

	if (OPC_2 == 0 && CRm == 6) {
		mmu_cache_invalidate_all (state, MAIN_D_CACHE ());
		mmu_cache_invalidate_all (state, MINI_D_CACHE ());
		return;
	}

	if (OPC_2 == 1 && CRm == 6) {
		mmu_cache_invalidate (state, MAIN_D_CACHE (), value);
		mmu_cache_invalidate (state, MINI_D_CACHE (), value);
		return;
	}

	if (OPC_2 == 1 && CRm == 0xa) {
		mmu_cache_clean (state, MAIN_D_CACHE (), value);
		mmu_cache_clean (state, MINI_D_CACHE (), value);
		return;
	}

	if (OPC_2 == 4 && CRm == 0xa) {
		mmu_wb_drain_all (state, WB ());
		return;
	}
	ERROR_LOG(ARM11, "Unknow OPC_2 = %x CRm = %x\n", OPC_2, CRm);
}

static void
sa_mmu_tlb_ops (ARMul_State * state, ARMword instr, ARMword value)
{
	int CRm, OPC_2;

	CRm = BITS (0, 3);
	OPC_2 = BITS (5, 7);


	if (OPC_2 == 0 && CRm == 0x7) {
		mmu_tlb_invalidate_all (state, I_TLB ());
		mmu_tlb_invalidate_all (state, D_TLB ());
		return;
	}

	if (OPC_2 == 0 && CRm == 0x5) {
		mmu_tlb_invalidate_all (state, I_TLB ());
		return;
	}

	if (OPC_2 == 0 && CRm == 0x6) {
		mmu_tlb_invalidate_all (state, D_TLB ());
		return;
	}

	if (OPC_2 == 1 && CRm == 0x6) {
		mmu_tlb_invalidate_entry (state, D_TLB (), value);
		return;
	}

	ERROR_LOG(ARM11, "Unknow OPC_2 = %x CRm = %x\n", OPC_2, CRm);
}

static void
sa_mmu_rb_ops (ARMul_State * state, ARMword instr, ARMword value)
{
	int CRm, OPC_2;

	CRm = BITS (0, 3);
	OPC_2 = BITS (5, 7);

	if (OPC_2 == 0x0 && CRm == 0x0) {
		mmu_rb_invalidate_all (RB ());
		return;
	}

	if (OPC_2 == 0x2) {
		int idx = CRm & 0x3;
		int type = ((CRm >> 2) & 0x3) + 1;

		if ((idx < 4) && (type < 4))
			mmu_rb_load (state, RB (), idx, type, value);
		return;
	}

	if ((OPC_2 == 1) && (CRm < 4)) {
		mmu_rb_invalidate_entry (RB (), CRm);
		return;
	}

	ERROR_LOG(ARM11, "Unknow OPC_2 = %x CRm = %x\n", OPC_2, CRm);
}

static ARMword
sa_mmu_mcr (ARMul_State * state, ARMword instr, ARMword value)
{
	mmu_regnum_t creg = (mmu_regnum_t)(BITS (16, 19) & 15);
	if (!strncmp (state->cpu->cpu_arch_name, "armv4", 5)) {
		switch (creg) {
		case MMU_CONTROL:
//              printf("mmu_mcr wrote CONTROL      ");
			state->mmu.control = (value | 0x70) & 0xFFFD;
			break;
		case MMU_TRANSLATION_TABLE_BASE:
//              printf("mmu_mcr wrote TTB          ");
			state->mmu.translation_table_base =
				value & 0xFFFFC000;
			break;
		case MMU_DOMAIN_ACCESS_CONTROL:
//              printf("mmu_mcr wrote DACR         ");
			state->mmu.domain_access_control = value;
			break;

		case MMU_FAULT_STATUS:
			state->mmu.fault_status = value & 0xFF;
			break;
		case MMU_FAULT_ADDRESS:
			state->mmu.fault_address = value;
			break;

		case MMU_CACHE_OPS:
			sa_mmu_cache_ops (state, instr, value);
			break;
		case MMU_TLB_OPS:
			sa_mmu_tlb_ops (state, instr, value);
			break;
		case MMU_SA_RB_OPS:
			sa_mmu_rb_ops (state, instr, value);
			break;
		case MMU_SA_DEBUG:
			break;
		case MMU_SA_CP15_R15:
			break;
		case MMU_PID:
			//2004-06-06 lyh, bug provided by wen ye wenye@cs.ucsb.edu
			state->mmu.process_id = value & 0x7e000000;
			break;

		default:
			printf ("mmu_mcr wrote UNKNOWN - reg %d\n", creg);
			break;
		}
	}
    return 0;
}

//teawater add for arm2x86 2005.06.24-------------------------------------------
static int
sa_mmu_v2p_dbct (ARMul_State * state, ARMword virt_addr, ARMword * phys_addr)
{
	fault_t fault;
	tlb_entry_t *tlb;

	virt_addr = mmu_pid_va_map (virt_addr);
	if (MMU_Enabled) {

		/*align check */
		if ((virt_addr & (WORD_SIZE - 1)) && MMU_Aligned) {
			DEBUG_LOG(ARM11, "align\n");
			return ALIGNMENT_FAULT;
		}
		else
			virt_addr &= ~(WORD_SIZE - 1);

		/*translate tlb */
		fault = translate (state, virt_addr, I_TLB (), &tlb);
		if (fault) {
			DEBUG_LOG(ARM11, "translate\n");
			return fault;
		}

		/*check access */
		fault = check_access (state, virt_addr, tlb, 1);
		if (fault) {
			DEBUG_LOG(ARM11, "check_fault\n");
			return fault;
		}
	}

	if (MMU_Disabled) {
		*phys_addr = virt_addr;
	}
	else {
		*phys_addr = tlb_va_to_pa (tlb, virt_addr);
	}

	return (0);
}

//AJ2D--------------------------------------------------------------------------

/*sa mmu_ops_t*/
mmu_ops_t sa_mmu_ops = {
	sa_mmu_init,
	sa_mmu_exit,
	sa_mmu_read_byte,
	sa_mmu_write_byte,
	sa_mmu_read_halfword,
	sa_mmu_write_halfword,
	sa_mmu_read_word,
	sa_mmu_write_word,
	sa_mmu_load_instr,
	sa_mmu_mcr,
	sa_mmu_mrc,
//teawater add for arm2x86 2005.06.24-------------------------------------------
	sa_mmu_v2p_dbct,
//AJ2D--------------------------------------------------------------------------
};
