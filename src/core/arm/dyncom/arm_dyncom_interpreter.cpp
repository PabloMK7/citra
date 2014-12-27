/* Copyright (C) 
* 2012 - Michael.Kang blackfin.kang@gmail.com
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
/**
* @file arm_dyncom_interpreter.cpp
* @brief The fast interpreter for arm
* @author Michael.Kang blackfin.kang@gmail.com
* @version 7849
* @date 2012-03-15
*/

#define CITRA_IGNORE_EXIT(x)

#include <algorithm>
#include <unordered_map>
#include <stdio.h>
#include <assert.h>
#include <cstdio>
#include <vector>

using namespace std;

#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armmmu.h"
#include "arm_dyncom_thumb.h"
#include "arm_dyncom_run.h"
#include "core/arm/skyeye_common/vfp/vfp.h"
/* shenoubang 2012-6-14 */
#ifdef __WIN32__
#include "bank_defs.h"
#endif

#include "core/mem_map.h"
#include "core/hle/hle.h"

enum {
    COND = (1 << 0),
    NON_BRANCH = (1 << 1),
    DIRECT_BRANCH = (1 << 2),
    INDIRECT_BRANCH = (1 << 3),
    CALL = (1 << 4),
    RET = (1 << 5),
    END_OF_PAGE = (1 << 6),
    THUMB = (1 << 7)
};

#define USER_MODE_OPT	1
#define HYBRID_MODE		0 // Enable for JIT mode

#define THRESHOLD			1000
#define DURATION			500
//#define PRINT_PROFILE_INFO

#define CHECK_RS 	if(RS == 15) rs += 8
#define CHECK_RM 	if(RM == 15) rm += 8

//#define BITS(s, a, b) (((s) >> (a)) & ((1 << (1 + (b) - (a))) - 1))
#undef BITS
#define BITS(s, a, b) ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) ((s >> (n)) & 1)
#define RM	BITS(sht_oper, 0, 3)
#define RS	BITS(sht_oper, 8, 11)

#define glue(x, y)		x ## y
#define DPO(s)			glue(DataProcessingOperands, s)
#define ROTATE_RIGHT(n, i, l)	((n << (l - i)) | (n >> i))
#define ROTATE_LEFT(n, i, l)	((n >> (l - i)) | (n << i))
#define ROTATE_RIGHT_32(n, i)	ROTATE_RIGHT(n, i, 32)
#define ROTATE_LEFT_32(n, i)	ROTATE_LEFT(n, i, 32)

//#define rotr(x,n) ((((x)>>(n))&((1<<(sizeof(x) * 8)-1))|(x<<(sizeof(x)*8-n))))
//#define rotl(x,n) ((((x)<<(n))&(-(1<<(n))))|(((x)>>(sizeof(x)*8-n))&((1<<(n))-1)))
#define rotr(x,n) ( (x >> n) | ((x & ((1 << (n + 1)) - 1)) << (32 - n)) )

extern void switch_mode(arm_core_t *core, uint32_t mode);
//extern bool InAPrivilegedMode(arm_core_t *core);

typedef arm_core_t arm_processor;
typedef unsigned int (*shtop_fp_t)(arm_processor *cpu, unsigned int sht_oper);

/* exclusive memory access */
static int exclusive_detect(ARMul_State* state, ARMword addr){
	#if 0
	for(int i = 0; i < 128; i++){
		if(state->exclusive_tag_array[i] == addr)
			return 0;
	}
	#endif
	if(state->exclusive_tag == addr)
		return 0;
	else
		return -1;
}

static void add_exclusive_addr(ARMul_State* state, ARMword addr){
	#if 0
	for(int i = 0; i < 128; i++){
		if(state->exclusive_tag_array[i] == 0xffffffff){
			state->exclusive_tag_array[i] = addr;
			//DEBUG_LOG(ARM11, "In %s, add  addr 0x%x\n", __func__, addr);
			return;
		}
	}
	DEBUG_LOG(ARM11, "In %s ,can not monitor the addr, out of array\n", __FUNCTION__);
	#endif
	state->exclusive_tag = addr;
	return;
}

static void remove_exclusive(ARMul_State* state, ARMword addr){
	#if 0
	int i;
	for(i = 0; i < 128; i++){
		if(state->exclusive_tag_array[i] == addr){
			state->exclusive_tag_array[i] = 0xffffffff;
			//DEBUG_LOG(ARM11, "In %s, remove  addr 0x%x\n", __func__, addr);
			return;
		}
	}
	#endif
	state->exclusive_tag = 0xFFFFFFFF;
}


unsigned int DPO(Immediate)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int immed_8 = BITS(sht_oper, 0, 7);
	unsigned int rotate_imm = BITS(sht_oper, 8, 11);
//	DEBUG_LOG(ARM11, "immed_8 is %x\n", immed_8);
//	DEBUG_LOG(ARM11, "rotate_imm is %x\n", rotate_imm);
	unsigned int shifter_operand = ROTATE_RIGHT_32(immed_8, rotate_imm * 2);//ROTATE_RIGHT_32(immed_8, rotate_imm * 2);
//	DEBUG_LOG(ARM11, "shifter_operand : %x\n", shifter_operand);
	/* set c flag */
	if (rotate_imm == 0) 
		cpu->shifter_carry_out = cpu->CFlag;
	else
		cpu->shifter_carry_out = BIT(shifter_operand, 31);
	return shifter_operand;
}

unsigned int DPO(Register)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand = rm;
	cpu->shifter_carry_out = cpu->CFlag;
	return shifter_operand;
}

unsigned int DPO(LogicalShiftLeftByImmediate)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	int shift_imm = BITS(sht_oper, 7, 11);
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand;
	if (shift_imm == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = cpu->CFlag;
	} else {
		shifter_operand = rm << shift_imm;
		cpu->shifter_carry_out = BIT(rm, 32 - shift_imm);
	}
	return shifter_operand;
}

unsigned int DPO(LogicalShiftLeftByRegister)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	int shifter_operand;
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	unsigned int rs = CHECK_READ_REG15(cpu, RS);
	//if (RM == 15) rm += 8;
	//if (RS == 15) rs += 8;
	if (BITS(rs, 0, 7) == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = cpu->CFlag;
	} else if (BITS(rs, 0, 7) < 32) {
		shifter_operand = rm << BITS(rs, 0, 7);
		cpu->shifter_carry_out = BIT(rm, 32 - BITS(rs, 0, 7));
	} else if (BITS(rs, 0, 7) == 32) {
		shifter_operand = 0;
		cpu->shifter_carry_out = BIT(rm, 0);
	} else {
		shifter_operand = 0;
		cpu->shifter_carry_out = 0;
	}
	return shifter_operand;
}

unsigned int DPO(LogicalShiftRightByImmediate)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	//unsigned int rm = cpu->Reg[RM];
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand;
	int shift_imm = BITS(sht_oper, 7, 11);
	if (shift_imm == 0) {
		shifter_operand = 0;
		cpu->shifter_carry_out = BIT(rm, 31);
	} else {
		shifter_operand = rm >> shift_imm;
		cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
	}
	return shifter_operand;
}

unsigned int DPO(LogicalShiftRightByRegister)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int rs = CHECK_READ_REG15(cpu, RS);
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RS == 15) rs += 8;
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand;
	if (BITS(rs, 0, 7) == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = cpu->CFlag;
	} else if (BITS(rs, 0, 7) < 32) {
		shifter_operand = rm >> BITS(rs, 0, 7);
		cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 7) - 1);
	} else if (BITS(rs, 0, 7) == 32) {
		shifter_operand = 0;
		cpu->shifter_carry_out = BIT(rm, 31);
	} else {
		shifter_operand = 0;
		cpu->shifter_carry_out = 0;
	}
	return shifter_operand;
}

unsigned int DPO(ArithmeticShiftRightByImmediate)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	//unsigned int rm = cpu->Reg[RM];
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand;
	int shift_imm = BITS(sht_oper, 7, 11);
	if (shift_imm == 0) {
		if (BIT(rm, 31)) {
			shifter_operand = 0;
			cpu->shifter_carry_out = BIT(rm, 31);
		} else {
			shifter_operand = 0xFFFFFFFF;
			cpu->shifter_carry_out = BIT(rm, 31);
		}
	} else {
		shifter_operand = static_cast<int>(rm) >> shift_imm;
		cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
	}
	return shifter_operand;
}

unsigned int DPO(ArithmeticShiftRightByRegister)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	//unsigned int rs = cpu->Reg[RS];
	unsigned int rs = CHECK_READ_REG15(cpu, RS);
	//unsigned int rm = cpu->Reg[RM];
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RS == 15) rs += 8;
	//if (RM == 15) rm += 8;
	unsigned int shifter_operand;
	if (BITS(rs, 0, 7) == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = cpu->CFlag;
	} else if (BITS(rs, 0, 7) < 32) {
		shifter_operand = static_cast<int>(rm) >> BITS(rs, 0, 7);
		cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 7) - 1);
	} else {
		if (BIT(rm, 31) == 0) {
			shifter_operand = 0;
		} else 
			shifter_operand = 0xffffffff;
		cpu->shifter_carry_out = BIT(rm, 31);
	}
	return shifter_operand;
}

unsigned int DPO(RotateRightByImmediate)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int shifter_operand;
	//unsigned int rm = cpu->Reg[RM];
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	int shift_imm = BITS(sht_oper, 7, 11);
	if (shift_imm == 0) {
		shifter_operand = (cpu->CFlag << 31) | 
					(rm >> 1);
		cpu->shifter_carry_out = BIT(rm, 0);
	} else {
		shifter_operand = ROTATE_RIGHT_32(rm, shift_imm);
		cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
	}
	return shifter_operand;
}

unsigned int DPO(RotateRightByRegister)(arm_processor *cpu, unsigned int sht_oper)
{
//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int rm = CHECK_READ_REG15(cpu, RM);
	//if (RM == 15) rm += 8;
	unsigned int rs = CHECK_READ_REG15(cpu, RS);
	//if (RS == 15) rs += 8;
	unsigned int shifter_operand;
	if (BITS(rs, 0, 7) == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = cpu->CFlag;
	} else if (BITS(rs, 0, 4) == 0) {
		shifter_operand = rm;
		cpu->shifter_carry_out = BIT(rm, 31);
	} else {
		shifter_operand = ROTATE_RIGHT_32(rm, BITS(rs, 0, 4));
		cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 4) - 1);
	}
	#if 0
	if (cpu->icounter >= 20371544) {
		DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
		DEBUG_LOG(ARM11, "RM:%d\nRS:%d\n", RM, RS);
		DEBUG_LOG(ARM11, "rm:0x%08x\nrs:0x%08x\n", cpu->Reg[RM], cpu->Reg[RS]);
	}
	#endif
	return shifter_operand;
}

//typedef unsigned int (*get_addr_fp_t)(arm_processor *cpu);
typedef struct _MiscImmeData {
	unsigned int U;
	unsigned int Rn;
	unsigned int offset_8;
} MiscLSData;

typedef struct _MiscRegData {
	unsigned int U;
	unsigned int Rn;
	unsigned int Rm;
} MiscRegData;

typedef struct _MiscImmePreIdx {
	unsigned int offset_8;
	unsigned int U;
	unsigned int Rn;
} MiscImmePreIdx;

typedef struct _MiscRegPreIdx {
	unsigned int U;
	unsigned int Rn;
	unsigned int Rm;
} MiscRegPreIdx;

typedef struct _MiscImmePstIdx {
	unsigned int offset_8;
	unsigned int U;
	unsigned int Rn;
} MIscImmePstIdx;

typedef struct _MiscRegPstIdx {
	unsigned int Rn;
	unsigned int Rm;
	unsigned int U;
} MiscRegPstIdx;

typedef struct _LSWordorUnsignedByte {
} LDnST;

#if USER_MODE_OPT
static inline fault_t interpreter_read_memory(addr_t virt_addr, addr_t phys_addr, uint32_t &value, uint32_t size){
	switch(size) {
	case 8:
        value = Memory::Read8(virt_addr);
		break;
	case 16:
        value = Memory::Read16(virt_addr);
		break;
	case 32:
        value = Memory::Read32(virt_addr);
		break;
	}
	return NO_FAULT;
}

//static inline void interpreter_write_memory(void *mem_ptr, uint32_t offset, uint32_t value, int size)
static inline fault_t interpreter_write_memory(addr_t virt_addr, addr_t phys_addr, uint32_t value, uint32_t size)
{
	switch(size) {
	case 8:
        Memory::Write8(virt_addr, value & 0xff);
		break;
	case 16:
		Memory::Write16(virt_addr, value & 0xffff);
		break;
	case 32:
		Memory::Write32(virt_addr, value);
		break;
	}
	return NO_FAULT;
}

static inline fault_t check_address_validity(arm_core_t *core, addr_t virt_addr, addr_t *phys_addr, uint32_t rw){
	*phys_addr = virt_addr;
	return NO_FAULT;
}

#else
fault_t interpreter_read_memory(cpu_t *cpu, addr_t virt_addr, addr_t phys_addr, uint32_t &value, uint32_t size);
fault_t interpreter_write_memory(cpu_t *cpu, addr_t virt_addr, addr_t phys_addr, uint32_t value, uint32_t size);
fault_t interpreter_fetch(cpu_t *cpu, addr_t virt_addr, uint32_t &value, uint32_t size);
fault_t check_address_validity(arm_core_t *core, addr_t virt_addr, addr_t *phys_addr, uint32_t rw, tlb_type_t access_type = DATA_TLB);
#endif

typedef fault_t (*get_addr_fp_t)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw);

typedef struct _ldst_inst {
	unsigned int inst;
	get_addr_fp_t get_addr;
} ldst_inst;
#define DEBUG_MSG LOG_DEBUG(Core_ARM11, "inst is %x", inst); CITRA_IGNORE_EXIT(0)

int CondPassed(arm_processor *cpu, unsigned int cond);
#define LnSWoUB(s)	glue(LnSWoUB, s)
#define MLnS(s)		glue(MLnS, s)
#define LdnStM(s)	glue(LdnStM, s)

#define W_BIT		BIT(inst, 21)
#define U_BIT		BIT(inst, 23)
#define I_BIT		BIT(inst, 25)
#define P_BIT		BIT(inst, 24)
#define OFFSET_12	BITS(inst, 0, 11)
fault_t LnSWoUB(ImmediateOffset)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int addr;
	fault_t fault;
	if (U_BIT) {
		addr = CHECK_READ_REG15_WA(cpu, Rn) + OFFSET_12;
	} else {
		addr = CHECK_READ_REG15_WA(cpu, Rn) - OFFSET_12;
	}
	//if (Rn == 15) rn += 8;
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	return fault;
//	return addr;
}

fault_t LnSWoUB(RegisterOffset)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst, 0, 3);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rm == 15) rm += 8;
	unsigned int addr;
	if (U_BIT) {
		addr = rn + rm;
	} else {
		addr = rn - rm;
	}
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	return fault;
}

fault_t LnSWoUB(ImmediatePostIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int addr = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) addr += 8;

	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (U_BIT) {
		cpu->Reg[Rn] += OFFSET_12;
	} else {
		cpu->Reg[Rn] -= OFFSET_12;
	}
	return fault;
}

fault_t LnSWoUB(ImmediatePreIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int addr;
	if (U_BIT) {
		addr = CHECK_READ_REG15_WA(cpu, Rn) + OFFSET_12;
	} else {
		addr = CHECK_READ_REG15_WA(cpu, Rn) - OFFSET_12;
	}
	#if 0
	if (Rn == 15) {
		addr += 8;
	}
	#endif

	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		cpu->Reg[Rn] = addr;
	}
	return fault;
}

fault_t MLnS(RegisterPreIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int addr;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst,  0,  3);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rn == 15) rn += 8;
	//if (Rm == 15) rm += 8;
	if (U_BIT) {
		addr = rn + rm;
	} else
		addr = rn - rm;
	if(BIT(inst, 20)){ /* L BIT */
	}
	if(BIT(inst, 6)){ /* Sign Bit */
	}
	if(BIT(inst, 5)){ /* Half Bit */
	}

	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		cpu->Reg[Rn] = addr;
	}
	return fault;
}

fault_t LnSWoUB(RegisterPreIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst, 0, 3);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rm == 15) rm += 8;
	unsigned int addr;
	if (U_BIT) {
		addr = rn + rm;
	} else {
		addr = rn - rm;
	}
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if(fault)
		return fault;
	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		cpu->Reg[Rn] = addr;
	}
	return fault;
}
fault_t LnSWoUB(ScaledRegisterPreIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int shift = BITS(inst, 5, 6);
	unsigned int shift_imm = BITS(inst, 7, 11);
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst, 0, 3);
	unsigned int index;
	unsigned int addr;

	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rm == 15) rm += 8;
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	switch (shift) {
	case 0:
		//DEBUG_MSG;
		index = rm << shift_imm;
		break;
	case 1:
//		DEBUG_MSG;
		if (shift_imm == 0) {
			index = 0;
		} else {
			index = rm >> shift_imm;
		}
		break;
	case 2:
		DEBUG_MSG;
		break;
	case 3:
		DEBUG_MSG;
		break;
	}
	if (U_BIT) {
		addr = rn + index;
	} else
		addr = rn - index;
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if(fault)
		return fault;
	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		cpu->Reg[Rn] = addr;
	}

	return fault;
}

fault_t LnSWoUB(ScaledRegisterPostIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int shift = BITS(inst, 5, 6);
	unsigned int shift_imm = BITS(inst, 7, 11);
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst, 0, 3);
	unsigned int index;
	unsigned int addr;

	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rm == 15) rm += 8;
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	addr = rn;
	switch (shift) {
	case 0:
		//DEBUG_MSG;
		index = rm << shift_imm;
		break;
	case 1:
//		DEBUG_MSG;
		if (shift_imm == 0) {
			index = 0;
		} else {
			index = rm >> shift_imm;
		}
		break;
	case 2:
		DEBUG_MSG;
		break;
	case 3:
		DEBUG_MSG;
		break;
	}
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if(fault)
		return fault;
	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		if (U_BIT)
			cpu->Reg[Rn] += index;
		else
			cpu->Reg[Rn] -= index;
	}

	return fault;
}

fault_t LnSWoUB(RegisterPostIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst,  0,  3);
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);

	unsigned int addr = rn;
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		if (U_BIT) {
			cpu->Reg[Rn] += rm;
		} else {
			cpu->Reg[Rn] -= rm;
		}
	}
	return fault;
}

fault_t MLnS(ImmediateOffset)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int immedL = BITS(inst, 0, 3);
	unsigned int immedH = BITS(inst, 8, 11);

	unsigned int Rn     = BITS(inst, 16, 19);
	unsigned int addr;

	unsigned int offset_8 = (immedH << 4) | immedL;
	if (U_BIT) {
		addr = CHECK_READ_REG15_WA(cpu, Rn) + offset_8;
	} else
		addr = CHECK_READ_REG15_WA(cpu, Rn) - offset_8;
	#if 0
	if (Rn == 15) {
		addr += 8;
	}
	#endif
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	return fault;
}

fault_t MLnS(RegisterOffset)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int addr;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst,  0,  3);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rn == 15) rn += 8;
	//if (Rm == 15) rm += 8;
	if (U_BIT) {
		addr = rn + rm;
	} else
		addr = rn - rm;
	if(BIT(inst, 20)){ /* L BIT */
	}
	if(BIT(inst, 6)){ /* Sign Bit */
	}
	if(BIT(inst, 5)){ /* Half Bit */
	}
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	return fault;
}

fault_t MLnS(ImmediatePreIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn     = BITS(inst, 16, 19);
	unsigned int immedH = BITS(inst,  8, 11);
	unsigned int immedL = BITS(inst,  0,  3);
	unsigned int addr;
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;

//	DEBUG_LOG(ARM11, "in %s\n", __FUNCTION__);
	unsigned int offset_8 = (immedH << 4) | immedL;
	if (U_BIT) {
		addr = rn + offset_8;
	} else 
		addr = rn - offset_8;

	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		cpu->Reg[Rn] = addr;
	}
	return fault;
}

fault_t MLnS(ImmediatePostIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn     = BITS(inst, 16, 19);
	unsigned int immedH = BITS(inst,  8, 11);
	unsigned int immedL = BITS(inst,  0,  3);
	unsigned int addr;
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	addr = rn;

	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		unsigned int offset_8 = (immedH << 4) | immedL;
		if (U_BIT) {
			rn += offset_8;
		} else {
			rn -= offset_8;
		}
		cpu->Reg[Rn] = rn;
	}

	return fault;
}
fault_t MLnS(RegisterPostIndexed)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst,  0,  3);
	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);

	unsigned int addr = rn;
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31))) {
		if (U_BIT) {
			cpu->Reg[Rn] += rm;
		} else {
			cpu->Reg[Rn] -= rm;
		}
	}
	return fault;
}

fault_t LdnStM(DecrementBefore)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int i = BITS(inst, 0, 15);
	int count = 0;
	while(i) {
		if(i & 1) count ++;
		i = i >> 1;
	}
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	unsigned int start_addr = rn - count * 4;
	unsigned int end_addr   = rn - 4;

	fault = check_address_validity(cpu, end_addr,   &phys_addr, rw);
	virt_addr = end_addr;
	if (fault) return fault;

	fault = check_address_validity(cpu, start_addr, &phys_addr, rw);
	virt_addr = start_addr;
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21)) {
		cpu->Reg[Rn] -= count * 4;
	}

	return fault;
}

fault_t LdnStM(IncrementBefore)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int i = BITS(inst, 0, 15);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	int count = 0;
	while(i) {
		if(i & 1) count ++;
		i = i >> 1;
	}

	unsigned int start_addr = rn + 4;
	unsigned int end_addr   = rn + count * 4;

	fault = check_address_validity(cpu, end_addr,   &phys_addr, rw);
	virt_addr = end_addr;
	if (fault) return fault;

	fault = check_address_validity(cpu, start_addr, &phys_addr, rw);
	virt_addr = start_addr;
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21)) {
		cpu->Reg[Rn] += count * 4;
	}
	return fault;
}

