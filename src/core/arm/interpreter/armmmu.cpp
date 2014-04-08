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
#include "armdefs.h"
/* two header for arm disassemble */
//#include "skyeye_arch.h"
#include "armcpu.h"


extern mmu_ops_t xscale_mmu_ops;
exception_t arm_mmu_write(short size, u32 addr, uint32_t *value);
exception_t arm_mmu_read(short size, u32 addr, uint32_t *value);
#define MMU_OPS (state->mmu.ops)
ARMword skyeye_cachetype = -1;

int
mmu_init (ARMul_State * state)
{
	int ret;

	state->mmu.control = 0x70;
	state->mmu.translation_table_base = 0xDEADC0DE;
	state->mmu.domain_access_control = 0xDEADC0DE;
	state->mmu.fault_status = 0;
	state->mmu.fault_address = 0;
	state->mmu.process_id = 0;

	switch (state->cpu->cpu_val & state->cpu->cpu_mask) {
	//case SA1100:
	//case SA1110:
	//	NOTICE_LOG(ARM11, "SKYEYE: use sa11xx mmu ops\n");
	//	state->mmu.ops = sa_mmu_ops;
	//	break;
	//case PXA250:
	//case PXA270:		//xscale
	//	NOTICE_LOG(ARM11, "SKYEYE: use xscale mmu ops\n");
	//	state->mmu.ops = xscale_mmu_ops;
	//	break;
	//case 0x41807200:	//arm720t
	//case 0x41007700:	//arm7tdmi
	//case 0x41007100:	//arm7100
	//	NOTICE_LOG(ARM11,  "SKYEYE: use arm7100 mmu ops\n");
	//	state->mmu.ops = arm7100_mmu_ops;
	//	break;
	//case 0x41009200:
	//	NOTICE_LOG(ARM11, "SKYEYE: use arm920t mmu ops\n");
	//	state->mmu.ops = arm920t_mmu_ops;
	//	break;
	//case 0x41069260:
	//	NOTICE_LOG(ARM11, "SKYEYE: use arm926ejs mmu ops\n");
	//	state->mmu.ops = arm926ejs_mmu_ops;
	//	break;
	/* case 0x560f5810: */
	case 0x0007b000:
		NOTICE_LOG(ARM11, "SKYEYE: use arm11jzf-s mmu ops\n");
		state->mmu.ops = arm1176jzf_s_mmu_ops;
		break;

	default:
		ERROR_LOG (ARM11,
			 "SKYEYE: armmmu.c : mmu_init: unknown cpu_val&cpu_mask 0x%x\n",
			 state->cpu->cpu_val & state->cpu->cpu_mask);
		break;

	};
	ret = state->mmu.ops.init (state);
	state->mmu_inited = (ret == 0);
	/* initialize mmu_read and mmu_write for disassemble */
	//skyeye_config_t *config  = get_current_config();
	//generic_arch_t *arch_instance = get_arch_instance(config->arch->arch_name);
	//arch_instance->mmu_read = arm_mmu_read;
	//arch_instance->mmu_write = arm_mmu_write;

	return ret;
}

int
mmu_reset (ARMul_State * state)
{
	if (state->mmu_inited)
		mmu_exit (state);
	return mmu_init (state);
}

void
mmu_exit (ARMul_State * state)
{
	MMU_OPS.exit (state);
	state->mmu_inited = 0;
}

fault_t
mmu_read_byte (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	return MMU_OPS.read_byte (state, virt_addr, data);
};

fault_t
mmu_read_halfword (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	return MMU_OPS.read_halfword (state, virt_addr, data);
};

fault_t
mmu_read_word (ARMul_State * state, ARMword virt_addr, ARMword * data)
{
	return MMU_OPS.read_word (state, virt_addr, data);
};

fault_t
mmu_write_byte (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	fault_t fault;
	//static int count = 0;
	//count ++;
	fault = MMU_OPS.write_byte (state, virt_addr, data);
	return fault;
}

fault_t
mmu_write_halfword (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	fault_t fault;
	//static int count = 0;
	//count ++;
	fault = MMU_OPS.write_halfword (state, virt_addr, data);
	return fault;
}

fault_t
mmu_write_word (ARMul_State * state, ARMword virt_addr, ARMword data)
{
	fault_t fault;
	fault = MMU_OPS.write_word (state, virt_addr, data);

	/*used for debug for MMU*

	   if (!fault){
	   ARMword tmp;

	   if (mmu_read_word(state, virt_addr, &tmp)){
	   err_msg("load back\n");
	   exit(-1);
	   }else{
	   if (tmp != data){
	   err_msg("load back not equal %d %x\n", count, virt_addr);
	   }
	   }
	   }
	 */

	return fault;
};

fault_t
mmu_load_instr (ARMul_State * state, ARMword virt_addr, ARMword * instr)
{
	return MMU_OPS.load_instr (state, virt_addr, instr);
}

ARMword
mmu_mrc (ARMul_State * state, ARMword instr, ARMword * value)
{
	return MMU_OPS.mrc (state, instr, value);
}

void
mmu_mcr (ARMul_State * state, ARMword instr, ARMword value)
{
	MMU_OPS.mcr (state, instr, value);
}

/*ywc 20050416*/
int
mmu_v2p_dbct (ARMul_State * state, ARMword virt_addr, ARMword * phys_addr)
{
	return (MMU_OPS.v2p_dbct (state, virt_addr, phys_addr));
}

//
//
///* dis_mmu_read for disassemble */
//exception_t arm_mmu_read(short size, uint32_t addr, uint32_t * value)
//{
//	ARMul_State *state;
//	ARM_CPU_State *cpu = get_current_cpu();
//	state = &cpu->core[0];
//	switch(size){
//	case 8:
//		MMU_OPS.read_byte (state, addr, value);
//		break;
//	case 16:
//	case 32:
//		break;
//	default:
//		ERROR_LOG(ARM11, "Error size %d", size);
//		break;
//	}
//	return No_exp;
//}
///* dis_mmu_write for disassemble */
//exception_t arm_mmu_write(short size, uint32_t addr, uint32_t *value)
//{
//	ARMul_State *state;
//	ARM_CPU_State *cpu = get_current_cpu();
//		state = &cpu->core[0];
//	switch(size){
//	case 8:
//		MMU_OPS.write_byte (state, addr, value);
//		break;
//	case 16:
//	case 32:
//		break;
//	default:
//		printf("In %s error size %d Line %d\n", __func__, size, __LINE__);
//		break;
//	}
//	return No_exp;
//}
