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
#include "core/arm/interpreter/armemu.h"

/*#include "pxa.h" */

/* chy 2005-09-19 */

/* extern pxa270_io_t pxa270_io; */
/* chy 2005-09-19 -----end */

typedef struct xscale_mmu_desc_s
{
	int i_tlb;
	cache_desc_t i_cache;

	int d_tlb;
	cache_desc_t main_d_cache;
	cache_desc_t mini_d_cache;
	//int   rb;  xscale has no read buffer
	wb_desc_t wb;
} xscale_mmu_desc_t;

static xscale_mmu_desc_t pxa_mmu_desc = {
	32,
	{32, 32, 32, CACHE_WRITE_BACK},

	32,
	{32, 32, 32, CACHE_WRITE_BACK},
	{32, 2, 8, CACHE_WRITE_BACK},
	{8, 16},		//for byte size, 
};

//chy 2005-09-19 for cp6
#define CR0_ICIP   0
#define CR1_ICMR   1
//chy 2005-09-19 ---end
//----------- for cp14-----------------
#define  CCLKCFG   6
#define  PWRMODE   7
typedef struct xscale_cp14_reg_s
{
	unsigned cclkcfg;	//reg6
	unsigned pwrmode;	//reg7
} xscale_cp14_reg_s;

xscale_cp14_reg_s pxa_cp14_regs;

//--------------------------------------

static fault_t xscale_mmu_write (ARMul_State * state, ARMword va,
				 ARMword data, ARMword datatype);
static fault_t xscale_mmu_read (ARMul_State * state, ARMword va,
				ARMword * data, ARMword datatype);

ARMword xscale_mmu_mrc (ARMul_State * state, ARMword instr, ARMword * value);
ARMword xscale_mmu_mcr (ARMul_State * state, ARMword instr, ARMword value);


/* jeff add 2010.9.26 for pxa270 cp6*/
#define PXA270_ICMR 0x40D00004
#define PXA270_ICPR 0x40D00010
#define PXA270_ICLR 0x40D00008
//chy 2005-09-19 for xscale pxa27x cp6
unsigned
xscale_cp6_mrc (ARMul_State * state, unsigned type, ARMword instr,
		ARMword * data)
{
	unsigned opcode_2 = BITS (5, 7);
	unsigned CRm = BITS (0, 3);
	unsigned reg = BITS (16, 19);
	unsigned result;

	//printf("SKYEYE: xscale_cp6_mrc:opcode_2 0x%x, CRm 0x%x, reg 0x%x,reg[15] 0x%x, instr %x\n",opcode_2,CRm,reg,state->Reg[15], instr);

	switch (reg) {
	case CR0_ICIP: {		// cp 6 reg 0
		//printf("cp6_mrc cr0 ICIP              \n");
		/* *data = (pxa270_io.icmr & pxa270_io.icpr) & ~pxa270_io.iclr; */
		/* use bus_read get the pxa270 machine registers  2010.9.26 jeff*/
		int icmr, icpr, iclr;
		bus_read(32, PXA270_ICMR, (uint32_t*)&icmr);
		bus_read(32, PXA270_ICPR, (uint32_t*)&icpr);
		bus_read(32, PXA270_ICLR, (uint32_t*)&iclr);
		*data = (icmr & icpr)  & ~iclr;
		}
		break;
	case CR1_ICMR: {	// cp 6 reg 1
		//printf("cp6_mrc cr1 ICMR\n");
		/* *data = pxa270_io.icmr; */
		int icmr;
		/* use bus_read get the pxa270 machine registers  2010.9.26 jeff*/
		bus_read(32, PXA270_ICMR, (uint32_t*)&icmr);
		*data = icmr;
		}
		break;
	default:
		*data = 0;
		printf ("SKYEYE:cp6_mrc unknown cp6 regs!!!!!!\n");
		printf ("SKYEYE: xscale_cp6_mrc:opcode_2 0x%x, CRm 0x%x, reg 0x%x,reg[15] 0x%x, instr %x\n", opcode_2, CRm, reg, state->Reg[15], instr);
		break;
	}
	return 0;
}

//chy 2005-09-19 end
//xscale cp13 ----------------------------------------------------
unsigned
xscale_cp13_init (ARMul_State * state)
{
	//printf("SKYEYE: xscale_cp13_init: begin\n");
	return 0;
}

unsigned
xscale_cp13_exit (ARMul_State * state)
{
	//printf("SKYEYE: xscale_cp13_exit: begin\n");
	return 0;
}