fault_t LdnStM(IncrementAfter)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int i = BITS(inst, 0, 15);
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	int count = 0;
	while(i) {
		if(i & 1) count ++;
		i = i >> 1;
	}
	//if (Rn == 15) rn += 8;
	unsigned int start_addr = rn;
	unsigned int end_addr = rn + count * 4 - 4;

	fault = check_address_validity(cpu, end_addr,   &phys_addr, rw);
	virt_addr = end_addr;
	if (fault) return fault;

	fault = check_address_validity(cpu, start_addr, &phys_addr, rw);
	virt_addr = start_addr;
	if (fault) return fault;

	if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21)) {
		cpu->Reg[Rn] += count * 4;
	}
	return fault;
}

fault_t LdnStM(DecrementAfter)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int i = BITS(inst, 0, 15);
	int count = 0;
	while(i) {
		if(i & 1) count ++;
		i = i >> 1;
	}
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	unsigned int start_addr = rn - count * 4 + 4;
	unsigned int end_addr   = rn;

	fault = check_address_validity(cpu, end_addr,   &phys_addr, rw);
	virt_addr = end_addr;
	if (fault) return fault;

	fault = check_address_validity(cpu, start_addr, &phys_addr, rw);
	if (fault) return fault;
	virt_addr = start_addr;

	if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21)) {
		cpu->Reg[Rn] -= count * 4;
	}
	return fault;
}

fault_t LnSWoUB(ScaledRegisterOffset)(arm_processor *cpu, unsigned int inst, unsigned int &virt_addr, unsigned int &phys_addr, unsigned int rw)
{
	fault_t fault;
	unsigned int shift = BITS(inst, 5, 6);
	unsigned int shift_imm = BITS(inst, 7, 11);
	unsigned int Rn = BITS(inst, 16, 19);
	unsigned int Rm = BITS(inst, 0, 3);
	unsigned int index;
	unsigned int addr;

	unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
	//if (Rm == 15) rm += 8;
	unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
	//if (Rn == 15) rn += 8;
	switch (shift) {
	case 0:
		//DEBUG_MSG;
		index = rm << shift_imm;
		break;
	case 1:
//		DEBUG_MSG;
		if (shift_imm == 0) {
			index = 0;
		} else {
			index = rm >> shift_imm;
		}
		break;
	case 2:
		if (shift_imm == 0){ /* ASR #32 */
			if (rm >> 31)
				index = 0xFFFFFFFF;
			else
				index = 0;
		}
		else {
			index = static_cast<int>(rm) >> shift_imm;
		}
		break;
	case 3:
		DEBUG_MSG;
		break;
	}
	if (U_BIT) {
		addr = rn + index;
	} else
		addr = rn - index;
	virt_addr = addr;
	fault = check_address_validity(cpu, addr, &phys_addr, rw);
	return fault;
}

#define ISNEG(n)	(n < 0)
#define ISPOS(n)	(n >= 0)

//enum {
//	COND            = (1 << 0),
//	NON_BRANCH      = (1 << 1),
//	DIRECT_BRANCH   = (1 << 2),
//	INDIRECT_BRANCH = (1 << 3),
//	CALL            = (1 << 4),
//	RET             = (1 << 5),
//	END_OF_PAGE     = (1 << 6),
//	THUMB           = (1 << 7)
//};

typedef struct _arm_inst {
	unsigned int idx;
	unsigned int cond;
	int br;
	int load_r15;
	char component[0];
} arm_inst;

typedef struct generic_arm_inst {
	u32 Ra;
	u32 Rm;
	u32 Rn;
	u32 Rd;
	u8 op1;
	u8 op2;
} generic_arm_inst;

typedef struct _adc_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} adc_inst;

typedef struct _add_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} add_inst;

typedef struct _orr_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} orr_inst;

typedef struct _and_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} and_inst;

typedef struct _eor_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} eor_inst;

typedef struct _bbl_inst {
	unsigned int L;
	int signed_immed_24;
	unsigned int next_addr;
	unsigned int jmp_addr;
} bbl_inst;

typedef struct _bx_inst {
	unsigned int Rm;
} bx_inst;

typedef struct _blx_inst {
	union {
		int32_t signed_immed_24;
		uint32_t Rm;
	} val;
	unsigned int inst;
} blx_inst;

typedef struct _clz_inst {
	unsigned int Rm;
	unsigned int Rd;
} clz_inst;

typedef struct _cps_inst {
	unsigned int imod0;
	unsigned int imod1;
	unsigned int mmod;
	unsigned int A, I, F;
	unsigned int mode;
} cps_inst;

typedef struct _clrex_inst {
} clrex_inst;

typedef struct _cpy_inst {
	unsigned int Rm;
	unsigned int Rd;
} cpy_inst;

typedef struct _bic_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} bic_inst;

typedef struct _sub_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} sub_inst;

typedef struct _tst_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} tst_inst;

typedef struct _cmn_inst {
	unsigned int I;
	//unsigned int S;
	unsigned int Rn;
	//unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} cmn_inst;

typedef struct _teq_inst {
	unsigned int I;
	unsigned int Rn;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} teq_inst;

typedef struct _stm_inst {
	unsigned int inst;
} stm_inst;

struct bkpt_inst {
};

struct blx1_inst {
	unsigned int addr;
};

struct blx2_inst {
	unsigned int Rm;
};

typedef struct _stc_inst {
} stc_inst;

typedef struct _ldc_inst {
} ldc_inst;

typedef struct _swi_inst {
	unsigned int num;
} swi_inst;

typedef struct _cmp_inst {
	unsigned int I;
	unsigned int Rn;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} cmp_inst;

typedef struct _mov_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} mov_inst;

typedef struct _mvn_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} mvn_inst;

typedef struct _rev_inst {
	unsigned int Rd;
	unsigned int Rm;
} rev_inst;

typedef struct _rsb_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} rsb_inst;

typedef struct _rsc_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} rsc_inst;

typedef struct _sbc_inst {
	unsigned int I;
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int shifter_operand;
	shtop_fp_t shtop_func;
} sbc_inst;

typedef struct _mul_inst {
	unsigned int S;
	unsigned int Rd;
	unsigned int Rs;
	unsigned int Rm;
} mul_inst;

typedef struct _smul_inst {
	unsigned int Rd;
	unsigned int Rs;
	unsigned int Rm;
	unsigned int x;
	unsigned int y;
} smul_inst;

typedef struct _umull_inst {
	unsigned int S;
	unsigned int RdHi;
	unsigned int RdLo;
	unsigned int Rs;
	unsigned int Rm;
} umull_inst;
typedef struct _smlad_inst {
	unsigned int m;
	unsigned int Rm;
	unsigned int Rd;
	unsigned int Ra;
	unsigned int Rn;
} smlad_inst;

typedef struct _smla_inst {
	unsigned int x;
	unsigned int y;
	unsigned int Rm;
	unsigned int Rd;
	unsigned int Rs;
	unsigned int Rn;
} smla_inst;

typedef struct umaal_inst {
	unsigned int Rn;
	unsigned int Rm;
	unsigned int RdHi;
	unsigned int RdLo;
} umaal_inst;

typedef struct _umlal_inst {
	unsigned int S;
	unsigned int Rm;
	unsigned int Rs;
	unsigned int RdHi;
	unsigned int RdLo;
} umlal_inst;

typedef struct _smlal_inst {
	unsigned int S;
	unsigned int Rm;
	unsigned int Rs;
	unsigned int RdHi;
	unsigned int RdLo;
} smlal_inst;

typedef struct _mla_inst {
	unsigned int S;
	unsigned int Rn;
	unsigned int Rd;
	unsigned int Rs;
	unsigned int Rm;
} mla_inst;

typedef struct _mrc_inst {
	unsigned int opcode_1;
	unsigned int opcode_2;
	unsigned int cp_num;
	unsigned int crn;
	unsigned int crm;
	unsigned int Rd;
	unsigned int inst;
} mrc_inst;

typedef struct _mcr_inst {
	unsigned int opcode_1;
	unsigned int opcode_2;
	unsigned int cp_num;
	unsigned int crn;
	unsigned int crm;
	unsigned int Rd;
	unsigned int inst;
} mcr_inst;

typedef struct _mrs_inst {
	unsigned int R;
	unsigned int Rd;
} mrs_inst;

typedef struct _msr_inst {
	unsigned int field_mask;
	unsigned int R;
	unsigned int inst;
} msr_inst;

typedef struct _pld_inst {
} pld_inst;

typedef struct _sxtb_inst {
	unsigned int Rd;
	unsigned int Rm;
	unsigned int rotate;
} sxtb_inst;

typedef struct _sxtab_inst {
	unsigned int Rd;
	unsigned int Rn;
	unsigned int Rm;
	unsigned rotate;
} sxtab_inst;

typedef struct _sxtah_inst {
	unsigned int Rd;
	unsigned int Rn;
	unsigned int Rm;
	unsigned int rotate;
} sxtah_inst;

typedef struct _sxth_inst {
	unsigned int Rd;
	unsigned int Rm;
	unsigned int rotate;
} sxth_inst;

typedef struct _uxtab_inst {
	unsigned int Rn;
	unsigned int Rd;
	unsigned int rotate;
	unsigned int Rm;
} uxtab_inst;

typedef struct _uxtah_inst {
	unsigned int Rn;
	unsigned int Rd;
	unsigned int rotate;
	unsigned int Rm;
} uxtah_inst;

typedef struct _uxth_inst {
	unsigned int Rd;
	unsigned int Rm;
	unsigned int rotate;
} uxth_inst;

typedef struct _cdp_inst {
	unsigned int opcode_1;
	unsigned int CRn;
	unsigned int CRd;
	unsigned int cp_num;
	unsigned int opcode_2;
	unsigned int CRm;
	uint32 inst;
}cdp_inst;

typedef struct _uxtb_inst {
	unsigned int Rd;
	unsigned int Rm;
	unsigned int rotate;
} uxtb_inst;

typedef struct _swp_inst {
	unsigned int Rn;
	unsigned int Rd;
	unsigned int Rm;
} swp_inst;

typedef struct _b_2_thumb {
	unsigned int imm;
}b_2_thumb;
typedef struct _b_cond_thumb {
	unsigned int imm;
	unsigned int cond;
}b_cond_thumb;

typedef struct _bl_1_thumb {
	unsigned int imm;
}bl_1_thumb;
typedef struct _bl_2_thumb {
	unsigned int imm;
}bl_2_thumb;
typedef struct _blx_1_thumb {
	unsigned int imm;
	unsigned int instr;
}blx_1_thumb;

typedef arm_inst * ARM_INST_PTR;

#define CACHE_BUFFER_SIZE	(64 * 1024 * 2000)
char inst_buf[CACHE_BUFFER_SIZE];
int top = 0;
inline void *AllocBuffer(unsigned int size)
{
	int start = top;
	top += size;
	if (top > CACHE_BUFFER_SIZE) {
		LOG_ERROR(Core_ARM11, "inst_buf is full");
		CITRA_IGNORE_EXIT(-1);
	}
	return (void *)&inst_buf[start];
}

int CondPassed(arm_processor *cpu, unsigned int cond)
{
	#define NFLAG		cpu->NFlag
	#define ZFLAG		cpu->ZFlag
	#define CFLAG		cpu->CFlag
	#define VFLAG		cpu->VFlag
	int temp;
	switch (cond) {
	case 0x0:
		temp = ZFLAG;
		break;
	case 0x1:   /* NE */
		temp = !ZFLAG;
		break;
	case 0x6:  /* VS */
		temp = VFLAG;
		break;
	case 0x7:        /* VC */
		temp = !VFLAG;
		break;
	case 0x4:         /* MI */
		temp = NFLAG;
		break;
	case 0x5:        /* PL */
		temp = !NFLAG;
		break;
	case 0x2:       /* CS */
		temp = CFLAG;
		break;
	case 0x3:      /* CC */
		temp = !CFLAG;
		break;
	case 0x8:     /* HI */
		temp = (CFLAG && !ZFLAG);
		break;
	case 0x9:    /* LS */
		temp = (!CFLAG || ZFLAG);
		break;
	case 0xa:   /* GE */
		temp = ((!NFLAG && !VFLAG) || (NFLAG && VFLAG));
		break;
	case 0xb:  /* LT */
		temp = ((NFLAG && !VFLAG) || (!NFLAG && VFLAG));
		break;
	case 0xc:  /* GT */
		temp = ((!NFLAG && !VFLAG && !ZFLAG)
			|| (NFLAG && VFLAG && !ZFLAG));
		break;
	case 0xd:  /* LE */
		temp = ((NFLAG && !VFLAG) || (!NFLAG && VFLAG))
			|| ZFLAG;
		break;
	case 0xe: /* AL */
		temp = 1;
		break;
	case 0xf:
//		DEBUG_LOG(ARM11, "inst is %x\n");
//		DEBUG_LOG(ARM11, "icounter is %lld\n", cpu->icounter);
//		CITRA_IGNORE_EXIT(-1);
		temp = 1;
		break;
	}
	return temp;
}

enum DECODE_STATUS {
	DECODE_SUCCESS,
	DECODE_FAILURE
};

int decode_arm_instr(uint32_t instr, int32_t *idx);

shtop_fp_t get_shtop(unsigned int inst)
{
	if (BIT(inst, 25)) {
		return DPO(Immediate);
	} else if (BITS(inst, 4, 11) == 0) {
		return DPO(Register);
	} else if (BITS(inst, 4, 6) == 0) {
		return DPO(LogicalShiftLeftByImmediate);
	} else if (BITS(inst, 4, 7) == 1) {
		return DPO(LogicalShiftLeftByRegister);
	} else if (BITS(inst, 4, 6) == 2) {
		return DPO(LogicalShiftRightByImmediate);
	} else if (BITS(inst, 4, 7) == 3) {
		return DPO(LogicalShiftRightByRegister);
	} else if (BITS(inst, 4, 6) == 4) {
		return DPO(ArithmeticShiftRightByImmediate);
	} else if (BITS(inst, 4, 7) == 5) {
		return DPO(ArithmeticShiftRightByRegister);
	} else if (BITS(inst, 4, 6) == 6) {
		return DPO(RotateRightByImmediate);
	} else if (BITS(inst, 4, 7) == 7) {
		return DPO(RotateRightByRegister);
	}
	return NULL;
}

get_addr_fp_t get_calc_addr_op(unsigned int inst)
{
	/* 1 */
	if (BITS(inst, 24, 27) == 5 && BIT(inst, 21) == 0) {
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LnSWoUB(ImmediateOffset);
	} else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 0 && BITS(inst, 4, 11) == 0) {
//		DEBUG_MSG;
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LnSWoUB(RegisterOffset);
	} else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 0 && BIT(inst, 4) == 0) {
//		DEBUG_MSG;
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LnSWoUB(ScaledRegisterOffset);
	} else if (BITS(inst, 24, 27) == 5 && BIT(inst, 21) == 1) {
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LnSWoUB(ImmediatePreIndexed);
	} else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 1 && BITS(inst, 4, 11) == 0) {
		return LnSWoUB(RegisterPreIndexed);
	} else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 1 && BIT(inst, 4) == 0) {
		return LnSWoUB(ScaledRegisterPreIndexed);
	} else if (BITS(inst, 24, 27) == 4 && BIT(inst, 21) == 0) {
		return LnSWoUB(ImmediatePostIndexed);
	} else if (BITS(inst, 24, 27) == 6 && BIT(inst, 21) == 0 && BITS(inst, 4, 11) == 0) {
//		DEBUG_MSG;
		return LnSWoUB(RegisterPostIndexed);
	} else if (BITS(inst, 24, 27) == 6 && BIT(inst, 21) == 0 && BIT(inst, 4) == 0) {
		return LnSWoUB(ScaledRegisterPostIndexed);
//		DEBUG_MSG;
	} else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 2 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
	/* 2 */
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return MLnS(ImmediateOffset);
//		DEBUG_MSG;
	} else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 0 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
//		DEBUG_LOG(ARM11, "line is %d\n", __LINE__);
		return MLnS(RegisterOffset);
//		DEBUG_MSG;
	} else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 3 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
//		DEBUG_LOG(ARM11, "line is %d\n", __LINE__);
		return MLnS(ImmediatePreIndexed);
//		DEBUG_MSG;
	} else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 1 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
		return MLnS(RegisterPreIndexed);
	} else if (BITS(inst, 24, 27) == 0 && BITS(inst, 21, 22) == 2 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
//		DEBUG_MSG;
		return MLnS(ImmediatePostIndexed);
	} else if (BITS(inst, 24, 27) == 0 && BITS(inst, 21, 22) == 0 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
		//DEBUG_MSG;
		return MLnS(RegisterPostIndexed);
	} else if (BITS(inst, 23, 27) == 0x11) {
	/* 3 */
//		DEBUG_MSG;
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LdnStM(IncrementAfter);
	} else if (BITS(inst, 23, 27) == 0x13) {
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LdnStM(IncrementBefore);
//		DEBUG_MSG;
	} else if (BITS(inst, 23, 27) == 0x10) {
//		DEBUG_MSG;
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LdnStM(DecrementAfter);
	} else if (BITS(inst, 23, 27) == 0x12) {
//		DEBUG_MSG;
//		DEBUG_LOG(ARM11, "line is %d", __LINE__);
		return LdnStM(DecrementBefore);
	}
	#if 0
	DEBUG_LOG(ARM11, "In %s Unknown addressing mode\n", __FUNCTION__);
	DEBUG_LOG(ARM11, "inst:%x\n", inst);
	CITRA_IGNORE_EXIT(-1);
	#endif
	return NULL;
}

#define INTERPRETER_TRANSLATE(s) glue(InterpreterTranslate_, s)

#define CHECK_RN			(inst_cream->Rn == 15)
#define CHECK_RM			(inst_cream->Rm == 15)
#define CHECK_RS			(inst_cream->Rs == 15)

#define UNIMPLEMENTED_INSTRUCTION(mnemonic) \
    LOG_ERROR(Core_ARM11, "unimplemented instruction: %s", mnemonic); \
    CITRA_IGNORE_EXIT(-1); \
    return nullptr;

ARM_INST_PTR INTERPRETER_TRANSLATE(adc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(adc_inst));
	adc_inst *inst_cream = (adc_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(add)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(add_inst));
	add_inst *inst_cream = (add_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(and)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(and_inst));
	and_inst *inst_cream = (and_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) 
		inst_base->br = INDIRECT_BRANCH;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bbl)(unsigned int inst, int index)
{
	#define POSBRANCH ((inst & 0x7fffff) << 2)
	#define NEGBRANCH ((0xff000000 |(inst & 0xffffff)) << 2)

	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(bbl_inst));
	bbl_inst *inst_cream = (bbl_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = DIRECT_BRANCH;

	if (BIT(inst, 24))
		inst_base->br = CALL;
	if (BITS(inst, 28, 31) <= 0xe)
		inst_base->br |= COND;

	inst_cream->L 	 = BIT(inst, 24);
	inst_cream->signed_immed_24 = BIT(inst, 23) ? NEGBRANCH : POSBRANCH;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bic)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(bic_inst));
	bic_inst *inst_cream = (bic_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (inst_cream->Rd == 15) 
		inst_base->br = INDIRECT_BRANCH;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bkpt)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("BKPT"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(blx)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(blx_inst));
	blx_inst *inst_cream = (blx_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = INDIRECT_BRANCH;

	inst_cream->inst = inst;
	if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
		inst_cream->val.Rm = BITS(inst, 0, 3);
	} else {
		inst_cream->val.signed_immed_24 = BITS(inst, 0, 23);
		//DEBUG_LOG(ARM11, " blx inst is %x\n", inst);
		//CITRA_IGNORE_EXIT(-1);
//		DEBUG_MSG;
	}

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bx)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(bx_inst));
	bx_inst *inst_cream = (bx_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = INDIRECT_BRANCH;

	inst_cream->Rm	 = BITS(inst, 0, 3);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bxj)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("BXJ"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(cdp)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(cdp_inst));
	cdp_inst *inst_cream = (cdp_inst *)inst_base->component;
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->CRm   = BITS(inst,  0,  3);
	inst_cream->CRd   = BITS(inst, 12, 15);
	inst_cream->CRn   = BITS(inst, 16, 19);
	inst_cream->cp_num   = BITS(inst, 8, 11);
	inst_cream->opcode_2   = BITS(inst, 5, 7);
	inst_cream->opcode_1   = BITS(inst, 20, 23);
	inst_cream->inst = inst;

	LOG_TRACE(Core_ARM11, "inst %x index %x", inst, index);
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(clrex)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(clrex_inst));
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(clz)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(clz_inst));
	clz_inst *inst_cream = (clz_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm   = BITS(inst,  0,  3);
	inst_cream->Rd   = BITS(inst, 12, 15);
	if (CHECK_RM) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(cmn)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(cmn_inst));
	cmn_inst *inst_cream = (cmn_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	//inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	//inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(cmp)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(cmp_inst));
	cmp_inst *inst_cream = (cmp_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(cps)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(cps_inst));
	cps_inst *inst_cream = (cps_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->imod0 = BIT(inst, 18);
	inst_cream->imod1 = BIT(inst, 19);
	inst_cream->mmod  = BIT(inst, 17);
	inst_cream->A	  = BIT(inst, 8);
	inst_cream->I	  = BIT(inst, 7);
	inst_cream->F	  = BIT(inst, 6);
	inst_cream->mode  = BITS(inst, 0, 4);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(cpy)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mov_inst));
	mov_inst *inst_cream = (mov_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(eor)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(eor_inst));
	eor_inst *inst_cream = (eor_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldc_inst));
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldm)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BIT(inst, 15)) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sxth)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sxtb_inst));
	sxtb_inst *inst_cream = (sxtb_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->Rm     = BITS(inst,  0,  3);
	inst_cream->rotate = BITS(inst, 10, 11);
	if (CHECK_RM) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldr)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(ldrcond)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(uxth)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(uxth_inst));
	uxth_inst *inst_cream = (uxth_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);
	if (CHECK_RM) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uxtah)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(uxtah_inst));
	uxtah_inst *inst_cream = (uxtah_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rn     = BITS(inst, 16, 19);
	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);
	if (CHECK_RM || CHECK_RN) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrbt)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	if (I_BIT == 0) {
		inst_cream->get_addr = LnSWoUB(ImmediatePostIndexed);
	} else {
		DEBUG_MSG;
	}
	#if 0
	inst_cream->get_addr = get_calc_addr_op(inst);
	if(inst == 0x54f13001) {
		DEBUG_LOG(ARM11, "get_calc_addr_op:%llx\n", inst_cream->get_addr);
	}
	#endif

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrd)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(ldrex)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	//inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrexb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrh)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrsb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrsh)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ldrt)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	if (I_BIT == 0) {
		inst_cream->get_addr = LnSWoUB(ImmediatePostIndexed);
	} else {
		DEBUG_MSG;
	}

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mcr)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mcr_inst));
	mcr_inst *inst_cream = (mcr_inst *)inst_base->component;
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->crn      = BITS(inst, 16, 19);
	inst_cream->crm      = BITS(inst,  0,  3);
	inst_cream->opcode_1 = BITS(inst, 21, 23);
	inst_cream->opcode_2 = BITS(inst,  5,  7);
	inst_cream->Rd       = BITS(inst, 12, 15);
	inst_cream->cp_num   = BITS(inst,  8, 11);
	inst_cream->inst     = inst;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mcrr)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("MCRR"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(mla)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mla_inst));
	mla_inst *inst_cream = (mla_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 12, 15);
	inst_cream->Rd	 = BITS(inst, 16, 19);
	inst_cream->Rs	 = BITS(inst,  8, 11);
	inst_cream->Rm	 = BITS(inst,  0,  3);

	if (CHECK_RM || CHECK_RN || CHECK_RS) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mov)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mov_inst));
	mov_inst *inst_cream = (mov_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mrc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mrc_inst));
	mrc_inst *inst_cream = (mrc_inst *)inst_base->component;
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->crn      = BITS(inst, 16, 19);
	inst_cream->crm      = BITS(inst,  0,  3);
	inst_cream->opcode_1 = BITS(inst, 21, 23);
	inst_cream->opcode_2 = BITS(inst,  5,  7);
	inst_cream->Rd       = BITS(inst, 12, 15);
	inst_cream->cp_num   = BITS(inst,  8, 11);
	inst_cream->inst     = inst;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mrrc)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("MRRC"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(mrs)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mrs_inst));
	mrs_inst *inst_cream = (mrs_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->Rd   = BITS(inst, 12, 15);
	inst_cream->R    = BIT(inst, 22);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(msr)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(msr_inst));
	msr_inst *inst_cream = (msr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->field_mask = BITS(inst, 16, 19);
	inst_cream->R          = BIT(inst, 22);
	inst_cream->inst       = inst;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mul)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mul_inst));
	mul_inst *inst_cream = (mul_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->Rd	 = BITS(inst, 16, 19);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(mvn)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(mvn_inst));
	mvn_inst *inst_cream = (mvn_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;

}
ARM_INST_PTR INTERPRETER_TRANSLATE(orr)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(orr_inst));
	orr_inst *inst_cream = (orr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(pkhbt)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("PKHBT"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(pkhtb)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("PKHTB"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(pld)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(pld_inst));

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(qadd)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("QADD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qadd8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("QADD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qadd16)(unsigned int inst, int index)
{
	arm_inst* const inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(generic_arm_inst));
	generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

	inst_base->cond     = BITS(inst, 28, 31);
	inst_base->idx      = index;
	inst_base->br       = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm  = BITS(inst, 0, 3);
	inst_cream->Rn  = BITS(inst, 16, 19);
	inst_cream->Rd  = BITS(inst, 12, 15);
	inst_cream->op1 = BITS(inst, 20, 21);
	inst_cream->op2 = BITS(inst, 5, 7);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(qaddsubx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(qadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(qdadd)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("QDADD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qdsub)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("QDSUB"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qsub)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("QSUB"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qsub8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("QSUB8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(qsub16)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(qadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(qsubaddx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(qadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(rev)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(rev_inst));
	rev_inst *inst_cream = (rev_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm   = BITS(inst,  0,  3);
	inst_cream->Rd   = BITS(inst, 12, 15);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(rev16)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(rev_inst));
	rev_inst *inst_cream = (rev_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm   = BITS(inst,  0,  3);
	inst_cream->Rd   = BITS(inst, 12, 15);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(revsh)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("REVSH"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(rfe)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("RFE"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(rsb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(rsb_inst));
	rsb_inst *inst_cream = (rsb_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (CHECK_RN) 
		inst_base->load_r15 = 1;

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(rsc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(rsc_inst));
	rsc_inst *inst_cream = (rsc_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (CHECK_RN)
		inst_base->load_r15 = 1;

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sadd8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SADD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(sadd16)(unsigned int inst, int index)
{
	arm_inst* const inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(generic_arm_inst));
	generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;
	
	inst_base->cond     = BITS(inst, 28, 31);
	inst_base->idx      = index;
	inst_base->br       = NON_BRANCH;
	inst_base->load_r15 = 0;
	
	inst_cream->Rm  = BITS(inst, 0, 3);
	inst_cream->Rn  = BITS(inst, 16, 19);
	inst_cream->Rd  = BITS(inst, 12, 15);
	inst_cream->op1 = BITS(inst, 20, 21);
	inst_cream->op2 = BITS(inst, 5, 7);
	
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(saddsubx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(sadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sbc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sbc_inst));
	sbc_inst *inst_cream = (sbc_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (CHECK_RN)
		inst_base->load_r15 = 1;

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sel)(unsigned int inst, int index)
{
	arm_inst* const inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(generic_arm_inst));
	generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

	inst_base->cond     = BITS(inst, 28, 31);
	inst_base->idx      = index;
	inst_base->br       = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm  = BITS(inst, 0, 3);
	inst_cream->Rn  = BITS(inst, 16, 19);
	inst_cream->Rd  = BITS(inst, 12, 15);
	inst_cream->op1 = BITS(inst, 20, 22);
	inst_cream->op2 = BITS(inst, 5, 7);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(setend)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SETEND"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shadd16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SHADD16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shadd8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SHADD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shaddsubx)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("SHADDSUBX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shsub16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SHSUB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shsub8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SHSUB8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(shsubaddx)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("SHSUBADDX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smla)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(smla_inst));
	smla_inst *inst_cream = (smla_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->x	 = BIT(inst, 5);
	inst_cream->y	 = BIT(inst, 6);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->Rd = BITS(inst, 16, 19);
	inst_cream->Rn = BITS(inst, 12, 15);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(smlad)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(smlad_inst));
	smlad_inst *inst_cream = (smlad_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->m	 = BIT(inst, 4);
	inst_cream->Rn	 = BITS(inst, 0, 3);
	inst_cream->Rm	 = BITS(inst, 8, 11);
	inst_cream->Rd = BITS(inst, 16, 19);
	inst_cream->Ra = BITS(inst, 12, 15);

	if (CHECK_RM ) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(smlal)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(umlal_inst));
	umlal_inst *inst_cream = (umlal_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->RdHi = BITS(inst, 16, 19);
	inst_cream->RdLo = BITS(inst, 12, 15);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(smlalxy)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("SMLALXY"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smlald)(unsigned int inst, int index)  { UNIMPLEMENTED_INSTRUCTION("SMLALD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smlaw)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMLAW"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smlsd)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMLSD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smlsld)(unsigned int inst, int index)  { UNIMPLEMENTED_INSTRUCTION("SMLSLD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smmla)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMMLA"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smmls)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMMLS"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smmul)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMMUL"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smuad)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SMUAD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(smul)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(smul_inst));
	smul_inst *inst_cream = (smul_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd = BITS(inst, 16, 19);
	inst_cream->Rs = BITS(inst,  8, 11);
	inst_cream->Rm = BITS(inst,  0,  3);

	inst_cream->x  = BIT(inst, 5);
	inst_cream->y  = BIT(inst, 6);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;

}
ARM_INST_PTR INTERPRETER_TRANSLATE(smull)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(umull_inst));
	umull_inst *inst_cream = (umull_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->RdHi = BITS(inst, 16, 19);
	inst_cream->RdLo = BITS(inst, 12, 15);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(smulw)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(smlad_inst));
	smlad_inst *inst_cream = (smlad_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->m	 = BIT(inst, 6);
	inst_cream->Rm	 = BITS(inst, 8, 11);
	inst_cream->Rn	 = BITS(inst, 0, 3);
	inst_cream->Rd = BITS(inst, 16, 19);

	if (CHECK_RM || CHECK_RN) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(smusd)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SMUSD"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(srs)(unsigned int inst, int index)      { UNIMPLEMENTED_INSTRUCTION("SRS"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(ssat)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("SSAT"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(ssat16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("SSAT16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(ssub8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("SSUB8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(ssub16)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(sadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(ssubaddx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(sadd16)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(stc)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(stc_inst));
	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(stm)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sxtb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sxtb_inst));
	sxtb_inst *inst_cream = (sxtb_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->Rm     = BITS(inst,  0,  3);
	inst_cream->rotate = BITS(inst, 10, 11);

	if (CHECK_RM) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(str)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uxtb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(uxth_inst));
	uxth_inst *inst_cream = (uxth_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);

	if (CHECK_RM) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uxtab)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(uxtab_inst));
	uxtab_inst *inst_cream = (uxtab_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);
	inst_cream->Rn     = BITS(inst, 16, 19);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strbt)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
//	inst_cream->get_addr = get_calc_addr_op(inst);
	if (I_BIT == 0) {
		inst_cream->get_addr = LnSWoUB(ImmediatePostIndexed);
	} else {
		DEBUG_MSG;
	}

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strd)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strex)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strexb)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strh)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	inst_cream->get_addr = get_calc_addr_op(inst);

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(strt)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(ldst_inst));
	ldst_inst *inst_cream = (ldst_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->inst = inst;
	if (I_BIT == 0) {
		inst_cream->get_addr = LnSWoUB(ImmediatePostIndexed);
	} else {
		DEBUG_MSG;
	}

	if (BITS(inst, 12, 15) == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sub)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sub_inst));
	sub_inst *inst_cream = (sub_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	if (CHECK_RN) 
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(swi)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(swi_inst));
	swi_inst *inst_cream = (swi_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->num  = BITS(inst, 0, 23);
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(swp)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(swp_inst));
	swp_inst *inst_cream = (swp_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->Rm	 = BITS(inst,  0,  3);

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(swpb)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(swp_inst));
	swp_inst *inst_cream = (swp_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;

	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->Rm	 = BITS(inst,  0,  3);

	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sxtab)(unsigned int inst, int index){
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sxtab_inst));
	sxtab_inst *inst_cream = (sxtab_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);
	inst_cream->Rn     = BITS(inst, 16, 19);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sxtab16)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("SXTAB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(sxtah)(unsigned int inst, int index){
	LOG_WARNING(Core_ARM11, "SXTAH untested");
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(sxtah_inst));
	sxtah_inst *inst_cream = (sxtah_inst *)inst_base->component;

	inst_base->cond = BITS(inst, 28, 31);
	inst_base->idx   = index;
	inst_base->br    = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rd     = BITS(inst, 12, 15);
	inst_cream->rotate = BITS(inst, 10, 11);
	inst_cream->Rm     = BITS(inst,  0,  3);
	inst_cream->Rn     = BITS(inst, 16, 19);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(sxtb16)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("SXTB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(teq)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(teq_inst));
	teq_inst *inst_cream = (teq_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);

	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(tst)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(tst_inst));
	tst_inst *inst_cream = (tst_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->I	 = BIT(inst, 25);
	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rn	 = BITS(inst, 16, 19);
	inst_cream->Rd	 = BITS(inst, 12, 15);
	inst_cream->shifter_operand = BITS(inst, 0, 11);
	inst_cream->shtop_func = get_shtop(inst);
	if (inst_cream->Rd == 15) {
		inst_base->br = INDIRECT_BRANCH;
	}

	if (CHECK_RN) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uadd8)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("UADD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uadd16)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("UADD16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uaddsubx)(unsigned int inst, int index)  { UNIMPLEMENTED_INSTRUCTION("UADDSUBX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uhadd8)(unsigned int inst, int index)
{
	arm_inst* const inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(generic_arm_inst));
	generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

	inst_base->cond     = BITS(inst, 28, 31);
	inst_base->idx      = index;
	inst_base->br       = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->op1 = BITS(inst, 20, 21);
	inst_cream->op2 = BITS(inst, 5, 7);
	inst_cream->Rm  = BITS(inst, 0, 3);
	inst_cream->Rn  = BITS(inst, 16, 19);
	inst_cream->Rd  = BITS(inst, 12, 15);

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uhadd16)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(uhadd8)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uhaddsubx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(uhadd8)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uhsub8)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(uhadd8)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uhsub16)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(uhadd8)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(uhsubaddx)(unsigned int inst, int index)
{
	return INTERPRETER_TRANSLATE(uhadd8)(inst, index);
}
ARM_INST_PTR INTERPRETER_TRANSLATE(umaal)(unsigned int inst, int index)
{
	arm_inst* const inst_base = (arm_inst*)AllocBuffer(sizeof(arm_inst) + sizeof(umaal_inst));
	umaal_inst* const inst_cream = (umaal_inst*)inst_base->component;

	inst_base->cond     = BITS(inst, 28, 31);
	inst_base->idx      = index;
	inst_base->br       = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->Rm   = BITS(inst, 8, 11);
	inst_cream->Rn   = BITS(inst, 0, 3);
	inst_cream->RdLo = BITS(inst, 12, 15);
	inst_cream->RdHi = BITS(inst, 16, 19);

	if (CHECK_RM || CHECK_RN)
		inst_base->load_r15 = 1;

	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(umlal)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(umlal_inst));
	umlal_inst *inst_cream = (umlal_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->RdHi = BITS(inst, 16, 19);
	inst_cream->RdLo = BITS(inst, 12, 15);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(umull)(unsigned int inst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(umull_inst));
	umull_inst *inst_cream = (umull_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->S	 = BIT(inst, 20);
	inst_cream->Rm	 = BITS(inst, 0, 3);
	inst_cream->Rs	 = BITS(inst, 8, 11);
	inst_cream->RdHi = BITS(inst, 16, 19);
	inst_cream->RdLo = BITS(inst, 12, 15);

	if (CHECK_RM || CHECK_RS) 
		inst_base->load_r15 = 1;
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(b_2_thumb)(unsigned int tinst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(b_2_thumb));
	b_2_thumb *inst_cream = (b_2_thumb *)inst_base->component;

	inst_cream->imm =((tinst & 0x3FF) << 1) | ((tinst & (1 << 10)) ? 0xFFFFF800 : 0);
	//DEBUG_LOG(ARM11, "In %s, tinst=0x%x, imm=0x%x\n", __FUNCTION__, tinst, inst_cream->imm);
	inst_base->idx = index;
	inst_base->br	 = DIRECT_BRANCH;
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(b_cond_thumb)(unsigned int tinst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(b_cond_thumb));
	b_cond_thumb *inst_cream = (b_cond_thumb *)inst_base->component;

	inst_cream->imm = (((tinst & 0x7F) << 1) | ((tinst & (1 << 7)) ?	0xFFFFFF00 : 0));
	inst_cream->cond = ((tinst >> 8) & 0xf);
	//DEBUG_LOG(ARM11, "In %s, tinst=0x%x, imm=0x%x, cond=0x%x\n", __FUNCTION__, tinst, inst_cream->imm, inst_cream->cond);
	inst_base->idx = index;
	inst_base->br	 = DIRECT_BRANCH;
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(bl_1_thumb)(unsigned int tinst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(bl_1_thumb));
	bl_1_thumb *inst_cream = (bl_1_thumb *)inst_base->component;

	inst_cream->imm = (((tinst & 0x07FF) << 12) | ((tinst & (1 << 10)) ? 0xFF800000 : 0));
	//DEBUG_LOG(ARM11, "In %s, tinst=0x%x, imm=0x%x\n", __FUNCTION__, tinst, inst_cream->imm);

	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(bl_2_thumb)(unsigned int tinst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(bl_2_thumb));
	bl_2_thumb *inst_cream = (bl_2_thumb *)inst_base->component;

	inst_cream->imm = (tinst & 0x07FF) << 1;
	//DEBUG_LOG(ARM11, "In %s, tinst=0x%x, imm=0x%x\n", __FUNCTION__, tinst, inst_cream->imm);
	inst_base->idx = index;
	inst_base->br	 = DIRECT_BRANCH;
	return inst_base;
}
ARM_INST_PTR INTERPRETER_TRANSLATE(blx_1_thumb)(unsigned int tinst, int index)
{
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(blx_1_thumb));
	blx_1_thumb *inst_cream = (blx_1_thumb *)inst_base->component;

	inst_cream->imm = (tinst & 0x07FF) << 1;
	//DEBUG_LOG(ARM11, "In %s, tinst=0x%x, imm=0x%x\n", __FUNCTION__, tinst, inst_cream->imm);
	inst_cream->instr = tinst;
	inst_base->idx	 = index;
	inst_base->br	 = DIRECT_BRANCH;
	return inst_base;
}

ARM_INST_PTR INTERPRETER_TRANSLATE(uqadd16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("UQADD16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uqadd8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("UQADD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uqaddsubx)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("UQADDSUBX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uqsub16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("UQSUB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uqsub8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("UQSUB8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uqsubaddx)(unsigned int inst, int index) { UNIMPLEMENTED_INSTRUCTION("UQSUBADDX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usad8)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("USAD8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usada8)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("USADA8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usat)(unsigned int inst, int index)      { UNIMPLEMENTED_INSTRUCTION("USAT"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usat16)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("USAT16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usub16)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("USUB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usub8)(unsigned int inst, int index)     { UNIMPLEMENTED_INSTRUCTION("USUB8"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(usubaddx)(unsigned int inst, int index)  { UNIMPLEMENTED_INSTRUCTION("USUBADDX"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uxtab16)(unsigned int inst, int index)   { UNIMPLEMENTED_INSTRUCTION("UXTAB16"); }
ARM_INST_PTR INTERPRETER_TRANSLATE(uxtb16)(unsigned int inst, int index)    { UNIMPLEMENTED_INSTRUCTION("UXTB16"); }



/* Floating point VFPv3 structures and instructions */

#define VFP_INTERPRETER_STRUCT
#include "core/arm/skyeye_common/vfp/vfpinstr.cpp"
#undef VFP_INTERPRETER_STRUCT
 
#define VFP_INTERPRETER_TRANS
#include "core/arm/skyeye_common/vfp/vfpinstr.cpp"
#undef VFP_INTERPRETER_TRANS



typedef ARM_INST_PTR (*transop_fp_t)(unsigned int, int);

const transop_fp_t arm_instruction_trans[] = {
	#define VFP_INTERPRETER_TABLE
	#include "core/arm/skyeye_common/vfp/vfpinstr.cpp"
	#undef VFP_INTERPRETER_TABLE
	INTERPRETER_TRANSLATE(srs),
	INTERPRETER_TRANSLATE(rfe),
	INTERPRETER_TRANSLATE(bkpt),
	INTERPRETER_TRANSLATE(blx),
	INTERPRETER_TRANSLATE(cps),
	INTERPRETER_TRANSLATE(pld),
	INTERPRETER_TRANSLATE(setend),
	INTERPRETER_TRANSLATE(clrex),
	INTERPRETER_TRANSLATE(rev16),
	INTERPRETER_TRANSLATE(usad8),
	INTERPRETER_TRANSLATE(sxtb),
	INTERPRETER_TRANSLATE(uxtb),
	INTERPRETER_TRANSLATE(sxth),
	INTERPRETER_TRANSLATE(sxtb16),
	INTERPRETER_TRANSLATE(uxth),
	INTERPRETER_TRANSLATE(uxtb16),
	INTERPRETER_TRANSLATE(cpy),
	INTERPRETER_TRANSLATE(uxtab),
	INTERPRETER_TRANSLATE(ssub8),
	INTERPRETER_TRANSLATE(shsub8),
	INTERPRETER_TRANSLATE(ssubaddx),
	INTERPRETER_TRANSLATE(strex),
	INTERPRETER_TRANSLATE(strexb),
	INTERPRETER_TRANSLATE(swp),
	INTERPRETER_TRANSLATE(swpb),
	INTERPRETER_TRANSLATE(ssub16),
	INTERPRETER_TRANSLATE(ssat16),
	INTERPRETER_TRANSLATE(shsubaddx),
	INTERPRETER_TRANSLATE(qsubaddx),
	INTERPRETER_TRANSLATE(shaddsubx),
	INTERPRETER_TRANSLATE(shadd8),
	INTERPRETER_TRANSLATE(shadd16),
	INTERPRETER_TRANSLATE(sel),
	INTERPRETER_TRANSLATE(saddsubx),
	INTERPRETER_TRANSLATE(sadd8),
	INTERPRETER_TRANSLATE(sadd16),
	INTERPRETER_TRANSLATE(shsub16),
	INTERPRETER_TRANSLATE(umaal),
	INTERPRETER_TRANSLATE(uxtab16),
	INTERPRETER_TRANSLATE(usubaddx),
	INTERPRETER_TRANSLATE(usub8),
	INTERPRETER_TRANSLATE(usub16),
	INTERPRETER_TRANSLATE(usat16),
	INTERPRETER_TRANSLATE(usada8),
	INTERPRETER_TRANSLATE(uqsubaddx),
	INTERPRETER_TRANSLATE(uqsub8),
	INTERPRETER_TRANSLATE(uqsub16),
	INTERPRETER_TRANSLATE(uqaddsubx),
	INTERPRETER_TRANSLATE(uqadd8),
	INTERPRETER_TRANSLATE(uqadd16),
	INTERPRETER_TRANSLATE(sxtab),
	INTERPRETER_TRANSLATE(uhsubaddx),
	INTERPRETER_TRANSLATE(uhsub8),
	INTERPRETER_TRANSLATE(uhsub16),
	INTERPRETER_TRANSLATE(uhaddsubx),
	INTERPRETER_TRANSLATE(uhadd8),
	INTERPRETER_TRANSLATE(uhadd16),
	INTERPRETER_TRANSLATE(uaddsubx),
	INTERPRETER_TRANSLATE(uadd8),
	INTERPRETER_TRANSLATE(uadd16),
	INTERPRETER_TRANSLATE(sxtah),
	INTERPRETER_TRANSLATE(sxtab16),
	INTERPRETER_TRANSLATE(qadd8),
	INTERPRETER_TRANSLATE(bxj),
	INTERPRETER_TRANSLATE(clz),
	INTERPRETER_TRANSLATE(uxtah),
	INTERPRETER_TRANSLATE(bx),
	INTERPRETER_TRANSLATE(rev),
	INTERPRETER_TRANSLATE(blx),
	INTERPRETER_TRANSLATE(revsh),
	INTERPRETER_TRANSLATE(qadd),
	INTERPRETER_TRANSLATE(qadd16),
	INTERPRETER_TRANSLATE(qaddsubx),
	INTERPRETER_TRANSLATE(ldrex),
	INTERPRETER_TRANSLATE(qdadd),
	INTERPRETER_TRANSLATE(qdsub),
	INTERPRETER_TRANSLATE(qsub),
	INTERPRETER_TRANSLATE(ldrexb),
	INTERPRETER_TRANSLATE(qsub8),
	INTERPRETER_TRANSLATE(qsub16),
	INTERPRETER_TRANSLATE(smuad),
	INTERPRETER_TRANSLATE(smmul),
	INTERPRETER_TRANSLATE(smusd),
	INTERPRETER_TRANSLATE(smlsd),
	INTERPRETER_TRANSLATE(smlsld),
	INTERPRETER_TRANSLATE(smmla),
	INTERPRETER_TRANSLATE(smmls),
	INTERPRETER_TRANSLATE(smlald),
	INTERPRETER_TRANSLATE(smlad),
	INTERPRETER_TRANSLATE(smlaw),
	INTERPRETER_TRANSLATE(smulw),
	INTERPRETER_TRANSLATE(pkhtb),
	INTERPRETER_TRANSLATE(pkhbt),
	INTERPRETER_TRANSLATE(smul),
	INTERPRETER_TRANSLATE(smlalxy),
	INTERPRETER_TRANSLATE(smla),
	INTERPRETER_TRANSLATE(mcrr),
	INTERPRETER_TRANSLATE(mrrc),
	INTERPRETER_TRANSLATE(cmp),
	INTERPRETER_TRANSLATE(tst),
	INTERPRETER_TRANSLATE(teq),
	INTERPRETER_TRANSLATE(cmn),
	INTERPRETER_TRANSLATE(smull),
	INTERPRETER_TRANSLATE(umull),
	INTERPRETER_TRANSLATE(umlal),
	INTERPRETER_TRANSLATE(smlal),
	INTERPRETER_TRANSLATE(mul),
	INTERPRETER_TRANSLATE(mla),
	INTERPRETER_TRANSLATE(ssat),
	INTERPRETER_TRANSLATE(usat),
	INTERPRETER_TRANSLATE(mrs),
	INTERPRETER_TRANSLATE(msr),
	INTERPRETER_TRANSLATE(and),
	INTERPRETER_TRANSLATE(bic),
	INTERPRETER_TRANSLATE(ldm),
	INTERPRETER_TRANSLATE(eor),
	INTERPRETER_TRANSLATE(add),
	INTERPRETER_TRANSLATE(rsb),
	INTERPRETER_TRANSLATE(rsc),
	INTERPRETER_TRANSLATE(sbc),
	INTERPRETER_TRANSLATE(adc),
	INTERPRETER_TRANSLATE(sub),
	INTERPRETER_TRANSLATE(orr),
	INTERPRETER_TRANSLATE(mvn),
	INTERPRETER_TRANSLATE(mov),
	INTERPRETER_TRANSLATE(stm),
	INTERPRETER_TRANSLATE(ldm),
	INTERPRETER_TRANSLATE(ldrsh),
	INTERPRETER_TRANSLATE(stm),
	INTERPRETER_TRANSLATE(ldm),
	INTERPRETER_TRANSLATE(ldrsb),
	INTERPRETER_TRANSLATE(strd),
	INTERPRETER_TRANSLATE(ldrh),
	INTERPRETER_TRANSLATE(strh),
	INTERPRETER_TRANSLATE(ldrd),
	INTERPRETER_TRANSLATE(strt),
	INTERPRETER_TRANSLATE(strbt),
	INTERPRETER_TRANSLATE(ldrbt),
	INTERPRETER_TRANSLATE(ldrt),
	INTERPRETER_TRANSLATE(mrc),
	INTERPRETER_TRANSLATE(mcr),
	INTERPRETER_TRANSLATE(msr),
	INTERPRETER_TRANSLATE(ldrb),
	INTERPRETER_TRANSLATE(strb),
	INTERPRETER_TRANSLATE(ldr),
	INTERPRETER_TRANSLATE(ldrcond),
	INTERPRETER_TRANSLATE(str),
	INTERPRETER_TRANSLATE(cdp),
	INTERPRETER_TRANSLATE(stc),
	INTERPRETER_TRANSLATE(ldc),
	INTERPRETER_TRANSLATE(swi),
	INTERPRETER_TRANSLATE(bbl),
	/* All the thumb instructions should be placed the end of table */
	INTERPRETER_TRANSLATE(b_2_thumb), 
	INTERPRETER_TRANSLATE(b_cond_thumb), 
	INTERPRETER_TRANSLATE(bl_1_thumb), 
	INTERPRETER_TRANSLATE(bl_2_thumb),
	INTERPRETER_TRANSLATE(blx_1_thumb)
};

typedef std::unordered_map<u32, int> bb_map;
bb_map CreamCache;

//#define USE_DUMMY_CACHE

#ifdef USE_DUMMY_CACHE
unsigned int DummyCache[0x100000];
#endif

void insert_bb(unsigned int addr, int start)
{
#ifdef USE_DUMMY_CACHE
	DummyCache[addr] = start;
#else
	CreamCache[addr] = start;
#endif
}

#define TRANS_THRESHOLD                 65000
int find_bb(unsigned int addr, int &start)
{
	int ret = -1;
#ifdef USE_DUMMY_CACHE
	start = DummyCache[addr];
	if (start) {
		ret = 0;
	} else
		ret = -1;
#else
	bb_map::const_iterator it = CreamCache.find(addr);
	if (it != CreamCache.end()) {
		start = static_cast<int>(it->second);
		ret = 0;
#if HYBRID_MODE
#if PROFILE
#else
		/* increase the bb counter */
		if(get_bb_prof(cpu, addr, 1) == TRANS_THRESHOLD){
			push_to_compiled(cpu, addr);
		}
#endif
#endif
	} else {
		ret = -1;
	}
#endif
	return ret;
}


enum {
	FETCH_SUCCESS,
	FETCH_FAILURE
};
static tdstate decode_thumb_instr(arm_processor *cpu, uint32_t inst, addr_t addr, uint32_t *arm_inst, uint32_t* inst_size, ARM_INST_PTR* ptr_inst_base){
	/* Check if in Thumb mode.  */
	tdstate ret;
	ret = thumb_translate (addr, inst, arm_inst, inst_size);
	if(ret == t_branch){
		/* FIXME, endian should be judged */
		uint32 tinstr;
		if((addr & 0x3) != 0)
			tinstr = inst >> 16;
		else
			tinstr = inst & 0xFFFF;

		//tinstr = inst & 0xFFFF;
		int inst_index;
		/* table_length */
		int table_length = sizeof(arm_instruction_trans) / sizeof(transop_fp_t);

		switch((tinstr & 0xF800) >> 11){
		/* we will translate the thumb instruction directly here */
		/* we will translate the thumb instruction directly here */
		case 26:
		case 27:
			if (((tinstr & 0x0F00) != 0x0E00) && ((tinstr & 0x0F00) != 0x0F00)){
				uint32 cond = (tinstr & 0x0F00) >> 8;
				inst_index = table_length - 4;
				//DEBUG_LOG(ARM11, "In %s, tinstr=0x%x, blx 1 thumb index=%d\n", __FUNCTION__, tinstr, inst_index);
				*ptr_inst_base = arm_instruction_trans[inst_index](tinstr, inst_index);
			}
			else{
			/* something wrong */
				LOG_ERROR(Core_ARM11, "thumb decoder error");
			}
			break;
		case 28:
			/* Branch 2, unconditional branch */
			inst_index = table_length - 5;
			//DEBUG_LOG(ARM11, "In %s, tinstr=0x%x, blx 1 thumb index=%d\n", __FUNCTION__, tinstr, inst_index);
			*ptr_inst_base = arm_instruction_trans[inst_index](tinstr, inst_index);
			break;

		case 8:
		case 29:
			/* For BLX 1 thumb instruction*/
			inst_index = table_length - 1;
			//DEBUG_LOG(ARM11, "In %s, tinstr=0x%x, blx 1 thumb index=%d, pc=0x%x\n", __FUNCTION__, tinstr, inst_index, cpu->translate_pc);
			*ptr_inst_base = arm_instruction_trans[inst_index](tinstr, inst_index);
			break;
		case 30:
			/* For BL 1 thumb instruction*/
			inst_index = table_length - 3;
			//DEBUG_LOG(ARM11, "In %s, tinstr=0x%x, bl 1 thumb index=%d, pc=0x%x\n", __FUNCTION__, tinstr, inst_index, cpu->translate_pc);
			*ptr_inst_base = arm_instruction_trans[inst_index](tinstr, inst_index);
			break;
		case 31:
			/* For BL 2 thumb instruction*/
			inst_index = table_length - 2;
			//DEBUG_LOG(ARM11, "In %s, tinstr=0x%x, bl 2 thumb index=%d, px=0x%x\n", __FUNCTION__, tinstr, inst_index, cpu->translate_pc);
			*ptr_inst_base = arm_instruction_trans[inst_index](tinstr, inst_index);
			break;
		default:
			ret = t_undefined;
			break;
		}
	}
	return ret;
}

#if 0
int FetchInst(cpu_t *core, unsigned int &inst)
{
	//arm_processor *cpu = (arm_processor *)get_cast_conf_obj(core->cpu_data, "arm_core_t");
	arm_processor *cpu = (arm_processor *)(core->cpu_data->obj);
//	fault_t fault = interpreter_read_memory(cpu->translate_pc, inst, 32);
	fault_t fault = interpreter_fetch(core, cpu->translate_pc, inst, 32);
	if (!core->is_user_mode) {
		if (fault) {
			cpu->abortSig = true;
			cpu->Aborted = ARMul_PrefetchAbortV;
			cpu->AbortAddr = cpu->translate_pc;
			cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)] = fault & 0xff;
			cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = cpu->translate_pc;
			return FETCH_FAILURE;
		}
	}
	return FETCH_SUCCESS;
}
#endif

unsigned int *InstLength;

enum {
	KEEP_GOING,
	FETCH_EXCEPTION
};

typedef struct instruction_set_encoding_item ISEITEM;

extern const ISEITEM arm_instruction[];

vector<uint64_t> code_page_set;

void flush_bb(uint32_t addr)
{
	bb_map::iterator it;
	uint32_t start;

	addr  &= 0xfffff000;
	for (it = CreamCache.begin(); it != CreamCache.end(); ) {
		start = static_cast<uint32_t>(it->first);
		//start = (start >> 12) << 12;
		start &= 0xfffff000;
		if (start == addr) {
			//DEBUG_LOG(ARM11, "[ERASE][0x%08x]\n", static_cast<int>(it->first));
			CreamCache.erase(it++);
		} else
			++it;
	}

	//DEBUG_LOG(ARM11, "flush bb @ %x\n", addr);
}

//static uint32_t get_bank_addr(void *addr)
//{
//	uint64_t address = (uint64_t)addr;
//	uint64_t bank0 = get_dma_addr(BANK0_START);
//	if ((address >= bank0) && (address < (bank0 + BANK0_SIZE))) {
//		//DEBUG_LOG(ARM11, "1.addr is %llx\n", addr);
//		return ((uint64_t)addr - bank0) + BANK0_START;
//	}
//	return 0;
//}

/* shenoubang add win32 2012-6-12 */
//#ifndef __WIN32__
//static void flush_code_cache(int signal_number, siginfo_t *si, void *unused)
//{
//	DEBUG_LOG(ARM11, "in %s, addr=0x%llx\n", __FUNCTION__, si->si_addr);
//	uint64_t addr = (uint64_t)si->si_addr;
//	addr = (addr >> 12) << 12;
//	skyeye_backtrace();
//	#if 0
//	if (addr == 0) {
//		return;
//	}
//	const vector<uint64_t>::iterator it = find(code_page_set.begin(), 
//						   code_page_set.end(),
//						   (uint64_t)addr);
//	if (it != code_page_set.end()) {
//		code_page_set.erase(it);
//	}
//	mprotect((void *)addr, 4096, PROT_READ | PROT_WRITE);
//	//DEBUG_LOG(ARM11, "[flush][ADDR:0x%08llx]\n", addr);
//	uint32_t phys_addr = get_bank_addr((void *)addr);
////	DEBUG_LOG(ARM11, "[PHYSICAL][ADDR:0x%08llx]\n", phys_addr);
//	flush_bb(phys_addr);
//	flush_bb(phys_addr + 4096);
//#if HYBRID_MODE
//	/* flush the translated BB of dyncom */
//      	clear_translated_cache(phys_addr); 
//#endif
//	#endif
//}
//#endif /* shenoubang */

//void protect_code_page(uint32_t addr)
//{
//	void *mem_ptr = (void *)get_dma_addr(addr);
//	mem_ptr = (void *)((long long int)mem_ptr & 0xfffffffffffff000LL);
//
//	const vector<uint64_t>::iterator it = find(code_page_set.begin(), 
//						   code_page_set.end(),
//						   (uint64_t)mem_ptr);
//	if (it != code_page_set.end()) {
//		return;
//	}
//	//DEBUG_LOG(ARM11, "[mprotect][ADDR:0x%08llx]\n", mem_ptr);
//	/* shenoubang add win32 2012-6-12 */
//#ifndef __WIN32__
//	struct sigaction sa;
//
//	memset(&sa, 0, sizeof(sa));
//	sa.sa_flags = SA_RESTART | SA_SIGINFO;
//	sa.sa_sigaction = &flush_code_cache;
//	sigaction(SIGSEGV, &sa, NULL);
//
//	//mprotect(mem_ptr, 4096, PROT_READ);
//
//	code_page_set.push_back((uint64_t)mem_ptr);
//#endif /* shenoubang */
//}



int InterpreterTranslate(arm_processor *cpu, int &bb_start, addr_t addr)
{
	/* Decode instruction, get index */
	/* Allocate memory and init InsCream */
	/* Go on next, until terminal instruction */
	/* Save start addr of basicblock in CreamCache */
	//arm_processor *cpu = (arm_processor *)get_cast_conf_obj(core->cpu_data, "arm_core_t");
	//arm_processor *cpu = (arm_processor *)(core->cpu_data->obj);
	ARM_INST_PTR inst_base = NULL;
	unsigned int inst, inst_size = 4;
	int idx;
	int ret = NON_BRANCH;
	int thumb = 0;
	/* instruction size of basic block */
	int size = 0;
	/* (R15 - 8) ? */
	//cpu->translate_pc = cpu->Reg[15];
	bb_start = top;

	if (cpu->TFlag)
		thumb = THUMB;

	addr_t phys_addr;
	addr_t pc_start;
	fault_t fault = NO_FAULT;
	//fault = check_address_validity(cpu, addr, &phys_addr, 1, INSN_TLB);
	fault = check_address_validity(cpu, addr, &phys_addr, 1);
	if(fault != NO_FAULT){
		cpu->abortSig = true;
		cpu->Aborted = ARMul_PrefetchAbortV;
		cpu->AbortAddr = addr;
		cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)] = fault & 0xff;
		cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = addr;
		return FETCH_EXCEPTION;
	}
	pc_start = phys_addr;
	//phys_addr = get_dma_addr(phys_addr);
	while(ret == NON_BRANCH) {
		/* shenoubang add win32 2012-6-14 */
#ifdef __WIN32__
               mem_bank_t* bank;
               if (bank = bank_ptr(addr)) {
                       bank->bank_read(32, phys_addr, &inst);
               }
               else {
                       LOG_ERROR(Core_ARM11, "SKYEYE: Read physical addr 0x%x error!!\n", phys_addr);
                       return FETCH_FAILURE;
               }
#else
        inst = Memory::Read32(phys_addr & 0xFFFFFFFC);//*(uint32_t *)(phys_addr & 0xFFFFFFFC);
#endif
		//or_tag(core, phys_addr,  TAG_FAST_INTERP);

		/*if (ret == FETCH_FAILURE) {
			return FETCH_EXCEPTION;
		}*/

		size ++;
		/* If we are in thumb instruction, we will translate one thumb to one corresponding arm instruction */
		if (cpu->TFlag){
		//if(cpu->Cpsr & (1 << THUMB_BIT)){
			uint32_t arm_inst;
			tdstate state;
			state = decode_thumb_instr(cpu, inst, phys_addr, &arm_inst, &inst_size, &inst_base);
			//or_tag(core, phys_addr, TAG_THUMB);
			//DEBUG_LOG(ARM11, "In thumb state, arm_inst=0x%x, inst_size=0x%x, pc=0x%x\n", arm_inst, inst_size, cpu->translate_pc);
			/* we have translated the branch instruction of thumb in thumb decoder */
			if(state == t_branch){
				goto translated;
			}
			inst = arm_inst;
		}

		ret = decode_arm_instr(inst, &idx);
		if (ret == DECODE_FAILURE) {
			LOG_ERROR(Core_ARM11, "Decode failure.\tPC : [0x%x]\tInstruction : [%x]", phys_addr, inst);
			LOG_ERROR(Core_ARM11, "cpsr=0x%x, cpu->TFlag=%d, r15=0x%x", cpu->Cpsr, cpu->TFlag, cpu->Reg[15]);
			CITRA_IGNORE_EXIT(-1);
		}
//		DEBUG_LOG(ARM11, "PC : [0x%x] INST : %s\n", cpu->translate_pc, arm_instruction[idx].name);
		inst_base = arm_instruction_trans[idx](inst, idx);
//		DEBUG_LOG(ARM11, "translated @ %x INST : %x\n", cpu->translate_pc, inst);
//		DEBUG_LOG(ARM11, "inst size is %d\n", InstLength[idx]);
translated:
		phys_addr += inst_size;

		if ((phys_addr & 0xfff) == 0) {
			inst_base->br = END_OF_PAGE;
		}
		ret = inst_base->br;
	};

	//DEBUG_LOG(ARM11, "In %s,insert_bb pc=0x%x, TFlag=0x%x\n", __FUNCTION__, pc_start, cpu->TFlag);
	insert_bb(pc_start, bb_start);
	return KEEP_GOING;
}

#define LOG_IN_CLR	skyeye_printf_in_color

int cmp(const void *x, const void *y)
{
	return *(unsigned long long int*)x - *(unsigned long long int *)y;
}

void InterpreterInitInstLength(unsigned long long int *ptr, size_t size)
{
	int array_size = size / sizeof(void *);
	unsigned long long int *InstLabel = new unsigned long long int[array_size];
	memcpy(InstLabel, ptr, size);
	qsort(InstLabel, array_size, sizeof(void *), cmp);
	InstLength = new unsigned int[array_size - 4];
	for (int i = 0; i < array_size - 4; i ++) {
		for (int j = 0; j < array_size; j ++) {
			if (ptr[i] == InstLabel[j]) {
				InstLength[i] = InstLabel[j + 1] - InstLabel[j];
				break;
			}
		}
	}
	for (int i = 0; i < array_size - 4; i ++)
		LOG_DEBUG(Core_ARM11, "[%d]:%d", i, InstLength[i]);
}

int clz(unsigned int x)
{
	int n;
	if (x == 0) return (32);
	n = 1;
	if ((x >> 16) == 0) { n = n + 16; x = x << 16;}
	if ((x >> 24) == 0) { n = n +  8; x = x <<  8;}
	if ((x >> 28) == 0) { n = n +  4; x = x <<  4;}
	if ((x >> 30) == 0) { n = n +  2; x = x <<  2;}
	n = n - (x >> 31);
	return n;
}

unsigned arm_dyncom_SWI (ARMul_State * state, ARMword number);

static bool InAPrivilegedMode(arm_core_t *core)
{
	return (core->Mode != USER32MODE);
}

/* r15 = r15 + 8 */
unsigned InterpreterMainLoop(ARMul_State* state)
{
	#define CRn				inst_cream->crn
	#define OPCODE_2			inst_cream->opcode_2
	#define CRm				inst_cream->crm
	#define CP15_REG(n)			cpu->CP15[CP15(n)]
	#define RD				cpu->Reg[inst_cream->Rd]
	#define RN				cpu->Reg[inst_cream->Rn]
	#define RM				cpu->Reg[inst_cream->Rm]
	#define RS				cpu->Reg[inst_cream->Rs]
	#define RDHI				cpu->Reg[inst_cream->RdHi]
	#define RDLO				cpu->Reg[inst_cream->RdLo]
	#define LINK_RTN_ADDR			(cpu->Reg[14] = cpu->Reg[15] + 4)
	#define SET_PC				(cpu->Reg[15] = cpu->Reg[15] + 8 + inst_cream->signed_immed_24)
	#define SHIFTER_OPERAND			inst_cream->shtop_func(cpu, inst_cream->shifter_operand)

	#if ENABLE_ICOUNTER
	#define INC_ICOUNTER			cpu->icounter++;                                                   \
						if(cpu->Reg[15] > 0xc0000000) 					\
							cpu->kernel_icounter++;
						/*if (debug_function(core))                                          \
							if (core->check_int_flag)                                  \
								goto END*/
						//LOG_TRACE(Core_ARM11, "icounter is %llx pc is %x\n", cpu->icounter, cpu->Reg[15])
	#else
	#define INC_ICOUNTER			;                                                   
	#endif

	#define FETCH_INST			if (inst_base->br != NON_BRANCH)                                   \
							goto DISPATCH;                                             \
						inst_base = (arm_inst *)&inst_buf[ptr]                             
#define INC_PC(l)			ptr += sizeof(arm_inst) + l

// GCC and Clang have a C++ extension to support a lookup table of labels. Otherwise, fallback to a
// clunky switch statement.
#if defined __GNUC__ || defined __clang__
#define GOTO_NEXT_INST \
    if (num_instrs >= cpu->NumInstrsToExecute) goto END; \
    num_instrs++; \
    goto *InstLabel[inst_base->idx]
#else
#define GOTO_NEXT_INST \
    if (num_instrs >= cpu->NumInstrsToExecute) goto END; \
    num_instrs++; \
    switch(inst_base->idx) { \
    case 0: goto VMLA_INST; \
    case 1: goto VMLS_INST; \
    case 2: goto VNMLA_INST; \
    case 3: goto VNMLA_INST; \
    case 4: goto VNMLS_INST; \
    case 5: goto VNMUL_INST; \
    case 6: goto VMUL_INST; \
    case 7: goto VADD_INST; \
    case 8: goto VSUB_INST; \
    case 9: goto VDIV_INST; \
    case 10: goto VMOVI_INST; \
    case 11: goto VMOVR_INST; \
    case 12: goto VABS_INST; \
    case 13: goto VNEG_INST; \
    case 14: goto VSQRT_INST; \
    case 15: goto VCMP_INST; \
    case 16: goto VCMP2_INST; \
    case 17: goto VCVTBDS_INST; \
    case 18: goto VCVTBFF_INST; \
    case 19: goto VCVTBFI_INST; \
    case 20: goto VMOVBRS_INST; \
    case 21: goto VMSR_INST; \
    case 22: goto VMOVBRC_INST; \
    case 23: goto VMRS_INST; \
    case 24: goto VMOVBCR_INST; \
    case 25: goto VMOVBRRSS_INST; \
    case 26: goto VMOVBRRD_INST; \
    case 27: goto VSTR_INST; \
    case 28: goto VPUSH_INST; \
    case 29: goto VSTM_INST; \
    case 30: goto VPOP_INST; \
    case 31: goto VLDR_INST; \
    case 32: goto VLDM_INST ; \
    case 33: goto SRS_INST; \
    case 34: goto RFE_INST; \
    case 35: goto BKPT_INST; \
    case 36: goto BLX_INST; \
    case 37: goto CPS_INST; \
    case 38: goto PLD_INST; \
    case 39: goto SETEND_INST; \
    case 40: goto CLREX_INST; \
    case 41: goto REV16_INST; \
    case 42: goto USAD8_INST; \
    case 43: goto SXTB_INST; \
    case 44: goto UXTB_INST; \
    case 45: goto SXTH_INST; \
    case 46: goto SXTB16_INST; \
    case 47: goto UXTH_INST; \
    case 48: goto UXTB16_INST; \
    case 49: goto CPY_INST; \
    case 50: goto UXTAB_INST; \
    case 51: goto SSUB8_INST; \
    case 52: goto SHSUB8_INST; \
    case 53: goto SSUBADDX_INST; \
    case 54: goto STREX_INST; \
    case 55: goto STREXB_INST; \
    case 56: goto SWP_INST; \
    case 57: goto SWPB_INST; \
    case 58: goto SSUB16_INST; \
    case 59: goto SSAT16_INST; \
    case 60: goto SHSUBADDX_INST; \
    case 61: goto QSUBADDX_INST; \
    case 62: goto SHADDSUBX_INST; \
    case 63: goto SHADD8_INST; \
    case 64: goto SHADD16_INST; \
    case 65: goto SEL_INST; \
    case 66: goto SADDSUBX_INST; \
    case 67: goto SADD8_INST; \
    case 68: goto SADD16_INST; \
    case 69: goto SHSUB16_INST; \
    case 70: goto UMAAL_INST; \
    case 71: goto UXTAB16_INST; \
    case 72: goto USUBADDX_INST; \
    case 73: goto USUB8_INST; \
    case 74: goto USUB16_INST; \
    case 75: goto USAT16_INST; \
    case 76: goto USADA8_INST; \
    case 77: goto UQSUBADDX_INST; \
    case 78: goto UQSUB8_INST; \
    case 79: goto UQSUB16_INST; \
    case 80: goto UQADDSUBX_INST; \
    case 81: goto UQADD8_INST; \
    case 82: goto UQADD16_INST; \
    case 83: goto SXTAB_INST; \
    case 84: goto UHSUBADDX_INST; \
    case 85: goto UHSUB8_INST; \
    case 86: goto UHSUB16_INST; \
    case 87: goto UHADDSUBX_INST; \
    case 88: goto UHADD8_INST; \
    case 89: goto UHADD16_INST; \
    case 90: goto UADDSUBX_INST; \
    case 91: goto UADD8_INST; \
    case 92: goto UADD16_INST; \
    case 93: goto SXTAH_INST; \
    case 94: goto SXTAB16_INST; \
    case 95: goto QADD8_INST; \
    case 96: goto BXJ_INST; \
    case 97: goto CLZ_INST; \
    case 98: goto UXTAH_INST; \
    case 99: goto BX_INST; \
    case 100: goto REV_INST; \
    case 101: goto BLX_INST; \
    case 102: goto REVSH_INST; \
    case 103: goto QADD_INST; \
    case 104: goto QADD16_INST; \
    case 105: goto QADDSUBX_INST; \
    case 106: goto LDREX_INST; \
    case 107: goto QDADD_INST; \
    case 108: goto QDSUB_INST; \
    case 109: goto QSUB_INST; \
    case 110: goto LDREXB_INST; \
    case 111: goto QSUB8_INST; \
    case 112: goto QSUB16_INST; \
    case 113: goto SMUAD_INST; \
    case 114: goto SMMUL_INST; \
    case 115: goto SMUSD_INST; \
    case 116: goto SMLSD_INST; \
    case 117: goto SMLSLD_INST; \
    case 118: goto SMMLA_INST; \
    case 119: goto SMMLS_INST; \
    case 120: goto SMLALD_INST; \
    case 121: goto SMLAD_INST; \
    case 122: goto SMLAW_INST; \
    case 123: goto SMULW_INST; \
    case 124: goto PKHTB_INST; \
    case 125: goto PKHBT_INST; \
    case 126: goto SMUL_INST; \
    case 127: goto SMLAL_INST; \
    case 128: goto SMLA_INST; \
    case 129: goto MCRR_INST; \
    case 130: goto MRRC_INST; \
    case 131: goto CMP_INST; \
    case 132: goto TST_INST; \
    case 133: goto TEQ_INST; \
    case 134: goto CMN_INST; \
    case 135: goto SMULL_INST; \
    case 136: goto UMULL_INST; \
    case 137: goto UMLAL_INST; \
    case 138: goto SMLAL_INST; \
    case 139: goto MUL_INST; \
    case 140: goto MLA_INST; \
    case 141: goto SSAT_INST; \
    case 142: goto USAT_INST; \
    case 143: goto MRS_INST; \
    case 144: goto MSR_INST; \
    case 145: goto AND_INST; \
    case 146: goto BIC_INST; \
    case 147: goto LDM_INST; \
    case 148: goto EOR_INST; \
    case 149: goto ADD_INST; \
    case 150: goto RSB_INST; \
    case 151: goto RSC_INST; \
    case 152: goto SBC_INST; \
    case 153: goto ADC_INST; \
    case 154: goto SUB_INST; \
    case 155: goto ORR_INST; \
    case 156: goto MVN_INST; \
    case 157: goto MOV_INST; \
    case 158: goto STM_INST; \
    case 159: goto LDM_INST; \
    case 160: goto LDRSH_INST; \
    case 161: goto STM_INST; \
    case 162: goto LDM_INST; \
    case 163: goto LDRSB_INST; \
    case 164: goto STRD_INST; \
    case 165: goto LDRH_INST; \
    case 166: goto STRH_INST; \
    case 167: goto LDRD_INST; \
    case 168: goto STRT_INST; \
    case 169: goto STRBT_INST; \
    case 170: goto LDRBT_INST; \
    case 171: goto LDRT_INST; \
    case 172: goto MRC_INST; \
    case 173: goto MCR_INST; \
    case 174: goto MSR_INST; \
    case 175: goto LDRB_INST; \
    case 176: goto STRB_INST; \
    case 177: goto LDR_INST; \
    case 178: goto LDRCOND_INST ; \
    case 179: goto STR_INST; \
    case 180: goto CDP_INST; \
    case 181: goto STC_INST; \
    case 182: goto LDC_INST; \
    case 183: goto SWI_INST; \
    case 184: goto BBL_INST; \
    case 185: goto B_2_THUMB ; \
    case 186: goto B_COND_THUMB ; \
    case 187: goto BL_1_THUMB ; \
    case 188: goto BL_2_THUMB ; \
    case 189: goto BLX_1_THUMB ; \
    case 190: goto DISPATCH; \
    case 191: goto INIT_INST_LENGTH; \
    case 192: goto END; \
    }
#endif

	#define UPDATE_NFLAG(dst)		(cpu->NFlag = BIT(dst, 31) ? 1 : 0)
	#define UPDATE_ZFLAG(dst)		(cpu->ZFlag = dst ? 0 : 1)
/*	#define UPDATE_CFLAG(dst, lop, rop)	(cpu->CFlag = ((ISNEG(lop) && ISPOS(rop)) ||                        \
								(ISNEG(lop) && ISPOS(dst)) ||                       \
								(ISPOS(rop) && ISPOS(dst)))) */
	#define UPDATE_CFLAG(dst, lop, rop)	(cpu->CFlag = ((dst < lop) || (dst < rop)))
	#define UPDATE_CFLAG_CARRY_FROM_ADD(lop, rop, flag)	(cpu->CFlag = (((uint64_t) lop + (uint64_t) rop + (uint64_t) flag) > 0xffffffff) )
	#define UPDATE_CFLAG_NOT_BORROW_FROM_FLAG(lop, rop, flag) (cpu->CFlag = ((uint64_t) lop >= ((uint64_t) rop + (uint64_t) flag)))
	#define UPDATE_CFLAG_NOT_BORROW_FROM(lop, rop)	(cpu->CFlag = (lop >= rop))
	#define UPDATE_CFLAG_WITH_NOT(dst, lop, rop)	(cpu->CFlag = !(dst < lop))
	#define UPDATE_CFLAG_WITH_SC		cpu->CFlag = cpu->shifter_carry_out
/*	#define UPDATE_CFLAG_WITH_NOT(dst, lop, rop)	cpu->CFlag = !((ISNEG(lop) && ISPOS(rop)) ||                        \
								(ISNEG(lop) && ISPOS(dst)) ||                       \
								(ISPOS(rop) && ISPOS(dst))) */
	#define UPDATE_VFLAG(dst, lop, rop)	(cpu->VFlag = (((lop < 0) && (rop < 0) && (dst >= 0)) ||            \
								((lop >= 0) && (rop) >= 0 && (dst < 0))))
	#define UPDATE_VFLAG_WITH_NOT(dst, lop, rop)	(cpu->VFlag = !(((lop < 0) && (rop < 0) && (dst >= 0)) ||            \
								((lop >= 0) && (rop) >= 0 && (dst < 0))))
	#define UPDATE_VFLAG_OVERFLOW_FROM(dst, lop, rop)	(cpu->VFlag = (((lop ^ rop) & (lop ^ dst)) >> 31))

	#define SAVE_NZCVT			cpu->Cpsr = (cpu->Cpsr & 0x0fffffdf) | \
						(cpu->NFlag << 31)   |                 \
						(cpu->ZFlag << 30)   |                 \
						(cpu->CFlag << 29)   |                 \
						(cpu->VFlag << 28)   |			\
						(cpu->TFlag << 5)
	#define LOAD_NZCVT			cpu->NFlag = (cpu->Cpsr >> 31);   \
						cpu->ZFlag = (cpu->Cpsr >> 30) & 1;   \
						cpu->CFlag = (cpu->Cpsr >> 29) & 1;   \
						cpu->VFlag = (cpu->Cpsr >> 28) & 1;	\
						cpu->TFlag = (cpu->Cpsr >> 5) & 1;

	#define CurrentModeHasSPSR		(cpu->Mode != SYSTEM32MODE) && (cpu->Mode != USER32MODE)
	#define PC				(cpu->Reg[15])
	#define CHECK_EXT_INT    		if (!cpu->NirqSig) {                       \
				                	if (!(cpu->Cpsr & 0x80)) {         \
								goto END;                  \
							}                                  \
						}

	

	//arm_processor *cpu = (arm_processor *)get_cast_conf_obj(core->cpu_data, "arm_core_t");
	arm_processor *cpu = state; //(arm_processor *)(core->cpu_data->obj);

    // GCC and Clang have a C++ extension to support a lookup table of labels. Otherwise, fallback
    // to a clunky switch statement.