unsigned
xscale_cp13_ldc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword data)
{
	printf ("SKYEYE: xscale_cp13_ldc: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp13_stc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword * data)
{
	printf ("SKYEYE: xscale_cp13_stc: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp13_mrc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword * data)
{
	printf ("SKYEYE: xscale_cp13_mrc: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp13_mcr (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword data)
{
	printf ("SKYEYE: xscale_cp13_mcr: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp13_cdp (ARMul_State * state, unsigned type, ARMword instr)
{
	printf ("SKYEYE: xscale_cp13_cdp: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp13_read_reg (ARMul_State * state, unsigned reg, ARMword * data)
{
	printf ("SKYEYE: xscale_cp13_read_reg: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	return 0;
	//exit(-1);
}

unsigned
xscale_cp13_write_reg (ARMul_State * state, unsigned reg, ARMword data)
{
	printf ("SKYEYE: xscale_cp13_write_reg: ERROR isn't existed,");
	SKYEYE_OUTREGS (stderr);
	fprintf (stderr, "\n");
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

//------------------------------------------------------------------
//xscale cp14 ----------------------------------------------------
unsigned
xscale_cp14_init (ARMul_State * state)
{
	//printf("SKYEYE: xscale_cp14_init: begin\n");
	pxa_cp14_regs.cclkcfg = 0;
	pxa_cp14_regs.pwrmode = 0;
	return 0;
}

unsigned
xscale_cp14_exit (ARMul_State * state)
{
	//printf("SKYEYE: xscale_cp14_exit: begin\n");
	return 0;
}

unsigned
xscale_cp14_ldc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword data)
{
	printf ("SKYEYE: xscale_cp14_ldc: ERROR isn't existed, reg15 0x%x\n",
		state->Reg[15]);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp14_stc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword * data)
{
	printf ("SKYEYE: xscale_cp14_stc: ERROR isn't existed, reg15 0x%x\n",
		state->Reg[15]);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}

unsigned
xscale_cp14_mrc (ARMul_State * state, unsigned type, ARMword instr,
		 ARMword * data)
{
	unsigned opcode_2 = BITS (5, 7);
	unsigned CRm = BITS (0, 3);
	unsigned reg = BITS (16, 19);
	unsigned result;

	//printf("SKYEYE: xscale_cp14_mrc:opcode_2 0x%x, CRm 0x%x, reg 0x%x,reg[15] 0x%x, instr %x\n",opcode_2,CRm,reg,\
	state->Reg[15], instr);

	switch (reg) {
	case CCLKCFG:		// cp 14 reg 6
		//printf("cp14_mrc cclkcfg              \n");
		*data = pxa_cp14_regs.cclkcfg;
		break;
	case PWRMODE:		// cp 14 reg 7
		//printf("cp14_mrc pwrmode              \n");
		*data = pxa_cp14_regs.pwrmode;
		break;
	default:
		*data = 0;
		printf ("SKYEYE:cp14_mrc unknown cp14 regs!!!!!!\n");
		break;
	}
	return 0;
}
unsigned xscale_cp14_mcr (ARMul_State * state, unsigned type, ARMword instr,
			  ARMword data)
{
	unsigned opcode_2 = BITS (5, 7);
	unsigned CRm = BITS (0, 3);
	unsigned reg = BITS (16, 19);
	unsigned result;

	//printf("SKYEYE: xscale_cp14_mcr:opcode_2 0x%x, CRm 0x%x, reg 0x%x,reg[15] 0x%x, instr %x\n",opcode_2,CRm,reg,\
	  state->Reg[15], instr);

	switch (reg) {
	case CCLKCFG:		// cp 14 reg 6
		//printf("cp14_mcr cclkcfg              \n");
		pxa_cp14_regs.cclkcfg = data & 0xf;
		break;
		case PWRMODE:	// cp 14 reg 7
			//printf("cp14_mcr pwrmode              \n");
		pxa_cp14_regs.pwrmode = data & 0x3;
		break;
		default:printf ("SKYEYE: cp14_mcr unknown cp14 regs!!!!!!\n");
		break;
	}
	return 0;
}
unsigned xscale_cp14_cdp (ARMul_State * state, unsigned type, ARMword instr)
{
	printf ("SKYEYE: xscale_cp14_cdp: ERROR isn't existed, reg15 0x%x\n",
		state->Reg[15]);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}
unsigned xscale_cp14_read_reg (ARMul_State * state, unsigned reg,
			       ARMword * data)
{
	printf ("SKYEYE: xscale_cp14_read_reg: ERROR isn't existed, reg15 0x%x\n", state->Reg[15]);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);
	return 0; //No matter return value, only for compiler.
}
unsigned xscale_cp14_write_reg (ARMul_State * state, unsigned reg,
				ARMword data)
{
	printf ("SKYEYE: xscale_cp14_write_reg: ERROR isn't existed, reg15 0x%x\n", state->Reg[15]);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);

	return 0; //No matter return value, only for compiler.
}

//------------------------------------------------------------------
//cp15 -------------------------------------
unsigned xscale_cp15_ldc (ARMul_State * state, unsigned type, ARMword instr,
			  ARMword data)
{
	printf ("SKYEYE: xscale_cp15_ldc: ERROR isn't existed\n");
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);

	return 0; //No matter return value, only for compiler.
}
unsigned xscale_cp15_stc (ARMul_State * state, unsigned type, ARMword instr,
			  ARMword * data)
{
	printf ("SKYEYE: xscale_cp15_stc: ERROR isn't existed\n");
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);

	return 0; //No matter return value, only for compiler.
}
unsigned xscale_cp15_cdp (ARMul_State * state, unsigned type, ARMword instr)
{
	printf ("SKYEYE: xscale_cp15_cdp: ERROR isn't existed\n");
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);

	return 0; //No matter return value, only for compiler.
}
unsigned xscale_cp15_read_reg (ARMul_State * state, unsigned reg,
			       ARMword * data)
{
//chy 2003-09-03: for xsacle_cp15_cp_access_allowed
	if (reg == 15) {
		*data = state->mmu.copro_access;
		//printf("SKYEYE: xscale_cp15_read_reg: reg 0x%x,data %x\n",reg,*data);
		return 0;
	}
	printf ("SKYEYE: xscale_cp15_read_reg: reg 0x%x, ERROR isn't existed\n", reg);
	SKYEYE_OUTREGS (stderr);
	// skyeye_exit (-1);

	return 0; //No matter return value, only for compiler.
}

//chy 2003-09-03 used by macro CP_ACCESS_ALLOWED in armemu.h
unsigned xscale_cp15_cp_access_allowed (ARMul_State * state, unsigned reg,
					unsigned cpnum)
{
	unsigned data;

	xscale_cp15_read_reg (state, reg, &data);
	//printf("SKYEYE: cp15_cp_access_allowed data %x, cpnum %x, result %x\n", data, cpnum, (data & 1<<cpnum));
	if (data & 1 << cpnum)
		return 1;
	else
		return 0;
}

unsigned xscale_cp15_write_reg (ARMul_State * state, unsigned reg,
				ARMword value)
{
	switch (reg) {
	case MMU_FAULT_STATUS:
		//printf("SKYEYE:cp15_write_reg  wrote FS        val 0x%x \n",value);
		state->mmu.fault_status = value & 0x6FF;
		break;
	case MMU_FAULT_ADDRESS:
		//printf("SKYEYE:cp15_write_reg wrote FA         val 0x%x \n",value);
		state->mmu.fault_address = value;
		break;
	default:
		printf ("SKYEYE: xscale_cp15_write_reg: reg 0x%x R15 %x ERROR isn't existed\n", reg, state->Reg[15]);
		SKYEYE_OUTREGS (stderr);
		// skyeye_exit (-1);
	}
	return 0;
}

unsigned
xscale_cp15_init (ARMul_State * state)
{
	xscale_mmu_desc_t *desc;
	cache_desc_t *c_desc;

	state->mmu.control = 0;
	state->mmu.translation_table_base = 0xDEADC0DE;
	state->mmu.domain_access_control = 0xDEADC0DE;
	state->mmu.fault_status = 0;
	state->mmu.fault_address = 0;
	state->mmu.process_id = 0;
	state->mmu.cache_type = 0xB1AA1AA;	//0000 1011 0001 1010 1010 0001 1010 1010
	state->mmu.aux_control = 0;

	desc = &pxa_mmu_desc;

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
#if 0
	if (mmu_rb_init (RB (), desc->rb)) {
		ERROR_LOG(ARM11, "rb init %d\n", -1);
		goto rb_init_error;
	}
#endif

	return 0;
#if 0
      rb_init_error:
	mmu_wb_exit (WB ());
#endif
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

unsigned
xscale_cp15_exit (ARMul_State * state)
{
	//mmu_rb_exit(RB());
	mmu_wb_exit (WB ());
	mmu_cache_exit (MINI_D_CACHE ());
	mmu_cache_exit (MAIN_D_CACHE ());
	mmu_tlb_exit (D_TLB ());
	mmu_cache_exit (I_CACHE ());
	mmu_tlb_exit (I_TLB ());
	return 0;
};


static fault_t
	xscale_mmu_load_instr (ARMul_State * state, ARMword va,
			       ARMword * instr)
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
		if ((va & (INSN_SIZE - 1)) && MMU_Aligned) {
			DEBUG_LOG(ARM11, "align\n");
			return ALIGNMENT_FAULT;
		}
		else
			va &= ~(INSN_SIZE - 1);

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
	//chy 2003-09-02 for test, don't use cache  ?????       
#if 0
	/*search cache no matter MMU enabled/disabled */
	cache = mmu_cache_search (state, I_CACHE (), va);
	if (cache) {
		*instr = cache->data[va_cache_index (va, I_CACHE ())];
		return 0;
	}
#endif
	/*if MMU disabled or C flag is set alloc cache */
	if (MMU_Disabled) {
		c = 1;
		pa = va;
	}
	else {
		c = tlb_c_flag (tlb);
		pa = tlb_va_to_pa (tlb, va);
	}

	//chy 2003-09-03 only read mem, don't use cache now,will change later ????
	//*instr = mem_read_word (state, pa);
	bus_read(32, pa, instr);
#if 0
//-----------------------------------------------------------
	//chy 2003-09-02 for test????
	if (pa >= 0xa01c8000 && pa <= 0xa01c8020) {
		printf ("SKYEYE:load_instr: pa %x, va %x,instr %x, R15 %x\n",
			pa, va, *instr, state->Reg[15]);
	}

//----------------------------------------------------------------------        
#endif
	return NO_FAULT;

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
	xscale_mmu_read_byte (ARMul_State * state, ARMword virt_addr,
			      ARMword * data)
{
	//ARMword temp,offset;
	fault_t fault;
	fault = xscale_mmu_read (state, virt_addr, data, ARM_BYTE_TYPE);
	return fault;
}

static fault_t
	xscale_mmu_read_halfword (ARMul_State * state, ARMword virt_addr,
				  ARMword * data)
{
	//ARMword temp,offset;
	fault_t fault;
	fault = xscale_mmu_read (state, virt_addr, data, ARM_HALFWORD_TYPE);
	return fault;
}

static fault_t
	xscale_mmu_read_word (ARMul_State * state, ARMword virt_addr,
			      ARMword * data)
{
	return xscale_mmu_read (state, virt_addr, data, ARM_WORD_TYPE);
}




static fault_t
	xscale_mmu_read (ARMul_State * state, ARMword va, ARMword * data,
			 ARMword datatype)
{
	fault_t fault;
//      rb_entry_t *rb;
	tlb_entry_t *tlb;
	cache_line_t *cache;
	ARMword pa, real_va, temp, offset;
	//chy 2003-09-02 for test ????
	static unsigned chyst1 = 0, chyst2 = 0;

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
			printf ("SKYEYE:1 xscale_mmu_read error: unknown data type %d\n", datatype);
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

#if 0
//------------------------------------------------
//chy 2003-09-02 for test only ,should commit ????
	if (datatype == ARM_WORD_TYPE) {
		if (real_va >= 0xffff0000 && real_va <= 0xffff0020) {
			pa = tlb_va_to_pa (tlb, va);
			*data = mem_read_word (state, pa);
			chyst1++;
			printf ("**SKYEYE:mmu_read word %d: pa %x, va %x, data %x, R15 %x\n", chyst1, pa, real_va, *data, state->Reg[15]);
			/*
			   cache==mmu_cache_search(state,MAIN_D_CACHE(),va);
			   if(cache){
			   *data = cache->data[va_cache_index(va, MAIN_D_CACHE())];
			   printf("cached data %x\n",*data);
			   }else   printf("no cached data\n");
			 */
		}
	}
//-------------------------------------------------
#endif
#if 0
	/*search in read buffer */
	rb = mmu_rb_search (RB (), va);
	if (rb) {
		if (rb->fault)
			return rb->fault;
		*data = rb->data[(va & (rb_masks[rb->type] - 1)) >> WORD_SHT];
		goto datatrans;
		//return 0;
	};
#endif

	/*2004-07-19 chy: add support of xscale MMU CacheDisabled option */
	if (MMU_CacheDisabled) {
		//if(1){ can be used to test cache error
		/*get phy_addr */
		pa = tlb_va_to_pa (tlb, real_va);
		if (datatype == ARM_BYTE_TYPE)
			//*data = mem_read_byte (state, pa);
			bus_read(8, pa, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			//*data = mem_read_halfword (state, pa);
			bus_read(16, pa, data);
		else if (datatype == ARM_WORD_TYPE)
			//*data = mem_read_word (state, pa);
			bus_read(32, pa, data);
		else {
			printf ("SKYEYE:MMU_CacheDisabled xscale_mmu_read error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}
		return NO_FAULT;
	}


	/*search main cache */
	cache = mmu_cache_search (state, MAIN_D_CACHE (), va);
	if (cache) {
		*data = cache->data[va_cache_index (va, MAIN_D_CACHE ())];
#if 0
//------------------------------------------------------------------------
//chy 2003-09-02 for test only ,should commit ????
		if (real_va >= 0xffff0000 && real_va <= 0xffff0020) {
			pa = tlb_va_to_pa (tlb, va);
			chyst2++;
			printf ("**SKYEYE:mmu_read wordk:cache %d: pa %x, va %x, data %x, R15 %x\n", chyst2, pa, real_va, *data, state->Reg[15]);
		}
//-------------------------------------------------------------------
#endif
		goto datatrans;
		//return 0;
	}
	//chy 2003-08-24, now maybe we don't need minidcache  ????
#if 0
	/*search mini cache */
	cache = mmu_cache_search (state, MINI_D_CACHE (), va);
	if (cache) {
		*data = cache->data[va_cache_index (va, MINI_D_CACHE ())];
		goto datatrans;
		//return 0;
	}
#endif
	/*get phy_addr */
	pa = tlb_va_to_pa (tlb, va);
	//chy 2003-08-24 , in xscale it means what ?????
#if 0
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
		return 0;
	}
#endif
	//chy 2003-08-24, check phy addr
	//ywc 2004-11-30, inactive this check because of using 0xc0000000 as the framebuffer start address
	/*
	   if(pa >= 0xb0000000){
	   printf("SKYEYE:xscale_mmu_read: phy address 0x%x error,reg[15] 0x%x\n",pa,state->Reg[15]);
	   return 0;
	   }
	 */

	//chy 2003-08-24, now maybe we don't need wb  ????
#if 0
	/*if Buffer, drain Write Buffer first */
	if (tlb_b_flag (tlb))
		mmu_wb_drain_all (state, WB ());
#endif
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
			printf ("SKYEYE:2 xscale_mmu_read error: unknown data type %d\n", datatype);
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
	xscale_mmu_write_byte (ARMul_State * state, ARMword virt_addr,
			       ARMword data)
{
	return xscale_mmu_write (state, virt_addr, data, ARM_BYTE_TYPE);
}

static fault_t
	xscale_mmu_write_halfword (ARMul_State * state, ARMword virt_addr,
				   ARMword data)
{
	return xscale_mmu_write (state, virt_addr, data, ARM_HALFWORD_TYPE);
}

static fault_t
	xscale_mmu_write_word (ARMul_State * state, ARMword virt_addr,
			       ARMword data)
{
	return xscale_mmu_write (state, virt_addr, data, ARM_WORD_TYPE);
}



static fault_t
	xscale_mmu_write (ARMul_State * state, ARMword va, ARMword data,
			  ARMword datatype)
{
	tlb_entry_t *tlb;
	cache_line_t *cache;
	cache_s *cache_t;
	int b;
	ARMword pa, real_va, temp, offset;
	fault_t fault;

	ARMword index;
//chy 2003-09-02 for test ????
//      static unsigned chyst1=0,chyst2=0;

	DEBUG_LOG(ARM11, "va = %x, val = %x\n", va, data);
	va = mmu_pid_va_map (va);
	real_va = va;

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
			printf ("SKYEYE:1 xscale_mmu_write error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}

		return NO_FAULT;
	}
	/*align check */
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

	/*2004-07-19 chy: add support for xscale MMU_CacheDisabled */
	if (MMU_CacheDisabled) {
		//if(1){ can be used to test the cache error
		/*get phy_addr */
		pa = tlb_va_to_pa (tlb, real_va);
		if (datatype == ARM_BYTE_TYPE)
			//mem_write_byte (state, pa, data);
			bus_write(8, pa, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			//mem_write_halfword (state, pa, data);
			bus_write(16, pa, data);
		else if (datatype == ARM_WORD_TYPE)
			//mem_write_word (state, pa, data);
			bus_write(32, pa , data);
		else {
			printf ("SKYEYE:MMU_CacheDisabled xscale_mmu_write error: unknown data type %d\n", datatype);
			// skyeye_exit (-1);
		}

		return NO_FAULT;
	}

	/*search main cache */
	b = tlb_b_flag (tlb);
	pa = tlb_va_to_pa (tlb, va);
	cache = mmu_cache_search (state, MAIN_D_CACHE (), va);
	if (cache) {
		cache_t = MAIN_D_CACHE ();
		goto has_cache;
	}
	//chy 2003-08-24, now maybe we don't need minidcache  ????
#if 0
	/*search mini cache */
	cache = mmu_cache_search (state, MINI_D_CACHE (), va);
	if (cache) {
		cache_t = MINI_D_CACHE ();
		goto has_cache;
	}
#endif
	b = tlb_b_flag (tlb);
	pa = tlb_va_to_pa (tlb, va);
	//chy 2003-08-24, check phy addr 0xa0000000, size 0x04000000
	//ywc 2004-11-30, inactive this check because of using 0xc0000000 as the framebuffer start address
	/*
	   if(pa >= 0xb0000000){
	   printf("SKYEYE:xscale_mmu_write phy address 0x%x error,reg[15] 0x%x\n",pa,state->Reg[15]);
	   return 0;
	   }
	 */

	//chy 2003-08-24, now maybe we don't need WB  ????
#if 0
	if (b) {
		if (MMU_WBEnabled) {
			if (datatype == ARM_WORD_TYPE)
				mmu_wb_write_bytes (state, WB (), pa, &data,
						    4);
			else if (datatype == ARM_HALFWORD_TYPE)
				mmu_wb_write_bytes (state, WB (),
						    (pa | (real_va & 2)),
						    &data, 2);
			else if (datatype == ARM_BYTE_TYPE)
				mmu_wb_write_bytes (state, WB (),
						    (pa | (real_va & 3)),
						    &data, 1);

		}
		else {
			if (datatype == ARM_WORD_TYPE)
				mem_write_word (state, pa, data);
			else if (datatype == ARM_HALFWORD_TYPE)
				mem_write_halfword (state,
						    (pa | (real_va & 2)),
						    data);
			else if (datatype == ARM_BYTE_TYPE)
				mem_write_byte (state, (pa | (real_va & 3)),
						data);
		}
	}
	else {

		mmu_wb_drain_all (state, WB ());

		if (datatype == ARM_WORD_TYPE)
			mem_write_word (state, pa, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			mem_write_halfword (state, (pa | (real_va & 2)),
					    data);
		else if (datatype == ARM_BYTE_TYPE)
			mem_write_byte (state, (pa | (real_va & 3)), data);
	}
#endif
	//chy 2003-08-24, just write phy addr
	if (datatype == ARM_WORD_TYPE)
		//mem_write_word (state, pa, data);
		bus_write(32, pa, data);
	else if (datatype == ARM_HALFWORD_TYPE)
		//mem_write_halfword (state, (pa | (real_va & 2)), data);
		bus_write(16, pa | (real_va & 2), data);
	else if (datatype == ARM_BYTE_TYPE)
		//mem_write_byte (state, (pa | (real_va & 3)), data);
		bus_write(8, (pa | (real_va & 3)), data);
#if 0
//-------------------------------------------------------------
//chy 2003-09-02 for test ????
	if (datatype == ARM_WORD_TYPE) {
		if (real_va >= 0xffff0000 && real_va <= 0xffff0020) {
			printf ("**SKYEYE:mmu_write word: pa %x, va %x, data %x, R15 %x \n", pa, real_va, data, state->Reg[15]);
		}
	}
//--------------------------------------------------------------
#endif
	return NO_FAULT;

      has_cache:
	index = va_cache_index (va, cache_t);
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
//-------------------------------------------------------------
//chy 2003-09-03 be sure the changed value will be in memory as soon as possible, so I cache can get the newest value
#if 0
	{
		if (datatype == ARM_WORD_TYPE)
			mem_write_word (state, pa, data);
		else if (datatype == ARM_HALFWORD_TYPE)
			mem_write_halfword (state, (pa | (real_va & 2)),
					    data);
		else if (datatype == ARM_BYTE_TYPE)
			mem_write_byte (state, (pa | (real_va & 3)), data);
	}
#endif
#if 0
//chy 2003-09-02 for test ????
	if (datatype == ARM_WORD_TYPE) {
		if (real_va >= 0xffff0000 && real_va <= 0xffff0020) {
			printf ("**SKYEYE:mmu_write word:cache: pa %x, va %x, data %x, R15 %x\n", pa, real_va, data, state->Reg[15]);
		}
	}
//-------------------------------------------------------------
#endif
	if (datatype == ARM_WORD_TYPE)
		//mem_write_word (state, pa, data);
		bus_write(32, pa, data);
	else if (datatype == ARM_HALFWORD_TYPE)
		//mem_write_halfword (state, (pa | (real_va & 2)), data);
		bus_write(16, pa | (real_va & 2), data);
	else if (datatype == ARM_BYTE_TYPE)
		//mem_write_byte (state, (pa | (real_va & 3)), data);
		bus_write(8, (pa | (real_va & 3)), data);
	return NO_FAULT;
}

ARMword xscale_cp15_mrc (ARMul_State * state,
			 unsigned type, ARMword instr, ARMword * value)
{
	return xscale_mmu_mrc (state, instr, value);
}

ARMword xscale_mmu_mrc (ARMul_State * state, ARMword instr, ARMword * value)
{
	ARMword data;
	unsigned opcode_2 = BITS (5, 7);
	unsigned CRm = BITS (0, 3);
	unsigned reg = BITS (16, 19);
	unsigned result;
	mmu_regnum_t creg = (mmu_regnum_t)reg;

/*
 printf("SKYEYE: xscale_cp15_mrc:opcode_2 0x%x, CRm 0x%x, reg 0x%x,reg[15] 0x%x, instr %x\n",opcode_2,CRm,reg,\
	state->Reg[15], instr);
*/
	switch (creg) {
	case MMU_ID:		//XSCALE_CP15
		//printf("mmu_mrc read ID       \n");
		data = (opcode_2 ? state->mmu.cache_type : state->cpu->
			cpu_val);
		break;
	case MMU_CONTROL:	//XSCALE_CP15_AUX_CONTROL
		//printf("mmu_mrc read CONTROL  \n");
		data = (opcode_2 ? state->mmu.aux_control : state->mmu.
			control);
		break;
	case MMU_TRANSLATION_TABLE_BASE:
		//printf("mmu_mrc read TTB      \n");
		data = state->mmu.translation_table_base;
		break;
	case MMU_DOMAIN_ACCESS_CONTROL:
		//printf("mmu_mrc read DACR     \n");
		data = state->mmu.domain_access_control;
		break;
	case MMU_FAULT_STATUS:
		//printf("mmu_mrc read FSR      \n");
		data = state->mmu.fault_status;
		break;
	case MMU_FAULT_ADDRESS:
		//printf("mmu_mrc read FAR      \n");
		data = state->mmu.fault_address;
		break;
	case MMU_PID:
		//printf("mmu_mrc read PID      \n");
		data = state->mmu.process_id;
	case XSCALE_CP15_COPRO_ACCESS:
		//printf("xscale cp15 read coprocessor access\n");
		data = state->mmu.copro_access;
		break;
	default:
		data = 0;
		printf ("SKYEYE: xscale_cp15_mrc read UNKNOWN - reg %d, pc 0x%x\n", creg, state->Reg[15]);
		// skyeye_exit (-1);
		break;
	}
	*value = data;
	//printf("SKYEYE: xscale_cp15_mrc:end value  0x%x\n",data);
	return ARMul_DONE;
}

void xscale_cp15_cache_ops (ARMul_State * state, ARMword instr, ARMword value)
{
//chy: 2003-08-24 now, the BTB isn't simualted ....????

	unsigned CRm, OPC_2;

	CRm = BITS (0, 3);
	OPC_2 = BITS (5, 7);
	//err_msg("SKYEYE: xscale cp15_cache_ops:OPC_2 = 0x%x CRm = 0x%x, Reg15 0x%x\n", OPC_2, CRm,state->Reg[15]);

	if (OPC_2 == 0 && CRm == 7) {
		mmu_cache_invalidate_all (state, I_CACHE ());
		mmu_cache_invalidate_all (state, MAIN_D_CACHE ());
		return;
	}

	if (OPC_2 == 0 && CRm == 5) {
		mmu_cache_invalidate_all (state, I_CACHE ());
		return;
	}
	if (OPC_2 == 1 && CRm == 5) {
		mmu_cache_invalidate (state, I_CACHE (), value);
		return;
	}

	if (OPC_2 == 0 && CRm == 6) {
		mmu_cache_invalidate_all (state, MAIN_D_CACHE ());
		return;
	}

	if (OPC_2 == 1 && CRm == 6) {
		mmu_cache_invalidate (state, MAIN_D_CACHE (), value);
		return;
	}

	if (OPC_2 == 1 && CRm == 0xa) {
		mmu_cache_clean (state, MAIN_D_CACHE (), value);
		return;
	}

	if (OPC_2 == 4 && CRm == 0xa) {
		mmu_wb_drain_all (state, WB ());
		return;
	}

	if (OPC_2 == 6 && CRm == 5) {
		//chy 2004-07-19 shoud fix in the future????!!!!
		//printf("SKYEYE: xscale_cp15_cache_ops:invalidate BTB CANT!!!!!!!!!!\n"); 
		//exit(-1);
		return;
	}

	if (OPC_2 == 5 && CRm == 2) {
		//printf("SKYEYE: cp15_c_o: A L in D C, value %x, reg15 %x\n",value, state->Reg[15]); 
		//exit(-1);
		//chy 2003-09-01 for test
		mmu_cache_invalidate_all (state, MAIN_D_CACHE ());
		return;
	}

	ERROR_LOG(ARM11, "SKYEYE: xscale cp15_cache_ops:Unknown OPC_2 = 0x%x CRm = 0x%x, Reg15 0x%x\n", OPC_2, CRm, state->Reg[15]);
	// skyeye_exit (-1);
}

static void
	xscale_cp15_tlb_ops (ARMul_State * state, ARMword instr,
			     ARMword value)
{
	int CRm, OPC_2;

	CRm = BITS (0, 3);
	OPC_2 = BITS (5, 7);


	//err_msg("SKYEYE:xscale_cp15_tlb_ops:OPC_2 = 0x%x CRm = 0x%x,Reg[15] 0x%x\n", OPC_2, CRm,state->Reg[15]);
	if (OPC_2 == 0 && CRm == 0x7) {
		mmu_tlb_invalidate_all (state, I_TLB ());
		mmu_tlb_invalidate_all (state, D_TLB ());
		return;
	}

	if (OPC_2 == 0 && CRm == 0x5) {
		mmu_tlb_invalidate_all (state, I_TLB ());
		return;
	}

	if (OPC_2 == 1 && CRm == 0x5) {
		mmu_tlb_invalidate_entry (state, I_TLB (), value);
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

	ERROR_LOG(ARM11, "SKYEYE:xscale_cp15_tlb_ops:Unknow OPC_2 = 0x%x CRm = 0x%x,Reg[15] 0x%x\n", OPC_2, CRm, state->Reg[15]);
	// skyeye_exit (-1);
}


ARMword xscale_cp15_mcr (ARMul_State * state,
			 unsigned type, ARMword instr, ARMword value)
{
	return xscale_mmu_mcr (state, instr, value);
}

ARMword xscale_mmu_mcr (ARMul_State * state, ARMword instr, ARMword value)
{
	ARMword data;
	unsigned opcode_2 = BITS (5, 7);
	unsigned CRm = BITS (0, 3);
	unsigned reg = BITS (16, 19);
	unsigned result;
	mmu_regnum_t creg = (mmu_regnum_t)reg;

	//printf("SKYEYE: xscale_cp15_mcr: opcode_2 0x%x, CRm 0x%x, reg ox%x, value 0x%x, reg[15] 0x%x, instr 0x%x\n",opcode_2,CRm,reg, value, state->Reg[15], instr);

	switch (creg) {
	case MMU_CONTROL:
		//printf("mmu_mcr wrote CONTROL  val 0x%x       \n",value);
		state->mmu.control =
			(opcode_2 ? (value & 0x33) : (value & 0x3FFF));
		break;
	case MMU_TRANSLATION_TABLE_BASE:
		//printf("mmu_mcr wrote TTB      val 0x%x       \n",value);
		state->mmu.translation_table_base = value & 0xFFFFC000;
		break;
	case MMU_DOMAIN_ACCESS_CONTROL:
		//printf("mmu_mcr wrote DACR    val 0x%x \n",value);
		state->mmu.domain_access_control = value;
		break;

	case MMU_FAULT_STATUS:
		//printf("mmu_mcr wrote FS        val 0x%x \n",value);
		state->mmu.fault_status = value & 0x6FF;
		break;
	case MMU_FAULT_ADDRESS:
		//printf("mmu_mcr wrote FA         val 0x%x \n",value);
		state->mmu.fault_address = value;
		break;

	case MMU_CACHE_OPS:
//              printf("mmu_mcr wrote CO         val 0x%x \n",value);
		xscale_cp15_cache_ops (state, instr, value);
		break;
	case MMU_TLB_OPS:
		//printf("mmu_mcr wrote TO          val 0x%x \n",value);
		xscale_cp15_tlb_ops (state, instr, value);
		break;
	case MMU_PID:
		//printf("mmu_mcr wrote PID          val 0x%x \n",value);
		state->mmu.process_id = value & 0xfe000000;
		break;
	case XSCALE_CP15_COPRO_ACCESS:
		//printf("xscale cp15 write coprocessor access  val 0x %x\n",value);
		state->mmu.copro_access = value & 0x3ff;
		break;

	default:
		printf ("SKYEYE: xscale_cp15_mcr wrote UNKNOWN - reg %d, reg15 0x%x\n", creg, state->Reg[15]);
		break;
	}
	//printf("SKYEYE: xscale_cp15_mcr wrote val 0x%x\n", value);
	return 0;
}

//teawater add for arm2x86 2005.06.24-------------------------------------------
static int xscale_mmu_v2p_dbct (ARMul_State * state, ARMword virt_addr,
				ARMword * phys_addr)
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

/*xscale mmu_ops_t*/
//mmu_ops_t xscale_mmu_ops = {
//	xscale_cp15_init,
//		xscale_cp15_exit,
//		xscale_mmu_read_byte,
//		xscale_mmu_write_byte,
//		xscale_mmu_read_halfword,
//		xscale_mmu_write_halfword,
//		xscale_mmu_read_word,
//		xscale_mmu_write_word,
//		xscale_mmu_load_instr, xscale_mmu_mcr, xscale_mmu_mrc,
////teawater add for arm2x86 2005.06.24-------------------------------------------
//		xscale_mmu_v2p_dbct,
////AJ2D--------------------------------------------------------------------------
//};