#if defined __GNUC__ || defined __clang__
    void *InstLabel[] = {
		#define VFP_INTERPRETER_LABEL
		#include "core/arm/skyeye_common/vfp/vfpinstr.cpp"
		#undef VFP_INTERPRETER_LABEL
		&&SRS_INST,&&RFE_INST,&&BKPT_INST,&&BLX_INST,&&CPS_INST,&&PLD_INST,&&SETEND_INST,&&CLREX_INST,&&REV16_INST,&&USAD8_INST,&&SXTB_INST,
		&&UXTB_INST,&&SXTH_INST,&&SXTB16_INST,&&UXTH_INST,&&UXTB16_INST,&&CPY_INST,&&UXTAB_INST,&&SSUB8_INST,&&SHSUB8_INST,&&SSUBADDX_INST,
		&&STREX_INST,&&STREXB_INST,&&SWP_INST,&&SWPB_INST,&&SSUB16_INST,&&SSAT16_INST,&&SHSUBADDX_INST,&&QSUBADDX_INST,&&SHADDSUBX_INST,
		&&SHADD8_INST,&&SHADD16_INST,&&SEL_INST,&&SADDSUBX_INST,&&SADD8_INST,&&SADD16_INST,&&SHSUB16_INST,&&UMAAL_INST,&&UXTAB16_INST,
		&&USUBADDX_INST,&&USUB8_INST,&&USUB16_INST,&&USAT16_INST,&&USADA8_INST,&&UQSUBADDX_INST,&&UQSUB8_INST,&&UQSUB16_INST,
		&&UQADDSUBX_INST,&&UQADD8_INST,&&UQADD16_INST,&&SXTAB_INST,&&UHSUBADDX_INST,&&UHSUB8_INST,&&UHSUB16_INST,&&UHADDSUBX_INST,&&UHADD8_INST,
		&&UHADD16_INST,&&UADDSUBX_INST,&&UADD8_INST,&&UADD16_INST,&&SXTAH_INST,&&SXTAB16_INST,&&QADD8_INST,&&BXJ_INST,&&CLZ_INST,&&UXTAH_INST,
		&&BX_INST,&&REV_INST,&&BLX_INST,&&REVSH_INST,&&QADD_INST,&&QADD16_INST,&&QADDSUBX_INST,&&LDREX_INST,&&QDADD_INST,&&QDSUB_INST,
		&&QSUB_INST,&&LDREXB_INST,&&QSUB8_INST,&&QSUB16_INST,&&SMUAD_INST,&&SMMUL_INST,&&SMUSD_INST,&&SMLSD_INST,&&SMLSLD_INST,&&SMMLA_INST,
		&&SMMLS_INST,&&SMLALD_INST,&&SMLAD_INST,&&SMLAW_INST,&&SMULW_INST,&&PKHTB_INST,&&PKHBT_INST,&&SMUL_INST,&&SMLAL_INST,&&SMLA_INST,
		&&MCRR_INST,&&MRRC_INST,&&CMP_INST,&&TST_INST,&&TEQ_INST,&&CMN_INST,&&SMULL_INST,&&UMULL_INST,&&UMLAL_INST,&&SMLAL_INST,&&MUL_INST,
		&&MLA_INST,&&SSAT_INST,&&USAT_INST,&&MRS_INST,&&MSR_INST,&&AND_INST,&&BIC_INST,&&LDM_INST,&&EOR_INST,&&ADD_INST,&&RSB_INST,&&RSC_INST,
		&&SBC_INST,&&ADC_INST,&&SUB_INST,&&ORR_INST,&&MVN_INST,&&MOV_INST,&&STM_INST,&&LDM_INST,&&LDRSH_INST,&&STM_INST,&&LDM_INST,&&LDRSB_INST,
		&&STRD_INST,&&LDRH_INST,&&STRH_INST,&&LDRD_INST,&&STRT_INST,&&STRBT_INST,&&LDRBT_INST,&&LDRT_INST,&&MRC_INST,&&MCR_INST,&&MSR_INST,
		&&LDRB_INST,&&STRB_INST,&&LDR_INST,&&LDRCOND_INST, &&STR_INST,&&CDP_INST,&&STC_INST,&&LDC_INST,&&SWI_INST,&&BBL_INST,&&B_2_THUMB, &&B_COND_THUMB, 
		&&BL_1_THUMB, &&BL_2_THUMB, &&BLX_1_THUMB, &&DISPATCH,&&INIT_INST_LENGTH,&&END
		};
#endif
	arm_inst * inst_base;
	unsigned int lop, rop, dst;
	unsigned int addr;
	unsigned int phys_addr;
	unsigned int last_pc = 0;
	unsigned int num_instrs = 0;
	fault_t fault;
	static unsigned int last_physical_base = 0, last_logical_base = 0;
	int ptr;
	bool single_step = (cpu->NumInstrsToExecute == 1);

	LOAD_NZCVT;
	DISPATCH:
	{
		if (!cpu->NirqSig) {
                	if (!(cpu->Cpsr & 0x80)) {
				goto END;
			}
		}

		if (cpu->TFlag) {
			cpu->Reg[15] &= 0xfffffffe;
		} else
			cpu->Reg[15] &= 0xfffffffc;
#if PROFILE
		/* check next instruction address is valid. */
		last_pc = cpu->Reg[15];
#endif
#if USER_MODE_OPT
		phys_addr = cpu->Reg[15];
#else
		{
			if (last_logical_base == (cpu->Reg[15] & 0xfffff000))
			       phys_addr = last_physical_base + (cpu->Reg[15] & 0xfff);
			else {
			       /* check next instruction address is valid. */
			       fault = check_address_validity(cpu, cpu->Reg[15], &phys_addr, 1, INSN_TLB);
			       if (fault) {
				       cpu->abortSig = true;
				       cpu->Aborted = ARMul_PrefetchAbortV;
				       cpu->AbortAddr = cpu->Reg[15];
				       cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)] = fault & 0xff;
				       cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = cpu->Reg[15];
				       goto END;
			       }
			       last_logical_base = cpu->Reg[15] & 0xfffff000;
			       last_physical_base = phys_addr & 0xfffff000;
			}
		}
#if HYBRID_MODE
		/* check if the native code of dyncom is available */
		//fast_map hash_map = core->dyncom_engine->fmap;
		//void * pfunc = NULL;
		//PFUNC(phys_addr);
		//if(pfunc){
		if(is_translated_entry(core, phys_addr)){
			int rc = JIT_RETURN_NOERR;
			//DEBUG_LOG(ARM11, "enter jit icounter is %lld, pc=0x%x\n", core->icounter, cpu->Reg[15]);
			SAVE_NZCVT;
//			resume_timing();
			rc = cpu_run(core);
			LOAD_NZCVT;
			//DEBUG_LOG(ARM11, "out of jit ret is %d icounter is %lld, pc=0x%x\n", rc, core->icounter, cpu->Reg[15]);
			if((rc == JIT_RETURN_FUNCNOTFOUND) || (rc == JIT_RETURN_FUNC_BLANK)){
				/* keep the tflag same with the bit in CPSR */
				//cpu->TFlag = cpu->Cpsr & (1 << THUMB_BIT);
				//cpu->TFlag = cpu->Cpsr & (1 << 5);
				//switch_mode(cpu, cpu->Cpsr & 0x1f);
				//DEBUG_LOG(ARM11, "FUNCTION not found , pc=0x%x\n", cpu->Reg[15]);
			       fault = check_address_validity(cpu, cpu->Reg[15], &phys_addr, 1, INSN_TLB);
			       if (fault) {
				       cpu->abortSig = true;
				       cpu->Aborted = ARMul_PrefetchAbortV;
				       cpu->AbortAddr = cpu->Reg[15];
				       cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)] = fault & 0xff;
				       cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = cpu->Reg[15];
				       goto END;
				}
				last_logical_base = cpu->Reg[15] & 0xfffff000;
				last_physical_base = phys_addr & 0xfffff000;
				core->current_page_phys = last_physical_base;
				core->current_page_effec = last_logical_base;
				//push_to_compiled(core, phys_addr);
			}
			else{
				if((cpu->CP15[CP15(CP15_TLB_FAULT_STATUS)] & 0xf0)){
					//DEBUG_LOG(ARM11, "\n\n###############In %s, fsr=0x%x, fault_addr=0x%x, pc=0x%x\n\n", __FUNCTION__, cpu->CP15[CP15(CP15_FAULT_STATUS)], cpu->CP15[CP15(CP15_FAULT_ADDRESS)], cpu->Reg[15]);
			//core->Reg[15] -= get_instr_size(cpu_dyncom);
					fill_tlb(cpu);
					goto END;
				}
				if (cpu->syscallSig) {
					goto END;
				}
				if (cpu->abortSig) {
					cpu->CP15[CP15_TLB_FAULT_STATUS - CP15_BASE] &= 0xFFFFFFF0;
					goto END;
				}
				if (!cpu->NirqSig) {
					if (!(cpu->Cpsr & 0x80)) {
						goto END;
					}
				}
			
				/* if regular trap */
				cpu->Reg[15] += GET_INST_SIZE(cpu);
				/*uint32_t mode = cpu->Cpsr & 0x1f;
				if ((mode != cpu->Mode) && (!is_user_mode(core))) {
					switch_mode(cpu, mode);
					return 1;
				}*/

				goto END;
			}
			//phys_addr = cpu->Reg[15];
		}
		else{
			if (last_logical_base == (cpu->Reg[15] & 0xfffff000))
			       phys_addr = last_physical_base + (cpu->Reg[15] & 0xfff);
			else {
			       /* check next instruction address is valid. */
			       fault = check_address_validity(cpu, cpu->Reg[15], &phys_addr, 1, INSN_TLB);
			       if (fault) {
				       cpu->abortSig = true;
				       cpu->Aborted = ARMul_PrefetchAbortV;
				       cpu->AbortAddr = cpu->Reg[15];
				       cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)] = fault & 0xff;
				       cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = cpu->Reg[15];
				       goto END;
			       }
			       last_logical_base = cpu->Reg[15] & 0xfffff000;
			       last_physical_base = phys_addr & 0xfffff000;
			}
		}
#endif /* #if HYBRID_MODE */
#endif /* #if USER_MODE_OPT */
		if (true){//if(is_fast_interp_code(core, phys_addr)){
			if (find_bb(phys_addr, ptr) == -1)
				if (InterpreterTranslate(cpu, ptr, cpu->Reg[15]) == FETCH_EXCEPTION)
					goto END;
		}
		else{
            if (InterpreterTranslate(cpu, ptr, cpu->Reg[15]) == FETCH_EXCEPTION)
				goto END;
		}
#if PROFILE
		resume_timing();
#endif
		inst_base = (arm_inst *)&inst_buf[ptr];
		GOTO_NEXT_INST;
	}
	ADC_INST:
	{
		INC_ICOUNTER;
		adc_inst *inst_cream = (adc_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			unsigned int sht_op = SHIFTER_OPERAND;
			rop = SHIFTER_OPERAND + cpu->CFlag;
			RD = dst = lop + rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_CARRY_FROM_ADD(lop, sht_op, cpu->CFlag);
				UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(adc_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(adc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	ADD_INST:
	{
		INC_ICOUNTER;
		add_inst *inst_cream = (add_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			if (inst_cream->Rn == 15) {
				lop += 2 * GET_INST_SIZE(cpu);
			}
			rop = SHIFTER_OPERAND;
			RD = dst = lop + rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr*/
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Cpsr & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG(dst, lop, rop);
				UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(add_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(add_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	AND_INST:
	{
		INC_ICOUNTER;
		and_inst *inst_cream = (and_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			rop = SHIFTER_OPERAND;
			RD = dst = lop & rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr*/
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Cpsr & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
				//UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(and_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(and_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	BBL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			bbl_inst *inst_cream = (bbl_inst *)inst_base->component;
			if (inst_cream->L) {
				LINK_RTN_ADDR;
			}
			SET_PC;
			INC_PC(sizeof(bbl_inst));
			goto DISPATCH;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(bbl_inst));
		goto DISPATCH;
	}
	BIC_INST:
	{
		INC_ICOUNTER;
		bic_inst *inst_cream = (bic_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			if (inst_cream->Rn == 15) {
				lop += 2 * GET_INST_SIZE(cpu);
			}
			rop = SHIFTER_OPERAND;
//			RD = dst = lop & (rop ^ 0xffffffff);
			RD = dst = lop & (~rop);
			if ((inst_cream->S) && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(bic_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(bic_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	BKPT_INST:
	BLX_INST:
	{
		INC_ICOUNTER;
		blx_inst *inst_cream = (blx_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int inst = inst_cream->inst;
			if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
				//LINK_RTN_ADDR;
				cpu->Reg[14] = (cpu->Reg[15] + GET_INST_SIZE(cpu));
				if(cpu->TFlag)
					cpu->Reg[14] |= 0x1;
				cpu->Reg[15] = cpu->Reg[inst_cream->val.Rm] & 0xfffffffe;
				cpu->TFlag = cpu->Reg[inst_cream->val.Rm] & 0x1;
				//cpu->Reg[15] = cpu->Reg[BITS(inst, 0, 3)] & 0xfffffffe;
				//cpu->TFlag = cpu->Reg[BITS(inst, 0, 3)] & 0x1;
			} else {
				cpu->Reg[14] = (cpu->Reg[15] + GET_INST_SIZE(cpu));
				cpu->TFlag = 0x1;
				int signed_int = inst_cream->val.signed_immed_24;
				signed_int = (signed_int) & 0x800000 ? (0x3F000000 | signed_int) : signed_int;
				signed_int = signed_int << 2;
			//	cpu->Reg[15] = cpu->Reg[15] + 2 * GET_INST_SIZE(cpu) 
				cpu->Reg[15] = cpu->Reg[15] + 8 
						+ signed_int + (BIT(inst, 24) << 1);
				//DEBUG_MSG;
			}
			INC_PC(sizeof(blx_inst));
			goto DISPATCH;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
//		INC_PC(sizeof(bx_inst));
		INC_PC(sizeof(blx_inst));
		goto DISPATCH;
	}
	BX_INST:
	{
		INC_ICOUNTER;
		bx_inst *inst_cream = (bx_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			if (inst_cream->Rm == 15)
				LOG_WARNING(Core_ARM11, "BX at pc %x: use of Rm = R15 is discouraged", cpu->Reg[15]);
			cpu->TFlag = cpu->Reg[inst_cream->Rm] & 0x1;
			cpu->Reg[15] = cpu->Reg[inst_cream->Rm] & 0xfffffffe;
//			cpu->TFlag = cpu->Reg[inst_cream->Rm] & 0x1;
			INC_PC(sizeof(bx_inst));
			goto DISPATCH;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
//		INC_PC(sizeof(bx_inst));
		INC_PC(sizeof(bx_inst));
		goto DISPATCH;
	}
	BXJ_INST:
	CDP_INST:
	{
		INC_ICOUNTER;
		cdp_inst *inst_cream = (cdp_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			/* FIXME, check if cp access allowed */
			#define CP_ACCESS_ALLOW 0
			if(CP_ACCESS_ALLOW){
				/* undefined instruction here */
				cpu->NumInstrsToExecute = 0;
				return num_instrs;
			}
			LOG_ERROR(Core_ARM11, "CDP insn inst=0x%x, pc=0x%x\n", inst_cream->inst, cpu->Reg[15]);
			unsigned cpab = (cpu->CDP[inst_cream->cp_num]) (cpu, ARMul_FIRST, inst_cream->inst);
			if(cpab != ARMul_DONE){
				LOG_ERROR(Core_ARM11, "CDP insn wrong, inst=0x%x, cp_num=0x%x\n", inst_cream->inst, inst_cream->cp_num);
				//CITRA_IGNORE_EXIT(-1);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(cdp_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	CLREX_INST:
	{
		INC_ICOUNTER;
		remove_exclusive(cpu, 0);
		cpu->exclusive_state = 0;

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(clrex_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	CLZ_INST:
	{
		INC_ICOUNTER;
		clz_inst *inst_cream = (clz_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			RD = clz(RM);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(clz_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	CMN_INST:
	{
		INC_ICOUNTER;
		cmn_inst *inst_cream = (cmn_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
//			DEBUG_LOG(ARM11, "RN is %x\n", RN);
			lop = RN;
			rop = SHIFTER_OPERAND;
			dst = lop + rop;
			UPDATE_NFLAG(dst);
			UPDATE_ZFLAG(dst);
			UPDATE_CFLAG(dst, lop, rop);
			UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(cmn_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	CMP_INST:
	{
//		DEBUG_LOG(ARM11, "cmp inst\n");
//		DEBUG_LOG(ARM11, "pc:       %x\n", cpu->Reg[15]);
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
//			DEBUG_LOG(ARM11, "r0 is %x\n", cpu->Reg[0]);
			cmp_inst *inst_cream = (cmp_inst *)inst_base->component;
			lop = RN;
			if (inst_cream->Rn == 15) {
				lop += 2 * GET_INST_SIZE(cpu);
			}
			rop = SHIFTER_OPERAND;
			dst = lop - rop;

			UPDATE_NFLAG(dst);
			UPDATE_ZFLAG(dst);
//			UPDATE_CFLAG(dst, lop, rop);
			UPDATE_CFLAG_NOT_BORROW_FROM(lop, rop);
//			UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
			UPDATE_VFLAG_OVERFLOW_FROM(dst, lop, rop);
//			UPDATE_VFLAG_WITH_NOT(dst, lop, rop);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(cmp_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	CPS_INST:
	{
		INC_ICOUNTER;
		cps_inst *inst_cream = (cps_inst *)inst_base->component;
		uint32_t aif_val = 0;
		uint32_t aif_mask = 0;
		if (InAPrivilegedMode(cpu)) {
			/* isInAPrivilegedMode */
			if (inst_cream->imod1) {
				if (inst_cream->A) {
					aif_val |= (inst_cream->imod0 << 8);
					aif_mask |= 1 << 8;
				}
				if (inst_cream->I) {
					aif_val |= (inst_cream->imod0 << 7);
					aif_mask |= 1 << 7;
				}
				if (inst_cream->F) {
					aif_val |= (inst_cream->imod0 << 6);
					aif_mask |= 1 << 6;
				}
				aif_mask = ~aif_mask;
				cpu->Cpsr = (cpu->Cpsr & aif_mask) | aif_val;
			}
			if (inst_cream->mmod) {
				cpu->Cpsr = (cpu->Cpsr & 0xffffffe0) | inst_cream->mode;
				switch_mode(cpu, inst_cream->mode);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(cps_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	CPY_INST:
	{
		INC_ICOUNTER;
		mov_inst *inst_cream = (mov_inst *)inst_base->component;
//		cpy_inst *inst_cream = (cpy_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			RD = SHIFTER_OPERAND;
//			RD = RM;
			if ((inst_cream->Rd == 15)) {
				INC_PC(sizeof(mov_inst));
				goto DISPATCH;
			}
		}
//		DEBUG_LOG(ARM11, "cpy inst %x\n", cpu->Reg[15]);
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mov_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	EOR_INST:
	{
		INC_ICOUNTER;
		eor_inst *inst_cream = (eor_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			if (inst_cream->Rn == 15) {
				lop += 2 * GET_INST_SIZE(cpu);
			}
			rop = SHIFTER_OPERAND;
			RD = dst = lop ^ rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr*/
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
//				UPDATE_CFLAG(dst, lop, rop);
//				UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(eor_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(eor_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDC_INST:
	{
		INC_ICOUNTER;
		/* NOT IMPL */
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDM_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			int i;
			unsigned int ret;
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) {
				goto MMU_EXCEPTION;
			}
			unsigned int inst = inst_cream->inst;
			if (BIT(inst, 22) && !BIT(inst, 15)) {
//				DEBUG_MSG;
				#if 1
				/* LDM (2) user */
				for (i = 0; i < 13; i++) {
					if(BIT(inst, i)){
						#if 0
						fault = check_address_validity(cpu, addr, &phys_addr, 1);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						#endif
						fault = interpreter_read_memory(addr, phys_addr, ret, 32);
						//if (fault) goto MMU_EXCEPTION;
						cpu->Reg[i] = ret;
						addr += 4;
						if ((addr & 0xfff) == 0) {
							fault = check_address_validity(cpu, addr, &phys_addr, 1);
						} else {
							phys_addr += 4;
						}
					}
				}
				if (BIT(inst, 13)) {
					#if 0
					fault = check_address_validity(cpu, addr, &phys_addr, 1);
					if (fault) {
						goto MMU_EXCEPTION;
					}
					#endif
					fault = interpreter_read_memory(addr, phys_addr, ret, 32);
					//if (fault) goto MMU_EXCEPTION;
					if (cpu->Mode == USER32MODE) 
						cpu->Reg[13] = ret;
					else
						cpu->Reg_usr[0] = ret;
					addr += 4;
					if ((addr & 0xfff) == 0) {
						fault = check_address_validity(cpu, addr, &phys_addr, 1);
					} else {
						phys_addr += 4;
					}
				}
				if (BIT(inst, 14)) {
					#if 0
					fault = check_address_validity(cpu, addr, &phys_addr, 1);
					if (fault) {
						goto MMU_EXCEPTION;
					}
					#endif
					fault = interpreter_read_memory(addr, phys_addr, ret, 32);
					//if (fault) goto MMU_EXCEPTION;
					if (cpu->Mode == USER32MODE) 
						cpu->Reg[14] = ret;
					else
						cpu->Reg_usr[1] = ret;
				}
				#endif
			} else if (!BIT(inst, 22)) {
				for( i = 0; i < 16; i ++ ){
					if(BIT(inst, i)){
						//bus_read(32, addr, &ret);
						#if 0
						fault = check_address_validity(cpu, addr, &phys_addr, 1);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						#endif
						fault = interpreter_read_memory(addr, phys_addr, ret, 32);
						if (fault) goto MMU_EXCEPTION;
						/* For armv5t, should enter thumb when bits[0] is non-zero. */
						if(i == 15){
							cpu->TFlag = ret & 0x1;
							ret &= 0xFFFFFFFE;
							//DEBUG_LOG(ARM11, "In %s, TFlag ret=0x%x\n", __FUNCTION__, ret);
						}

						cpu->Reg[i] = ret;
						addr += 4;
						if ((addr & 0xfff) == 0) {
							fault = check_address_validity(cpu, addr, &phys_addr, 1);
						} else {
							phys_addr += 4;
						}
					}
				}
			} else if (BIT(inst, 22) && BIT(inst, 15)) {
				for( i = 0; i < 15; i ++ ){
					if(BIT(inst, i)){
						#if 0
						fault = check_address_validity(cpu, addr, &phys_addr, 1);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						#endif
						fault = interpreter_read_memory(addr, phys_addr, ret, 32);
						//if (fault) goto MMU_EXCEPTION;
						cpu->Reg[i] = ret;
						addr += 4;
						if ((addr & 0xfff) == 0) {
							fault = check_address_validity(cpu, addr, &phys_addr, 1);
						} else {
							phys_addr += 4;
						}
 					}
 				}
				
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Cpsr & 0x1f);
					LOAD_NZCVT;
				}
				#if 0
				fault = check_address_validity(cpu, addr, &phys_addr, 1);
				if (fault) {
					goto MMU_EXCEPTION;
				}
				#endif
				fault = interpreter_read_memory(addr, phys_addr, ret, 32);
				if (fault) {
					goto MMU_EXCEPTION;
				}
				cpu->Reg[15] = ret;
				#if 0
				addr += 4;
				phys_addr += 4;
				#endif
 			}
			if (BIT(inst, 15)) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SXTH_INST:
	{
		INC_ICOUNTER;
		sxth_inst *inst_cream = (sxth_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate);
			if (BIT(operand2, 15)) {
				operand2 |= 0xffff0000;
			} else {
				operand2 &= 0xffff;
			}
			RD = operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(sxth_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDR_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		//if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			//bus_read(32, addr, &value);
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (BIT(CP15_REG(CP15_CONTROL), 22) == 1)
				cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			else {
				value = ROTATE_RIGHT_32(value,(8*(addr&0x3)));
				cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			}
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				/* For armv5t, should enter thumb when bits[0] is non-zero. */
				cpu->TFlag = value & 0x1;
				cpu->Reg[15] &= 0xFFFFFFFE;
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		//}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRCOND_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if (CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			//bus_read(32, addr, &value);
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (BIT(CP15_REG(CP15_CONTROL), 22) == 1)
				cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			else {
                                value = ROTATE_RIGHT_32(value,(8*(addr&0x3)));
                                cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
                        }

			if (BITS(inst_cream->inst, 12, 15) == 15) {
				/* For armv5t, should enter thumb when bits[0] is non-zero. */
				cpu->TFlag = value & 0x1;
				cpu->Reg[15] &= 0xFFFFFFFE;
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UXTH_INST:
	{
		INC_ICOUNTER;
		uxth_inst *inst_cream = (uxth_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) 
						& 0xffff;
			RD = operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(uxth_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UXTAH_INST:
	{
		INC_ICOUNTER;
		uxtah_inst *inst_cream = (uxtah_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) 
						& 0xffff;
			RD = RN + operand2;
			if (inst_cream->Rn == 15 || inst_cream->Rm == 15) {
				LOG_ERROR(Core_ARM11, "invalid operands for UXTAH");
				CITRA_IGNORE_EXIT(-1);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(uxtah_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRB_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
			//bus_read(8, addr, &value);
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRBT_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRD_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			/* Should check if RD is even-numbered, Rd != 14, addr[0:1] == 0, (CP15_reg1_U == 1 || addr[2] == 0) */
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			uint32_t rear_phys_addr;
			 fault = check_address_validity(cpu, addr + 4, &rear_phys_addr, 1);
			if(fault){
				LOG_ERROR(Core_ARM11, "mmu fault , should rollback the above get_addr\n");
				CITRA_IGNORE_EXIT(-1);
				goto MMU_EXCEPTION;
			}
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			fault = interpreter_read_memory(addr + 4, rear_phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			cpu->Reg[BITS(inst_cream->inst, 12, 15) + 1] = value;
			/* No dispatch since this operation should not modify R15 */
		}
		cpu->Reg[15] += 4;
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	LDREX_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = cpu->Reg[BITS(inst_cream->inst, 16, 19)];
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;

			add_exclusive_addr(cpu, phys_addr);
			cpu->exclusive_state = 1;

			//bus_read(32, addr, &value);
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDREXB_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = cpu->Reg[BITS(inst_cream->inst, 16, 19)];
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
			
			add_exclusive_addr(cpu, phys_addr);
			cpu->exclusive_state = 1;

			//bus_read(8, addr, &value);
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRH_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = 0;
			fault = interpreter_read_memory(addr, phys_addr, value, 16);
//			fault = interpreter_read_memory(addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			//if (value == 0xffff && cpu->icounter > 190000000 && cpu->icounter < 210000000) {
			//	value = 0xffffffff;
			//}
			//bus_read(16, addr, &value);
//			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value & 0xffff;
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRSB_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
//			DEBUG_LOG(ARM11, "ldrsb addr is %x\n", addr);
			fault = interpreter_read_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
			//bus_read(8, addr, &value);
			if (BIT(value, 7)) {
				value |= 0xffffff00;
			}
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRSH_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 16);
			if (fault) goto MMU_EXCEPTION;
			//bus_read(16, addr, &value);
			if (BIT(value, 15)) {
				value |= 0xffff0000;
			}
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	LDRT_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;

			if (BIT(CP15_REG(CP15_CONTROL), 22) == 1)
				cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
			else
				cpu->Reg[BITS(inst_cream->inst, 12, 15)] = ROTATE_RIGHT_32(value,(8*(addr&0x3))) ;

			if (BITS(inst_cream->inst, 12, 15) == 15) {
				INC_PC(sizeof(ldst_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MCR_INST:
	{
		INC_ICOUNTER;
		/* NOT IMPL */
		mcr_inst *inst_cream = (mcr_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int inst = inst_cream->inst;
			if (inst_cream->Rd == 15) {
				DEBUG_MSG;
			} else {
				if (inst_cream->cp_num == 15) {
					if(CRn == 0 && OPCODE_2 == 0 && CRm == 0) {
						//LET(RD, CONST(0x0007b000));
						//LET(RD, CONST(0x410FB760));
						//LET(CP15_MAIN_ID, R(RD));
						CP15_REG(CP15_MAIN_ID) = RD;
					} else if (CRn == 1 && CRm == 0 && OPCODE_2 == 1) {
						//LET(RD, R(CP15_CONTROL));
						CP15_REG(CP15_AUXILIARY_CONTROL) = RD;
					} else if (CRn == 1 && CRm == 0 && OPCODE_2 == 2) {
						//LET(RD, R(CP15_CONTROL));
						CP15_REG(CP15_COPROCESSOR_ACCESS_CONTROL) = RD;
					} else if(CRn == 1 && CRm == 0 && OPCODE_2 == 0) {
						//LET(CP15_CONTROL, R(RD));
						CP15_REG(CP15_CONTROL) = RD;
					} else if (CRn == 3 && CRm == 0 && OPCODE_2 == 0) {
						//LET(CP15_DOMAIN_ACCESS_CONTROL, R(RD));
						CP15_REG(CP15_DOMAIN_ACCESS_CONTROL) = RD;
					} else if (CRn == 2 && CRm == 0 && OPCODE_2 == 0) {
						//LET(CP15_TRANSLATION_BASE_TABLE_0, R(RD));
						CP15_REG(CP15_TRANSLATION_BASE_TABLE_0) = RD;
					} else if (CRn == 2 && CRm == 0 && OPCODE_2 == 1) {
						//LET(CP15_TRANSLATION_BASE_TABLE_1, R(RD));
						CP15_REG(CP15_TRANSLATION_BASE_TABLE_1) = RD;
					} else if (CRn == 2 && CRm == 0 && OPCODE_2 == 2) {
						//LET(CP15_TRANSLATION_BASE_CONTROL, R(RD));
						CP15_REG(CP15_TRANSLATION_BASE_CONTROL) = RD;
					} else if(CRn == MMU_CACHE_OPS){
						//SKYEYE_WARNING("cache operation have not implemented.\n");
					} else if(CRn == MMU_TLB_OPS){
						switch (CRm) {
						case 5: /* ITLB */
							switch(OPCODE_2){
							case 0: /* invalidate all */
								//invalidate_all_tlb(state);
								LOG_DEBUG(Core_ARM11, "{TLB} [INSN] invalidate all");
								//remove_tlb(INSN_TLB);
								//erase_all(core, INSN_TLB);
								break;
							case 1: /* invalidate by MVA */
								//invalidate_by_mva(state, value);
								//DEBUG_LOG(ARM11, "{TLB} [INSN] invalidate by mva\n");
								//remove_tlb_by_mva(RD, INSN_TLB);
								//erase_by_mva(core, RD, INSN_TLB);
								break;
							case 2: /* invalidate by asid */
								//invalidate_by_asid(state, value);
								//DEBUG_LOG(ARM11, "{TLB} [INSN] invalidate by asid\n");
								//erase_by_asid(core, RD, INSN_TLB);
								break;
							default:
								break;
							}

							break;
						case 6: /* DTLB */
							switch(OPCODE_2){
							case 0: /* invalidate all */
								//invalidate_all_tlb(state);
								//remove_tlb(DATA_TLB);
								//erase_all(core, DATA_TLB);
								LOG_DEBUG(Core_ARM11, "{TLB} [DATA] invalidate all");
								break;
							case 1: /* invalidate by MVA */
								//invalidate_by_mva(state, value);
								//remove_tlb_by_mva(RD, DATA_TLB);
								//erase_by_mva(core, RD, DATA_TLB);
								//DEBUG_LOG(ARM11, "{TLB} [DATA] invalidate by mva\n");
								break;
							case 2: /* invalidate by asid */
								//invalidate_by_asid(state, value);
								//remove_tlb_by_asid(RD, DATA_TLB);
								//erase_by_asid(core, RD, DATA_TLB);
								//DEBUG_LOG(ARM11, "{TLB} [DATA] invalidate by asid\n");
								break;
							default:
								break;
							}
							break;
						case 7: /* UNIFILED TLB */
							switch(OPCODE_2){
							case 0: /* invalidate all */
								//invalidate_all_tlb(state);
								//erase_all(core, INSN_TLB);
								//erase_all(core, DATA_TLB);
								//remove_tlb(DATA_TLB);
								//remove_tlb(INSN_TLB);
								//DEBUG_LOG(ARM11, "{TLB} [UNIFILED] invalidate all\n");
								break;
							case 1: /* invalidate by MVA */
								//invalidate_by_mva(state, value);
								//erase_by_mva(core, RD, DATA_TLB);
								//erase_by_mva(core, RD, INSN_TLB);
								LOG_DEBUG(Core_ARM11, "{TLB} [UNIFILED] invalidate by mva");
								break;
							case 2: /* invalidate by asid */
								//invalidate_by_asid(state, value);
								//erase_by_asid(core, RD, DATA_TLB);
								//erase_by_asid(core, RD, INSN_TLB);
								LOG_DEBUG(Core_ARM11, "{TLB} [UNIFILED] invalidate by asid");
								break;
							default:
								break;
							}
							break;
						default:
							break;
						}
					} else if(CRn == MMU_PID){
						if(OPCODE_2 == 0)
							CP15_REG(CP15_PID) = RD;
						else if(OPCODE_2 == 1)
							CP15_REG(CP15_CONTEXT_ID) = RD;
						else if(OPCODE_2 == 3){
							CP15_REG(CP15_THREAD_URO) = RD;
						}
						else{
							printf ("mmu_mcr wrote UNKNOWN - reg %d\n", CRn);
						}

					} else {
						LOG_ERROR(Core_ARM11, "mcr CRn=%d, CRm=%d OP2=%d is not implemented", CRn, CRm, OPCODE_2);
					}
				}
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mcr_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MCRR_INST:
	MLA_INST:
	{
		INC_ICOUNTER;
		mla_inst *inst_cream = (mla_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			uint64_t rm = RM;
			uint64_t rs = RS;
			uint64_t rn = RN;
			if (inst_cream->Rm == 15 || inst_cream->Rs == 15 || inst_cream->Rn == 15) {
				LOG_ERROR(Core_ARM11, "invalid operands for MLA");
				CITRA_IGNORE_EXIT(-1);
			}
//			RD = dst = RM * RS + RN;
			RD = dst = static_cast<uint32_t>((rm * rs + rn) & 0xffffffff);
			if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(mla_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mla_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MOV_INST:
	{
//		DEBUG_LOG(ARM11, "mov inst\n");
//		DEBUG_LOG(ARM11, "pc:       %x\n", cpu->Reg[15]);
//		debug_function(cpu);
//		cpu->icount ++;
		INC_ICOUNTER;
		mov_inst *inst_cream = (mov_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			RD = dst = SHIFTER_OPERAND;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(mov_inst));
				goto DISPATCH;
			}
//				return;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mov_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MRC_INST:
	{
		INC_ICOUNTER;
		/* NOT IMPL */
		mrc_inst *inst_cream = (mrc_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int inst = inst_cream->inst;
			if (inst_cream->Rd == 15) {
				DEBUG_MSG;
			}
			if (inst_cream->inst == 0xeef04a10) {
				/* undefined instruction fmrx */
				RD = 0x20000000;
				CITRA_IGNORE_EXIT(-1);
				goto END;
			} else {
				if (inst_cream->cp_num == 15) {
					if(CRn == 0 && OPCODE_2 == 0 && CRm == 0) {
						//LET(RD, CONST(0x0007b000));
						//LET(RD, CONST(0x410FB760));
						//LET(RD, R(CP15_MAIN_ID));
						RD = cpu->CP15[CP15(CP15_MAIN_ID)];
					} else if (CRn == 1 && CRm == 0 && OPCODE_2 == 0) {
						//LET(RD, R(CP15_CONTROL));
						RD = cpu->CP15[CP15(CP15_CONTROL)];
					} else if (CRn == 1 && CRm == 0 && OPCODE_2 == 1) {
						//LET(RD, R(CP15_CONTROL));
						RD = cpu->CP15[CP15(CP15_AUXILIARY_CONTROL)];
					} else if (CRn == 1 && CRm == 0 && OPCODE_2 == 2) {
						//LET(RD, R(CP15_CONTROL));
						RD = cpu->CP15[CP15(CP15_COPROCESSOR_ACCESS_CONTROL)];
					} else if (CRn == 3 && CRm == 0 && OPCODE_2 == 0) {
						//LET(RD, R(CP15_DOMAIN_ACCESS_CONTROL));
						RD = cpu->CP15[CP15(CP15_DOMAIN_ACCESS_CONTROL)];
					} else if (CRn == 2 && CRm == 0 && OPCODE_2 == 0) {
						//LET(RD, R(CP15_TRANSLATION_BASE_TABLE_0));
						RD = cpu->CP15[CP15(CP15_TRANSLATION_BASE_TABLE_0)];
					} else if (CRn == 5 && CRm == 0 && OPCODE_2 == 0) {
						//LET(RD, R(CP15_FAULT_STATUS));
						RD = cpu->CP15[CP15(CP15_FAULT_STATUS)];
					} else if (CRn == 6 && CRm == 0 && OPCODE_2 == 0) {
						//LET(RD, R(CP15_FAULT_ADDRESS));
						RD = cpu->CP15[CP15(CP15_FAULT_ADDRESS)];
					} else if (CRn == 0 && CRm == 0 && OPCODE_2 == 1) {
						//LET(RD, R(CP15_CACHE_TYPE));
						RD = cpu->CP15[CP15(CP15_CACHE_TYPE)];
					} else if (CRn == 5 && CRm == 0 && OPCODE_2 == 1) {
						//LET(RD, R(CP15_INSTR_FAULT_STATUS));
						RD = cpu->CP15[CP15(CP15_INSTR_FAULT_STATUS)];
					} else if (CRn == 13) {
						if(OPCODE_2 == 0)
							RD = CP15_REG(CP15_PID);
						else if(OPCODE_2 == 1)
							RD = CP15_REG(CP15_CONTEXT_ID);
						else if(OPCODE_2 == 3){
							RD = Memory::KERNEL_MEMORY_VADDR;
						}
						else{
							printf ("mmu_mrr wrote UNKNOWN - reg %d\n", CRn);
						}
					}
					else {
					    LOG_ERROR(Core_ARM11, "mrc CRn=%d, CRm=%d, OP2=%d is not implemented", CRn, CRm, OPCODE_2);
					}
				}
				//DEBUG_LOG(ARM11, "mrc is not implementated. CRn is %d, CRm is %d, OPCODE_2 is %d\n", CRn, CRm, OPCODE_2);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mrc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MRRC_INST:
	MRS_INST:
	{
		INC_ICOUNTER;
		mrs_inst *inst_cream = (mrs_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			if (inst_cream->R) {
				RD = cpu->Spsr_copy;
			} else {
				SAVE_NZCVT;
				RD = cpu->Cpsr;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mrs_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MSR_INST:
	{
		INC_ICOUNTER;
		msr_inst *inst_cream = (msr_inst *)inst_base->component;
		const uint32_t UnallocMask = 0x06f0fc00, UserMask = 0xf80f0200, PrivMask = 0x000001df, StateMask = 0x01000020;
		unsigned int inst = inst_cream->inst;
		unsigned int operand;

		if (BIT(inst, 25)) {
			int rot_imm = BITS(inst, 8, 11) * 2;
			//operand = ROTL(CONST(BITS(0, 7)), CONST(32 - rot_imm));
			operand = ROTATE_RIGHT_32(BITS(inst, 0, 7), rot_imm);
		} else {
			//operand = R(RM);
			operand = cpu->Reg[BITS(inst, 0, 3)];
		}
		uint32_t byte_mask = (BIT(inst, 16) ? 0xff : 0) | (BIT(inst, 17) ? 0xff00 : 0)
					| (BIT(inst, 18) ? 0xff0000 : 0) | (BIT(inst, 19) ? 0xff000000 : 0);
		uint32_t mask;
		if (!inst_cream->R) {
			if (InAPrivilegedMode(cpu)) {
				if ((operand & StateMask) != 0) {
					/* UNPREDICTABLE */
					DEBUG_MSG;
				} else
					mask = byte_mask & (UserMask | PrivMask);
			} else {
				mask = byte_mask & UserMask;
			}
			//LET(CPSR_REG, OR(AND(R(CPSR_REG), COM(CONST(mask))), AND(operand, CONST(mask))));
			SAVE_NZCVT;

			cpu->Cpsr = (cpu->Cpsr & ~mask) | (operand & mask);
			switch_mode(cpu, cpu->Cpsr & 0x1f);
			LOAD_NZCVT;
		} else {
			if (CurrentModeHasSPSR) {
				mask = byte_mask & (UserMask | PrivMask | StateMask);
				//LET(SPSR_REG, OR(AND(R(SPSR_REG), COM(CONST(mask))), AND(operand, CONST(mask))));
				cpu->Spsr_copy = (cpu->Spsr_copy & ~mask) | (operand & mask);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(msr_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MUL_INST:
	{
		INC_ICOUNTER;
		mul_inst *inst_cream = (mul_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
//			RD = dst = SHIFTER_OPERAND;
			uint64_t rm = RM;
			uint64_t rs = RS;
			RD = dst = static_cast<uint32_t>((rm * rs) & 0xffffffff);
			if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(mul_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mul_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	MVN_INST:
	{
		INC_ICOUNTER;
		mvn_inst *inst_cream = (mvn_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
//			RD = dst = (SHIFTER_OPERAND ^ 0xffffffff);
			RD = dst = ~SHIFTER_OPERAND;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(mvn_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(mvn_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	ORR_INST:
	{
		INC_ICOUNTER;
		orr_inst *inst_cream = (orr_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			rop = SHIFTER_OPERAND;
//			DEBUG_LOG(ARM11, "lop is %x, rop is %x, r2 is %x, r3 is %x\n", lop, rop, cpu->Reg[2], cpu->Reg[3]);
			RD = dst = lop | rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr*/
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_WITH_SC;
//				UPDATE_CFLAG(dst, lop, rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(orr_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(orr_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	PKHBT_INST:
	PKHTB_INST:
	PLD_INST:
	{
		INC_ICOUNTER;
		/* NOT IMPL */
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(stc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	QADD_INST:
	QADD8_INST:

	QADD16_INST:
	QADDSUBX_INST:
	QSUB16_INST:
	QSUBADDX_INST:
	{
		INC_ICOUNTER;
		if (inst_base->cond == 0xE || CondPassed(cpu, inst_base->cond)) {
			generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;
			const s16 rm_lo = (RM & 0xFFFF);
			const s16 rm_hi = ((RM >> 16) & 0xFFFF);
			const s16 rn_lo = (RN & 0xFFFF);
			const s16 rn_hi = ((RN >> 16) & 0xFFFF);
			const u8 op2    = inst_cream->op2;

			s32 lo_result = 0;
			s32 hi_result = 0;

			// QADD16
			if (op2 == 0x00) {
				lo_result = (rn_lo + rm_lo);
				hi_result = (rn_hi + rm_hi);
			}
			// QASX
			else if (op2 == 0x01) {
				lo_result = (rn_lo - rm_hi);
				hi_result = (rn_hi + rm_lo);
			}
			// QSAX
			else if (op2 == 0x02) {
				lo_result = (rn_lo + rm_hi);
				hi_result = (rn_hi - rm_lo);
			}
			// QSUB16
			else if (op2 == 0x03) {
				lo_result = (rn_lo - rm_lo);
				hi_result = (rn_hi - rm_hi);
			}

			if (lo_result > 0x7FFF)
				lo_result = 0x7FFF;
			else if (lo_result < -0x8000)
				lo_result = -0x8000;

			if (hi_result > 0x7FFF)
				hi_result = 0x7FFF;
			else if (hi_result < -0x8000)
				hi_result = -0x8000;

			RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
		}

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(generic_arm_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	QDADD_INST:
	QDSUB_INST:
	QSUB_INST:
	QSUB8_INST:
	REV_INST:
	{
		INC_ICOUNTER;
		rev_inst *inst_cream = (rev_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			RD = ((RM & 0xff) << 24) |
				(((RM >> 8) & 0xff) << 16) |
				(((RM >> 16) & 0xff) << 8) |
				((RM >> 24) & 0xff);
			if (inst_cream->Rm == 15) {
				LOG_ERROR(Core_ARM11, "invalid operand for REV");
				CITRA_IGNORE_EXIT(-1);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(rev_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	REV16_INST:
	{
		INC_ICOUNTER;
		rev_inst *inst_cream = (rev_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			RD = (BITS(RM, 0, 7) << 8) | 
				BITS(RM, 8, 15) |
				(BITS(RM, 16, 23) << 24) |
				(BITS(RM, 24, 31) << 16);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(rev_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	REVSH_INST:
	RFE_INST:
	RSB_INST:
	{
		INC_ICOUNTER;
		rsb_inst *inst_cream = (rsb_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			rop = RN;
			lop = SHIFTER_OPERAND;
			if (inst_cream->Rn == 15) {
				rop += 2 * GET_INST_SIZE(cpu);;
			}
			RD = dst = lop - rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
				UPDATE_CFLAG_NOT_BORROW_FROM(lop, rop);
//				UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
				UPDATE_VFLAG_OVERFLOW_FROM(dst, lop, rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(rsb_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(rsb_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	RSC_INST:
	{
		INC_ICOUNTER;
		rsc_inst *inst_cream = (rsc_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			//lop = RN + !cpu->CFlag;
			//rop = SHIFTER_OPERAND;
			//RD = dst = rop - lop;
			lop = RN;
			rop = SHIFTER_OPERAND;
			RD = dst = rop - lop - !cpu->CFlag;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
//				UPDATE_CFLAG(dst, lop, rop);
//				UPDATE_CFLAG_NOT_BORROW_FROM(rop, lop);
				UPDATE_CFLAG_NOT_BORROW_FROM_FLAG(rop, lop, !cpu->CFlag);
//				cpu->CFlag = !((ISNEG(lop) && ISPOS(rop)) || (ISNEG(lop) && ISPOS(dst)) || (ISPOS(rop) && ISPOS(dst)));
				UPDATE_VFLAG_OVERFLOW_FROM((int)dst, (int)rop, (int)lop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(rsc_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(rsc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SADD8_INST:

	SADD16_INST:
	SADDSUBX_INST:
	SSUBADDX_INST:
	SSUB16_INST:
	{
		INC_ICOUNTER;
		if (inst_base->cond == 0xE || CondPassed(cpu, inst_base->cond)) {
			generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

			const s16 rn_lo = (RN & 0xFFFF);
			const s16 rn_hi = ((RN >> 16) & 0xFFFF);
			const s16 rm_lo = (RM & 0xFFFF);
			const s16 rm_hi = ((RM >> 16) & 0xFFFF);

			s32 lo_result = 0;
			s32 hi_result = 0;

			// SADD16
			if (inst_cream->op2 == 0x00) {
				lo_result = (rn_lo + rm_lo);
				hi_result = (rn_hi + rm_hi);
			}
			// SASX
			else if (inst_cream->op2 == 0x01) {
				lo_result = (rn_lo - rm_hi);
				hi_result = (rn_hi + rm_lo);
			}
			// SSAX
			else if (inst_cream->op2 == 0x02) {
				lo_result = (rn_lo + rm_hi);
				hi_result = (rn_hi - rm_lo);
			}
			// SSUB16
			else if (inst_cream->op2 == 0x03) {
				lo_result = (rn_lo - rm_lo);
				hi_result = (rn_hi - rm_hi);
			}

			RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

			if (lo_result >= 0) {
				cpu->Cpsr |= (1 << 16);
				cpu->Cpsr |= (1 << 17);
			} else {
				cpu->Cpsr &= ~(1 << 16);
				cpu->Cpsr &= ~(1 << 17);
			}

			if (hi_result >= 0) {
				cpu->Cpsr |= (1 << 18);
				cpu->Cpsr |= (1 << 19);
			} else {
				cpu->Cpsr &= ~(1 << 18);
				cpu->Cpsr &= ~(1 << 19);
			}
		}

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(generic_arm_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	SBC_INST:
	{
		INC_ICOUNTER;
		sbc_inst *inst_cream = (sbc_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = SHIFTER_OPERAND + !cpu->CFlag;
			rop = RN;
			RD = dst = rop - lop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
//				UPDATE_CFLAG(dst, lop, rop);
				//UPDATE_CFLAG_NOT_BORROW_FROM(rop, lop);
				//rop = rop - !cpu->CFlag;
				if(rop >= !cpu->CFlag)
					UPDATE_CFLAG_NOT_BORROW_FROM(rop - !cpu->CFlag, SHIFTER_OPERAND);
				else
					UPDATE_CFLAG_NOT_BORROW_FROM(rop, !cpu->CFlag);
//				cpu->CFlag = !((ISNEG(lop) && ISPOS(rop)) || (ISNEG(lop) && ISPOS(dst)) || (ISPOS(rop) && ISPOS(dst)));
				UPDATE_VFLAG_OVERFLOW_FROM(dst, rop, lop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(sbc_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(sbc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	SEL_INST:
	{
		INC_ICOUNTER;
		if (inst_base->cond == 0xE || CondPassed(cpu, inst_base->cond)) {
			generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

			const u32 to = RM;
			const u32 from = RN;
			const u32 cpsr = cpu->Cpsr;

			u32 result;
			if (cpsr & (1 << 16))
				result = from & 0xff;
			else
				result = to & 0xff;

			if (cpsr & (1 << 17))
				result |= from & 0x0000ff00;
			else
				result |= to & 0x0000ff00;

			if (cpsr & (1 << 18))
				result |= from & 0x00ff0000;
			else
				result |= to & 0x00ff0000;

			if (cpsr & (1 << 19))
				result |= from & 0xff000000;
			else
				result |= to & 0xff000000;

			RD = result;
		}

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(generic_arm_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	SETEND_INST:
	SHADD16_INST:
	SHADD8_INST:
	SHADDSUBX_INST:
	SHSUB16_INST:
	SHSUB8_INST:
	SHSUBADDX_INST:
	SMLA_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			smla_inst *inst_cream = (smla_inst *)inst_base->component;
			int32_t operand1, operand2;
			if (inst_cream->x == 0)
				operand1 = (BIT(RM, 15)) ? (BITS(RM, 0, 15) | 0xffff0000) : BITS(RM, 0, 15);
			else
				operand1 = (BIT(RM, 31)) ? (BITS(RM, 16, 31) | 0xffff0000) : BITS(RM, 16, 31);

			if (inst_cream->y == 0)
				operand2 = (BIT(RS, 15)) ? (BITS(RS, 0, 15) | 0xffff0000) : BITS(RS, 0, 15);
			else
				operand2 = (BIT(RS, 31)) ? (BITS(RS, 16, 31) | 0xffff0000) : BITS(RS, 16, 31);
			RD = operand1 * operand2 + RN;
			//FIXME: UPDATE Q FLAGS
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(smla_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SMLAD_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			smlad_inst *inst_cream = (smlad_inst *)inst_base->component;
			long long int rm = cpu->Reg[inst_cream->Rm];
			long long int rn = cpu->Reg[inst_cream->Rn];
			long long int ra = cpu->Reg[inst_cream->Ra];
			/* see SMUAD */
			if(inst_cream->Ra == 15)
				CITRA_IGNORE_EXIT(-1);
			int operand2 = (inst_cream->m)? ROTATE_RIGHT_32(rm, 16):rm;
			
			int half_rn, half_operand2;
			half_rn = rn & 0xFFFF;
			half_rn = (half_rn & 0x8000)? (0xFFFF0000|half_rn) : half_rn;

			half_operand2 = operand2 & 0xFFFF;
			half_operand2 = (half_operand2 & 0x8000)? (0xFFFF0000|half_operand2) : half_operand2;
		
			long long int product1 = half_rn * half_operand2;

			half_rn = (rn & 0xFFFF0000) >> 16;
			half_rn = (half_rn & 0x8000)? (0xFFFF0000|half_rn) : half_rn;

			half_operand2 = (operand2 & 0xFFFF0000) >> 16;
			half_operand2 = (half_operand2 & 0x8000)? (0xFFFF0000|half_operand2) : half_operand2;
		
			long long int product2 = half_rn * half_operand2;

			long long int signed_ra = (ra & 0x80000000)? (0xFFFFFFFF00000000LL) | ra : ra;
			long long int result = product1 + product2 + signed_ra;
			cpu->Reg[inst_cream->Rd] = result & 0xFFFFFFFF;
			/* FIXME , should check Signed overflow */
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umlal_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}

	SMLAL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			umlal_inst *inst_cream = (umlal_inst *)inst_base->component;
			long long int rm = RM;
			long long int rs = RS;
			if (BIT(rm, 31)) {
				rm |= 0xffffffff00000000LL;
			}
			if (BIT(rs, 31)) {
				rs |= 0xffffffff00000000LL;
			}
			long long int rst = rm * rs;
			long long int rdhi32 = RDHI;
			long long int hilo = (rdhi32 << 32) + RDLO;
			rst += hilo;
			RDLO = BITS(rst,  0, 31);
			RDHI = BITS(rst, 32, 63);
			if (inst_cream->S) {
				cpu->NFlag = BIT(RDHI, 31);
				cpu->ZFlag = (RDHI == 0 && RDLO == 0);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umlal_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SMLALXY_INST:
	SMLALD_INST:
	SMLAW_INST:
	SMLSD_INST:
	SMLSLD_INST:
	SMMLA_INST:
	SMMLS_INST:
	SMMUL_INST:
	SMUAD_INST:
	SMUL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			smul_inst *inst_cream = (smul_inst *)inst_base->component;
			uint32_t operand1, operand2;
			if (inst_cream->x == 0)
				operand1 = (BIT(RM, 15)) ? (BITS(RM, 0, 15) | 0xffff0000) : BITS(RM, 0, 15);
			else
				operand1 = (BIT(RM, 31)) ? (BITS(RM, 16, 31) | 0xffff0000) : BITS(RM, 16, 31);

			if (inst_cream->y == 0)
				operand2 = (BIT(RS, 15)) ? (BITS(RS, 0, 15) | 0xffff0000) : BITS(RS, 0, 15);
			else
				operand2 = (BIT(RS, 31)) ? (BITS(RS, 16, 31) | 0xffff0000) : BITS(RS, 16, 31);
			RD = operand1 * operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(smul_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SMULL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			umull_inst *inst_cream = (umull_inst *)inst_base->component;
//			DEBUG_LOG(ARM11, "rm : [%llx] rs : [%llx] rst [%llx]\n", RM, RS, rst);
			int64_t rm = RM;
			int64_t rs = RS;
			if (BIT(rm, 31)) {
				rm |= 0xffffffff00000000LL;
			}
			if (BIT(rs, 31)) {
				rs |= 0xffffffff00000000LL;
			}
			int64_t rst = rm * rs;
			RDHI = BITS(rst, 32, 63);
			RDLO = BITS(rst,  0, 31);


			if (inst_cream->S) {
				cpu->NFlag = BIT(RDHI, 31);
				cpu->ZFlag = (RDHI == 0 && RDLO == 0);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umull_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SMULW_INST:
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			smlad_inst *inst_cream = (smlad_inst *)inst_base->component;
//			DEBUG_LOG(ARM11, "rm : [%llx] rs : [%llx] rst [%llx]\n", RM, RS, rst);
			int64_t rm = RM;
			int64_t rn = RN;
			if (inst_cream->m)
				rm = BITS(rm,16 , 31);
			else
				rm = BITS(rm,0 , 15);
			int64_t rst = rm * rn;
			RD = BITS(rst,  16, 47);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(smlad_inst));
		FETCH_INST;
		GOTO_NEXT_INST;

	SMUSD_INST:
	SRS_INST:
	SSAT_INST:
	SSAT16_INST:
	SSUB8_INST:
	STC_INST:
	{
		INC_ICOUNTER;
		/* NOT IMPL */
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(stc_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STM_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		unsigned int inst = inst_cream->inst;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			int i;
			unsigned int Rn = BITS(inst, 16, 19);
			unsigned int old_RN = cpu->Reg[Rn];

			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			if (BIT(inst_cream->inst, 22) == 1) {
//				DEBUG_MSG;
				#if 1
				for (i = 0; i < 13; i++) {
					if(BIT(inst_cream->inst, i)){
						fault = check_address_validity(cpu, addr, &phys_addr, 0);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
					}
				}
				if (BIT(inst_cream->inst, 13)) {
					if (cpu->Mode == USER32MODE) {
						fault = check_address_validity(cpu, addr, &phys_addr, 0);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
					} else {
						fault = interpreter_write_memory(addr, phys_addr, cpu->Reg_usr[0], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
					}
				}
				if (BIT(inst_cream->inst, 14)) {
					if (cpu->Mode == USER32MODE) {
						fault = check_address_validity(cpu, addr, &phys_addr, 0);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
					} else {
						fault = check_address_validity(cpu, addr, &phys_addr, 0);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						fault = interpreter_write_memory(addr, phys_addr, cpu->Reg_usr[1], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
					}
				}
				if (BIT(inst_cream->inst, 15)) {
					fault = check_address_validity(cpu, addr, &phys_addr, 0);
					if (fault) {
						goto MMU_EXCEPTION;
					}
					fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i] + 8, 32);
					if (fault) goto MMU_EXCEPTION;
				}
				#endif
			} else {
				for( i = 0; i < 15; i ++ ){
					if(BIT(inst_cream->inst, i)){
						//arch_write_memory(cpu, bb, Addr, R(i), 32);
						//bus_write(32, addr, cpu->Reg[i]);
						fault = check_address_validity(cpu, addr, &phys_addr, 0);
						if (fault) {
							goto MMU_EXCEPTION;
						}
						if(i == Rn)
							fault = interpreter_write_memory(addr, phys_addr, old_RN, 32);
						else
							fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i], 32);
						if (fault) goto MMU_EXCEPTION;
						addr += 4;
						phys_addr += 4;
						//Addr = ADD(Addr, CONST(4));
					}
				}

				/* check pc reg*/
				if(BIT(inst_cream->inst, i)){
					//arch_write_memory(cpu, bb, Addr, STOREM_CHECK_PC, 32);
					//bus_write(32, addr, cpu->Reg[i] + 8);
					fault = check_address_validity(cpu, addr, &phys_addr, 0);
					if (fault) {
						goto MMU_EXCEPTION;
					}
					fault = interpreter_write_memory(addr, phys_addr, cpu->Reg[i] + 8, 32);
					if (fault) goto MMU_EXCEPTION;
				}
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SXTB_INST:
	{
		INC_ICOUNTER;
		sxtb_inst *inst_cream = (sxtb_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			if (inst_cream->Rm == 15) {
				LOG_ERROR(Core_ARM11, "invalid operand for SXTB");
				CITRA_IGNORE_EXIT(-1);
			}
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate);
			if (BIT(operand2, 7)) {
				operand2 |= 0xffffff00;
			} else
				operand2 &= 0xff;
			RD = operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(sxtb_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STR_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)];
			//bus_write(32, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UXTB_INST:
	{
		INC_ICOUNTER;
		uxtb_inst *inst_cream = (uxtb_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) 
						& 0xff;
			RD = operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(uxtb_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UXTAB_INST:
	{
		INC_ICOUNTER;
		uxtab_inst *inst_cream = (uxtab_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) 
						& 0xff;
			RD = RN + operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(uxtab_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STRB_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xff;
			//bus_write(8, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STRBT_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xff;
			//bus_write(8, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		//if (BITS(inst_cream->inst, 12, 15) == 15)
		//	goto DISPATCH;
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STRD_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			uint32_t rear_phys_addr;
			fault = check_address_validity(cpu, addr + 4, &rear_phys_addr, 0);
                        if (fault){
				LOG_ERROR(Core_ARM11, "mmu fault , should rollback the above get_addr\n");
				CITRA_IGNORE_EXIT(-1);
				goto MMU_EXCEPTION;
			}

			//fault = inst_cream->get_addr(cpu, inst_cream->inst, addr + 4, phys_addr + 4, 0);
			//if (fault) goto MMU_EXCEPTION;

			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)];
			//bus_write(32, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			value = cpu->Reg[BITS(inst_cream->inst, 12, 15) + 1];
			//bus_write(32, addr, value);
			fault = interpreter_write_memory(addr + 4, rear_phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STREX_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = cpu->Reg[BITS(inst_cream->inst, 16, 19)];
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 0, 3)];
			fault = check_address_validity(cpu, addr, &phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;

			int dest_reg = BITS(inst_cream->inst, 12, 15);
			if((exclusive_detect(cpu, phys_addr) == 0) && (cpu->exclusive_state == 1)){
				remove_exclusive(cpu, phys_addr);
				cpu->Reg[dest_reg] = 0;
				cpu->exclusive_state = 0;
				
				//			bus_write(32, addr, value);
				fault = interpreter_write_memory(addr, phys_addr, value, 32);
				if (fault) goto MMU_EXCEPTION;
			}
			else{
				/* Failed to write due to mutex access */
				cpu->Reg[dest_reg] = 1;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STREXB_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = cpu->Reg[BITS(inst_cream->inst, 16, 19)];
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 0, 3)] & 0xff;
			fault = check_address_validity(cpu, addr, &phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			//bus_write(8, addr, value);
			int dest_reg = BITS(inst_cream->inst, 12, 15);
			if((exclusive_detect(cpu, phys_addr) == 0) && (cpu->exclusive_state == 1)){
				remove_exclusive(cpu, phys_addr);
				cpu->Reg[dest_reg] = 0;
				cpu->exclusive_state = 0;
				fault = interpreter_write_memory(addr, phys_addr, value, 8);
				if (fault) goto MMU_EXCEPTION;

			}
			else{
				cpu->Reg[dest_reg] = 1;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STRH_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xffff;
			//bus_write(16, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 16);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		//if (BITS(inst_cream->inst, 12, 15) == 15)
		//	goto DISPATCH;
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	STRT_INST:
	{
		INC_ICOUNTER;
		ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			fault = inst_cream->get_addr(cpu, inst_cream->inst, addr, phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)];
			//bus_write(16, addr, value);
			fault = interpreter_write_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(ldst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SUB_INST:
	{
		INC_ICOUNTER;
		sub_inst *inst_cream = (sub_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			lop = RN;
			if (inst_cream->Rn == 15) {
				lop += 8;
			}
			rop = SHIFTER_OPERAND;
			RD = dst = lop - rop;
			if (inst_cream->S && (inst_cream->Rd == 15)) {
				/* cpsr = spsr */
				if (CurrentModeHasSPSR) {
					cpu->Cpsr = cpu->Spsr_copy;
					switch_mode(cpu, cpu->Spsr_copy & 0x1f);
					LOAD_NZCVT;
				}
			} else if (inst_cream->S) {
				UPDATE_NFLAG(dst);
				UPDATE_ZFLAG(dst);
//				UPDATE_CFLAG(dst, lop, rop);
				UPDATE_CFLAG_NOT_BORROW_FROM(lop, rop);
	//			UPDATE_VFLAG((int)dst, (int)lop, (int)rop);
				UPDATE_VFLAG_OVERFLOW_FROM(dst, lop, rop);
			}
			if (inst_cream->Rd == 15) {
				INC_PC(sizeof(sub_inst));
				goto DISPATCH;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(sub_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SWI_INST:
	{
		INC_ICOUNTER;
		swi_inst *inst_cream = (swi_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			if (true){ //if (core->is_user_mode) { --> Citra only emulates user mode
				//arm_dyncom_SWI(cpu, inst_cream->num);
				HLE::CallSVC(Memory::Read32(cpu->Reg[15]));
			} else {
				cpu->syscallSig = 1;
				goto END;
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(swi_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SWP_INST:
	{
		INC_ICOUNTER;
		swp_inst *inst_cream = (swp_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = RN;
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 32);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_write_memory(addr, phys_addr, RM, 32);
			if (fault) goto MMU_EXCEPTION;

			/* ROR(data, 8*UInt(address<1:0>)); */
			assert((phys_addr & 0x3) == 0);
			RD = value;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(swp_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SWPB_INST:
	{
		INC_ICOUNTER;
		swp_inst *inst_cream = (swp_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			addr = RN;
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			unsigned int value;
			fault = interpreter_read_memory(addr, phys_addr, value, 8);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_write_memory(addr, phys_addr, (RM & 0xFF), 8);
			if (fault) goto MMU_EXCEPTION;

			/* FIXME */
			#if 0
			if Shared(address) then
			/* ARMv6 */
			physical_address = TLB(address)
			ClearExclusiveByAddress(physical_address,processor_id,1)
			/* See Summary of operation on page A2-49 */
			#endif
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(swp_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SXTAB_INST:
	{
		INC_ICOUNTER;
		sxtab_inst *inst_cream = (sxtab_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			/* R15 should be check */
			if(inst_cream->Rn == 15 || inst_cream->Rm == 15 || inst_cream->Rd ==15){
				CITRA_IGNORE_EXIT(-1);
			}
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) 
						& 0xff;
			/* sign extend for byte */
			operand2 = (0x80 & operand2)? (0xFFFFFF00 | operand2):operand2;
			RD = RN + operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(uxtab_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SXTAB16_INST:
	SXTAH_INST:
	{
		INC_ICOUNTER;
		sxtah_inst *inst_cream = (sxtah_inst *)inst_base->component;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			/* R15 should be check */
			if(inst_cream->Rn == 15 || inst_cream->Rm == 15 || inst_cream->Rd ==15){
				CITRA_IGNORE_EXIT(-1);
			}
			unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xffff;
			/* sign extend for half */
			operand2 = (0x8000 & operand2)? (0xFFFF0000 | operand2):operand2;
			RD = RN + operand2;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(sxtah_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	SXTB16_INST:
	TEQ_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			teq_inst *inst_cream = (teq_inst *)inst_base->component;
			lop = RN;
			if (inst_cream->Rn == 15)
				lop += GET_INST_SIZE(cpu) * 2;

			rop = SHIFTER_OPERAND;
			dst = lop ^ rop;

			UPDATE_NFLAG(dst);
			UPDATE_ZFLAG(dst);
			UPDATE_CFLAG_WITH_SC;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(teq_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	TST_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			tst_inst *inst_cream = (tst_inst *)inst_base->component;
			lop = RN;
			if (inst_cream->Rn == 15)
				lop += GET_INST_SIZE(cpu) * 2;
			rop = SHIFTER_OPERAND;
			dst = lop & rop;

			UPDATE_NFLAG(dst);
			UPDATE_ZFLAG(dst);
			UPDATE_CFLAG_WITH_SC;
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(tst_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UADD8_INST:
	UADD16_INST:
	UADDSUBX_INST:

	UHADD8_INST:
	UHADD16_INST:
	UHADDSUBX_INST:
	UHSUBADDX_INST:
	UHSUB8_INST:
	UHSUB16_INST:
	{
		INC_ICOUNTER;
		if (inst_base->cond == 0xE || CondPassed(cpu, inst_base->cond)) {
			generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

			const u32 rm_val = RM;
			const u32 rn_val = RN;
			const u8 op2 = inst_cream->op2;

			
			if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03)
			{
				u32 lo_val = 0;
				u32 hi_val = 0;
				
				// UHADD16
				if (op2 == 0x00) {
					lo_val = (rn_val & 0xFFFF) + (rm_val & 0xFFFF);
					hi_val = ((rn_val >> 16) & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);
				}
				// UHASX
				else if (op2 == 0x01) {
					lo_val = (rn_val & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);
					hi_val = ((rn_val >> 16) & 0xFFFF) + (rm_val & 0xFFFF);
				}
				// UHSAX
				else if (op2 == 0x02) {
					lo_val = (rn_val & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);
					hi_val = ((rn_val >> 16) & 0xFFFF) - (rm_val & 0xFFFF);
				}
				// UHSUB16
				else if (op2 == 0x03) {
					lo_val = (rn_val & 0xFFFF) - (rm_val & 0xFFFF);
					hi_val = ((rn_val >> 16) & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);
				}
				
				lo_val >>= 1;
				hi_val >>= 1;
				
				RD = (lo_val & 0xFFFF) | ((hi_val & 0xFFFF) << 16);
			}
			else if (op2 == 0x04 || op2 == 0x07) {
				u32 sum1;
				u32 sum2;
				u32 sum3;
				u32 sum4;
				
				// UHADD8
				if (op2 == 0x04) {
					sum1 = (rn_val & 0xFF) + (rm_val & 0xFF);
					sum2 = ((rn_val >> 8) & 0xFF) + ((rm_val >> 8) & 0xFF);
					sum3 = ((rn_val >> 16) & 0xFF) + ((rm_val >> 16) & 0xFF);
					sum4 = ((rn_val >> 24) & 0xFF) + ((rm_val >> 24) & 0xFF);
				}
				// UHSUB8
				else {
					sum1 = (rn_val & 0xFF) - (rm_val & 0xFF);
					sum2 = ((rn_val >> 8) & 0xFF) - ((rm_val >> 8) & 0xFF);
					sum3 = ((rn_val >> 16) & 0xFF) - ((rm_val >> 16) & 0xFF);
					sum4 = ((rn_val >> 24) & 0xFF) - ((rm_val >> 24) & 0xFF);
				}
				
				sum1 >>= 1;
				sum2 >>= 1;
				sum3 >>= 1;
				sum4 >>= 1;
				
				RD = (sum1 & 0xFF) | ((sum2 & 0xFF) << 8) | ((sum3 & 0xFF) << 16) | ((sum4 & 0xFF) << 24);
			}
			
		}

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(generic_arm_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	
	
	UMAAL_INST:
	{
		INC_ICOUNTER;
		if (inst_base->cond == 0xE || CondPassed(cpu, inst_base->cond)) {
			umaal_inst* const inst_cream = (umaal_inst*)inst_base->component;

			const u32 rm = RM;
			const u32 rn = RN;
			const u32 rd_lo = RDLO;
			const u32 rd_hi = RDHI;

			const u64 result = (rm * rn) + rd_lo + rd_hi;

			RDLO = (result & 0xFFFFFFFF);
			RDHI = ((result >> 32) & 0xFFFFFFFF);
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umaal_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UMLAL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			umlal_inst *inst_cream = (umlal_inst *)inst_base->component;
			unsigned long long int rm = RM;
			unsigned long long int rs = RS;
			unsigned long long int rst = rm * rs;
			unsigned long long int add = ((unsigned long long) RDHI)<<32;
			add += RDLO;
			//DEBUG_LOG(ARM11, "rm[%llx] * rs[%llx] = rst[%llx] | add[%llx]\n", RM, RS, rst, add);
			rst += add;
			RDLO = BITS(rst,  0, 31);
			RDHI = BITS(rst, 32, 63);
			
			if (inst_cream->S)
			{
				cpu->NFlag = BIT(RDHI, 31);
				cpu->ZFlag = (RDHI == 0 && RDLO == 0);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umlal_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	UMULL_INST:
	{
		INC_ICOUNTER;
		if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
			umull_inst *inst_cream = (umull_inst *)inst_base->component;
			unsigned long long int rm = RM;
			unsigned long long int rs = RS;
			unsigned long long int rst = rm * rs;
//			DEBUG_LOG(ARM11, "rm : [%llx] rs : [%llx] rst [%llx]\n", RM, RS, rst);
			RDHI = BITS(rst, 32, 63);
			RDLO = BITS(rst,  0, 31);

			if (inst_cream->S) {
				cpu->NFlag = BIT(RDHI, 31);
				cpu->ZFlag = (RDHI == 0 && RDLO == 0);
			}
		}
		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(umull_inst));
		FETCH_INST;
		GOTO_NEXT_INST;
	}
	B_2_THUMB:
	{
		INC_ICOUNTER;
		b_2_thumb *inst_cream = (b_2_thumb *)inst_base->component;
		cpu->Reg[15] = cpu->Reg[15] + 4 + inst_cream->imm;
		//DEBUG_LOG(ARM11, " BL_1_THUMB: imm=0x%x, r14=0x%x, r15=0x%x\n", inst_cream->imm, cpu->Reg[14], cpu->Reg[15]);
		INC_PC(sizeof(b_2_thumb));
		goto DISPATCH;
	}
	B_COND_THUMB:
	{
		INC_ICOUNTER;
		b_cond_thumb *inst_cream = (b_cond_thumb *)inst_base->component;
		if(CondPassed(cpu, inst_cream->cond))
			cpu->Reg[15] = cpu->Reg[15] + 4 + inst_cream->imm;
		else
			cpu->Reg[15] += 2;
		//DEBUG_LOG(ARM11, " B_COND_THUMB: imm=0x%x, r15=0x%x\n", inst_cream->imm, cpu->Reg[15]);
		INC_PC(sizeof(b_cond_thumb));
		goto DISPATCH;
	}
	BL_1_THUMB:
	{
		INC_ICOUNTER;
		bl_1_thumb *inst_cream = (bl_1_thumb *)inst_base->component;
		cpu->Reg[14] = cpu->Reg[15] + 4 + inst_cream->imm;
		//cpu->Reg[15] += 2;
		//DEBUG_LOG(ARM11, " BL_1_THUMB: imm=0x%x, r14=0x%x, r15=0x%x\n", inst_cream->imm, cpu->Reg[14], cpu->Reg[15]);

		cpu->Reg[15] += GET_INST_SIZE(cpu);
		INC_PC(sizeof(bl_1_thumb));
		FETCH_INST;
		GOTO_NEXT_INST;

	}
	BL_2_THUMB:
	{
		INC_ICOUNTER;
		bl_2_thumb *inst_cream = (bl_2_thumb *)inst_base->component;
		int tmp = ((cpu->Reg[15] + 2) | 1);
		cpu->Reg[15] =
			(cpu->Reg[14] + inst_cream->imm);
		cpu->Reg[14] = tmp;
		//DEBUG_LOG(ARM11, " BL_2_THUMB: imm=0x%x, r14=0x%x, r15=0x%x\n", inst_cream->imm, cpu->Reg[14], cpu->Reg[15]);
		INC_PC(sizeof(bl_2_thumb));
		goto DISPATCH;
	}
	BLX_1_THUMB:
	{	
		/* BLX 1 for armv5t and above */
		INC_ICOUNTER;
		uint32 tmp = cpu->Reg[15];
		blx_1_thumb *inst_cream = (blx_1_thumb *)inst_base->component;
		cpu->Reg[15] = (cpu->Reg[14] + inst_cream->imm) & 0xFFFFFFFC;
		//DEBUG_LOG(ARM11, "In BLX_1_THUMB, BLX(1),imm=0x%x,r14=0x%x, instr=0x%x\n", inst_cream->imm, cpu->Reg[14], inst_cream->instr);
		cpu->Reg[14] = ((tmp + 2) | 1);
		//(state->Reg[14] + ((tinstr & 0x07FF) << 1)) & 0xFFFFFFFC;
		/* switch to arm state from thumb state */
		cpu->TFlag = 0;
		//DEBUG_LOG(ARM11, "In BLX_1_THUMB, BLX(1),imm=0x%x,r14=0x%x, r15=0x%x, \n", inst_cream->imm, cpu->Reg[14], cpu->Reg[15]);
		INC_PC(sizeof(blx_1_thumb));
		goto DISPATCH;
	}

	UQADD16_INST:
	UQADD8_INST:
	UQADDSUBX_INST:
	UQSUB16_INST:
	UQSUB8_INST:
	UQSUBADDX_INST:
	USAD8_INST:
	USADA8_INST:
	USAT_INST:
	USAT16_INST:
	USUB16_INST:
	USUB8_INST:
	USUBADDX_INST:
	UXTAB16_INST:
	UXTB16_INST:
	#define VFP_INTERPRETER_IMPL
	#include "core/arm/skyeye_common/vfp/vfpinstr.cpp"
	#undef VFP_INTERPRETER_IMPL
	MMU_EXCEPTION:
	{
		SAVE_NZCVT;
		cpu->abortSig = true;
		cpu->Aborted = ARMul_DataAbortV;
		cpu->AbortAddr = addr;
		cpu->CP15[CP15(CP15_FAULT_STATUS)] = fault & 0xff;
		cpu->CP15[CP15(CP15_FAULT_ADDRESS)] = addr;
		cpu->NumInstrsToExecute = 0;
		return num_instrs;
	}
	END:
	{
		SAVE_NZCVT;
		cpu->NumInstrsToExecute = 0;
		return num_instrs;
	}
	INIT_INST_LENGTH:
	{
#if 0
	DEBUG_LOG(ARM11, "InstLabel:%d\n", sizeof(InstLabel));
	for (int i = 0; i < (sizeof(InstLabel) / sizeof(void *)); i ++)
		DEBUG_LOG(ARM11, "[%llx]\n", InstLabel[i]);
	DEBUG_LOG(ARM11, "InstLabel:%d\n", sizeof(InstLabel));
#endif
#if defined __GNUC__ || defined __clang__
	InterpreterInitInstLength((unsigned long long int *)InstLabel, sizeof(InstLabel));
#endif
#if 0
	for (int i = 0; i < (sizeof(InstLabel) / sizeof(void *)); i ++)
		DEBUG_LOG(ARM11, "[%llx]\n", InstLabel[i]);
	DEBUG_LOG(ARM11, "%llx\n", InstEndLabel[1]);
	DEBUG_LOG(ARM11, "%llx\n", InstLabel[1]);
	DEBUG_LOG(ARM11, "%lld\n", (char *)InstEndLabel[1] - (char *)InstLabel[1]);
#endif
	cpu->NumInstrsToExecute = 0;
	return num_instrs;
	}
}

