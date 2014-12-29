/*
    vfp/vfpinstr.c - ARM VFPv3 emulation unit - Individual instructions data
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

/* Notice: this file should not be compiled as is, and is meant to be
   included in other files only. */

/* ----------------------------------------------------------------------- */
/* CDP instructions */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */

/* ----------------------------------------------------------------------- */
/* VMLA */
/* cond 1110 0D00 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmla_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vmla_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmla)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmla_inst));
	vmla_inst *inst_cream = (vmla_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMLA_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VMLA :\n");
		
		vmla_inst *inst_cream = (vmla_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmla_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmla),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmla)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmla)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int add = (BIT(6) == 0);
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG32(tmp);
		mm = FR32(d);
		tmp = FPADD(mm,tmp);
		//LETS(d,tmp);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * m)));
		nn = ZEXT64(IBITCAST32(FR32(2 * m  + 1)));
		tmp = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(tmp);
		tmp = ZEXT64(IBITCAST32(FR32(2 * n)));
		nn = ZEXT64(IBITCAST32(FR32(2 * n  + 1)));
		nn = OR(SHL(nn,CONST64(32)),tmp);
		nn = FPBITCAST64(nn);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG64(tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * d)));
		nn = ZEXT64(IBITCAST32(FR32(2 * d  + 1)));
		mm = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(mm);
		tmp = FPADD(mm,tmp);
		mm = TRUNC32(LSHR(IBITCAST64(tmp),CONST64(32)));
		nn = TRUNC32(AND(IBITCAST64(tmp),CONST64(0xffffffff)));	
		LETFPS(2*d ,FPBITCAST32(nn));
		LETFPS(d*2 + 1 , FPBITCAST32(mm));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLS */
/* cond 1110 0D00 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmls_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vmls_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmls)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmls_inst));
	vmls_inst *inst_cream = (vmls_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMLS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VMLS :\n");
		
		vmls_inst *inst_cream = (vmls_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmls_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmls),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmls)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmls)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s VMLS instruction is executed out of here.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int add = (BIT(6) == 0);
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG32(tmp);
		mm = FR32(d);
		tmp = FPADD(mm,tmp);
		//LETS(d,tmp);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * m)));
		nn = ZEXT64(IBITCAST32(FR32(2 * m  + 1)));
		tmp = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(tmp);
		tmp = ZEXT64(IBITCAST32(FR32(2 * n)));
		nn = ZEXT64(IBITCAST32(FR32(2 * n  + 1)));
		nn = OR(SHL(nn,CONST64(32)),tmp);
		nn = FPBITCAST64(nn);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG64(tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * d)));
		nn = ZEXT64(IBITCAST32(FR32(2 * d  + 1)));
		mm = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(mm);
		tmp = FPADD(mm,tmp);
		mm = TRUNC32(LSHR(IBITCAST64(tmp),CONST64(32)));
		nn = TRUNC32(AND(IBITCAST64(tmp),CONST64(0xffffffff)));	
		LETFPS(2*d ,FPBITCAST32(nn));
		LETFPS(d*2 + 1 , FPBITCAST32(mm));
	}	
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLA */
/* cond 1110 0D01 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vnmla_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vnmla_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vnmla)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vnmla_inst));
	vnmla_inst *inst_cream = (vnmla_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMLA_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VNMLA :\n");

		vnmla_inst *inst_cream = (vnmla_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vnmla_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vnmla),
DYNCOM_FILL_ACTION(vnmla),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vnmla)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vnmla)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s VNMLA instruction is executed out of here.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int add = (BIT(6) == 0);
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG32(tmp);
		mm = FR32(d);
		tmp = FPADD(FPNEG32(mm),tmp);
		//LETS(d,tmp);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * m)));
		nn = ZEXT64(IBITCAST32(FR32(2 * m  + 1)));
		tmp = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(tmp);
		tmp = ZEXT64(IBITCAST32(FR32(2 * n)));
		nn = ZEXT64(IBITCAST32(FR32(2 * n  + 1)));
		nn = OR(SHL(nn,CONST64(32)),tmp);
		nn = FPBITCAST64(nn);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG64(tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * d)));
		nn = ZEXT64(IBITCAST32(FR32(2 * d  + 1)));
		mm = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(mm);
		tmp = FPADD(FPNEG64(mm),tmp);
		mm = TRUNC32(LSHR(IBITCAST64(tmp),CONST64(32)));	
		nn = TRUNC32(AND(IBITCAST64(tmp),CONST64(0xffffffff)));
		LETFPS(2*d ,FPBITCAST32(nn));
		LETFPS(d*2 + 1 , FPBITCAST32(mm));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMLS */
/* cond 1110 0D01 Vn-- Vd-- 101X N0M0 Vm-- */

#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vnmls_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vnmls_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vnmls)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vnmls_inst));
	vnmls_inst *inst_cream = (vnmls_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMLS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VNMLS :\n");

		vnmls_inst *inst_cream = (vnmls_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vnmls_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vnmls),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vnmls)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vnmls)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int add = (BIT(6) == 0);
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG32(tmp);
		mm = FR32(d);
		tmp = FPADD(FPNEG32(mm),tmp);
		//LETS(d,tmp);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * m)));
		nn = ZEXT64(IBITCAST32(FR32(2 * m  + 1)));
		tmp = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(tmp);
		tmp = ZEXT64(IBITCAST32(FR32(2 * n)));
		nn = ZEXT64(IBITCAST32(FR32(2 * n  + 1)));
		nn = OR(SHL(nn,CONST64(32)),tmp);
		nn = FPBITCAST64(nn);
		tmp = FPMUL(nn,mm);
		if(!add)
			tmp = FPNEG64(tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * d)));
		nn = ZEXT64(IBITCAST32(FR32(2 * d  + 1)));
		mm = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(mm);
		tmp = FPADD(FPNEG64(mm),tmp);
		mm = TRUNC32(LSHR(IBITCAST64(tmp),CONST64(32)));
		nn = TRUNC32(AND(IBITCAST64(tmp),CONST64(0xffffffff)));	
		LETFPS(2*d ,FPBITCAST32(nn));
		LETFPS(d*2 + 1 , FPBITCAST32(mm));
	}	
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNMUL */
/* cond 1110 0D10 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vnmul_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vnmul_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vnmul)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vnmul_inst));
	vnmul_inst *inst_cream = (vnmul_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNMUL_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VNMUL :\n");

		vnmul_inst *inst_cream = (vnmul_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vnmul_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vnmul),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vnmul)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}		
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vnmul)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int add = (BIT(6) == 0);
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		//LETS(d,tmp);
		LETFPS(d,FPNEG32(tmp));
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		mm = ZEXT64(IBITCAST32(FR32(2 * m)));
		nn = ZEXT64(IBITCAST32(FR32(2 * m  + 1)));
		tmp = OR(SHL(nn,CONST64(32)),mm);
		mm = FPBITCAST64(tmp);
		tmp = ZEXT64(IBITCAST32(FR32(2 * n)));
		nn = ZEXT64(IBITCAST32(FR32(2 * n  + 1)));
		nn = OR(SHL(nn,CONST64(32)),tmp);
		nn = FPBITCAST64(nn);
		tmp = FPMUL(nn,mm);
		tmp = FPNEG64(tmp);
		mm = TRUNC32(LSHR(IBITCAST64(tmp),CONST64(32)));
		nn = TRUNC32(AND(IBITCAST64(tmp),CONST64(0xffffffff)));	
		LETFPS(2*d ,FPBITCAST32(nn));
		LETFPS(d*2 + 1 , FPBITCAST32(mm));
	}
	return No_exp;
}
#endif


/* ----------------------------------------------------------------------- */
/* VMUL */
/* cond 1110 0D10 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmul_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vmul_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmul)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmul_inst));
	vmul_inst *inst_cream = (vmul_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMUL_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VMUL :\n");

		vmul_inst *inst_cream = (vmul_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmul_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmul),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmul)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmul)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//printf("\n\n\t\tin %s instruction is executed out.\n\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		//mm = SITOFP(32,FR(m));
		//nn = SITOFP(32,FRn));
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPMUL(nn,mm);
		//LETS(d,tmp);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		//mm = SITOFP(32,RSPR(m));
		//LETS(d,tmp);
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value* m0 = FPBITCAST64(v64);
		lo = FR32(2 * n);
		hi = FR32(2 * n + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		hi64 = ZEXT64(hi);
		lo64 = ZEXT64(lo);
		v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value *n0 = FPBITCAST64(v64); 
		tmp = FPMUL(n0,m0);
		Value *val64 = IBITCAST64(tmp);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VADD */
/* cond 1110 0D11 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vadd_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vadd_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vadd)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vadd_inst));
	vadd_inst *inst_cream = (vadd_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VADD_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VADD :\n");

		vadd_inst *inst_cream = (vadd_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vadd_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vadd),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vadd)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vadd)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction will implement out of JIT.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPADD(nn,mm);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value* m0 = FPBITCAST64(v64);
		lo = FR32(2 * n);
		hi = FR32(2 * n + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		hi64 = ZEXT64(hi);
		lo64 = ZEXT64(lo);
		v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value *n0 = FPBITCAST64(v64); 
		tmp = FPADD(n0,m0);
		Value *val64 = IBITCAST64(tmp);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSUB */
/* cond 1110 0D11 Vn-- Vd-- 101X N1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vsub_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vsub_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vsub)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vsub_inst));
	vsub_inst *inst_cream = (vsub_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSUB_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VSUB :\n");

		vsub_inst *inst_cream = (vsub_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vsub_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif
#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vsub),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vsub)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vsub)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instr=0x%x, instruction is executed out of JIT.\n", __FUNCTION__, instr);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPSUB(nn,mm);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value* m0 = FPBITCAST64(v64);
		lo = FR32(2 * n);
		hi = FR32(2 * n + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		hi64 = ZEXT64(hi);
		lo64 = ZEXT64(lo);
		v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value *n0 = FPBITCAST64(v64); 
		tmp = FPSUB(n0,m0);
		Value *val64 = IBITCAST64(tmp);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	} 
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VDIV */
/* cond 1110 1D00 Vn-- Vd-- 101X N0M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vdiv_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vdiv_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vdiv)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vdiv_inst));
	vdiv_inst *inst_cream = (vdiv_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VDIV_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VDIV :\n");

		vdiv_inst *inst_cream = (vdiv_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vdiv_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vdiv),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vdiv)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vdiv)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int m;
	int n;
	int d ;
	int s = BIT(8) == 0;
	Value *mm;
	Value *nn;
	Value *tmp;
	if(s){
		m = BIT(5) | BITS(0,3) << 1;
		n = BIT(7) | BITS(16,19) << 1;
		d = BIT(22) | BITS(12,15) << 1;
		mm = FR32(m);
		nn = FR32(n);
		tmp = FPDIV(nn,mm);
		LETFPS(d,tmp);
	}else {
		m = BITS(0,3) | BIT(5) << 4;
		n = BITS(16,19) | BIT(7) << 4;
		d = BIT(22) << 4 | BITS(12,15);
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value* m0 = FPBITCAST64(v64);
		lo = FR32(2 * n);
		hi = FR32(2 * n + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		hi64 = ZEXT64(hi);
		lo64 = ZEXT64(lo);
		v64 = OR(SHL(hi64,CONST64(32)),lo64);
		Value *n0 = FPBITCAST64(v64); 
		tmp = FPDIV(n0,m0);
		Value *val64 = IBITCAST64(tmp);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	} 		
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVI move immediate */
/* cond 1110 1D11 im4H Vd-- 101X 0000 im4L */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovi_inst {
	unsigned int single;
	unsigned int d;
	unsigned int imm;
} vmovi_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovi)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovi_inst));
	vmovi_inst *inst_cream = (vmovi_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;
	
	inst_cream->single   = BIT(inst, 8) == 0;
	inst_cream->d        = (inst_cream->single ? BITS(inst,12,15)<<1 | BIT(inst,22) : BITS(inst,12,15) | BIT(inst,22)<<4);
	unsigned int imm8 = BITS(inst, 16, 19) << 4 | BITS(inst, 0, 3);
	if (inst_cream->single)
		inst_cream->imm = BIT(imm8, 7)<<31 | (BIT(imm8, 6)==0)<<30 | (BIT(imm8, 6) ? 0x1f : 0)<<25 | BITS(imm8, 0, 5)<<19;
	else
		inst_cream->imm = BIT(imm8, 7)<<31 | (BIT(imm8, 6)==0)<<30 | (BIT(imm8, 6) ? 0xff : 0)<<22 | BITS(imm8, 0, 5)<<16;
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVI_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovi_inst *inst_cream = (vmovi_inst *)inst_base->component;

		VMOVI(cpu, inst_cream->single, inst_cream->d, inst_cream->imm);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovi_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovi),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovi)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovi)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int single = (BIT(8) == 0);
	int d;
	int imm32;
	Value *v;
	Value *tmp;
	v = CONST32(BITS(0,3) | BITS(16,19) << 4);
	//v = CONST64(0x3ff0000000000000);
	if(single){
		d = BIT(22) | BITS(12,15) << 1;
	}else {
		d = BITS(12,15) | BIT(22) << 4;
	}
	if(single){
		LETFPS(d,FPBITCAST32(v));
	}else {
		//v = UITOFP(64,v);
		//tmp = IBITCAST64(v);
		LETFPS(d*2 ,FPBITCAST32(TRUNC32(AND(v,CONST64(0xffffffff)))));
		LETFPS(d * 2 + 1,FPBITCAST32(TRUNC32(LSHR(v,CONST64(32)))));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVR move register */
/* cond 1110 1D11 0000 Vd-- 101X 01M0 Vm-- */
/* cond 1110 opc1 CRn- CRd- copr op20 CRm- CDP */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovr_inst {
	unsigned int single;
	unsigned int d;
	unsigned int m;
} vmovr_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovr)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	VFP_DEBUG_UNTESTED(VMOVR);
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovr_inst));
	vmovr_inst *inst_cream = (vmovr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;
	
	inst_cream->single   = BIT(inst, 8) == 0;
	inst_cream->d        = (inst_cream->single ? BITS(inst,12,15)<<1 | BIT(inst,22) : BITS(inst,12,15) | BIT(inst,22)<<4);
	inst_cream->m        = (inst_cream->single ? BITS(inst, 0, 3)<<1 | BIT(inst, 5) : BITS(inst, 0, 3) | BIT(inst, 5)<<4);
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVR_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovr_inst *inst_cream = (vmovr_inst *)inst_base->component;

		VMOVR(cpu, inst_cream->single, inst_cream->d, inst_cream->m);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovr_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovr),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovr)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovr)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s VMOV \n", __FUNCTION__);
	int single   = BIT(8) == 0;
	int d        = (single ? BITS(12,15)<<1 | BIT(22) : BIT(22) << 4 | BITS(12,15));
	int m        = (single ? BITS(0, 3)<<1 | BIT(5) : BITS(0, 3) | BIT(5)<<4);

	if (single)
	{
		LETFPS(d, FR32(m));
	}
	else
	{
		/* Check endian please */
		LETFPS((d*2 + 1), FR32(m*2 + 1));
		LETFPS((d * 2), FR32(m * 2));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VABS */
/* cond 1110 1D11 0000 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vabs_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vabs_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vabs)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;VFP_DEBUG_UNTESTED(VABS);
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vabs_inst));
	vabs_inst *inst_cream = (vabs_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VABS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VABS :\n");

		vabs_inst *inst_cream = (vabs_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vabs_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vabs),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vabs)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vabs)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int single   = BIT(8) == 0;
	int d        = (single ? BITS(12,15)<<1 | BIT(22) : BIT(22) << 4 | BITS(12,15));
	int m        = (single ? BITS(0, 3)<<1 | BIT(5) : BITS(0, 3) | BIT(5)<<4);
	Value* m0;
	if (single)
	{
		m0 =  FR32(m);
		m0 = SELECT(FPCMP_OLT(m0,FPCONST32(0.0)),FPNEG32(m0),m0);
		LETFPS(d,m0);
	}
	else
	{
		/* Check endian please */
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		m0 = FPBITCAST64(v64);
		m0 = SELECT(FPCMP_OLT(m0,FPCONST64(0.0)),FPNEG64(m0),m0);
		Value *val64 = IBITCAST64(m0);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VNEG */
/* cond 1110 1D11 0001 Vd-- 101X 11M0 Vm-- */

#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vneg_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vneg_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vneg)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;VFP_DEBUG_UNTESTED(VNEG);
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vneg_inst));
	vneg_inst *inst_cream = (vneg_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VNEG_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;

		DBG("VNEG :\n");

		vneg_inst *inst_cream = (vneg_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vneg_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vneg),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vneg)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vneg)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int single   = BIT(8) == 0;
	int d        = (single ? BITS(12,15)<<1 | BIT(22) : BIT(22) << 4 | BITS(12,15));
	int m        = (single ? BITS(0, 3)<<1 | BIT(5) : BITS(0, 3) | BIT(5)<<4);
	Value* m0;
	if (single)
	{
		m0 =  FR32(m);
		m0 = FPNEG32(m0);
		LETFPS(d,m0);
	}
	else
	{
		/* Check endian please */
		Value *lo = FR32(2 * m);
		Value *hi = FR32(2 * m + 1);
		hi = IBITCAST32(hi);
		lo = IBITCAST32(lo);
		Value *hi64 = ZEXT64(hi);
		Value* lo64 = ZEXT64(lo);
		Value* v64 = OR(SHL(hi64,CONST64(32)),lo64);
		m0 = FPBITCAST64(v64);
		m0 = FPNEG64(m0);
		Value *val64 = IBITCAST64(m0);
		hi = LSHR(val64,CONST64(32));
		lo = AND(val64,CONST64(0xffffffff));
		hi = TRUNC32(hi);
		lo  = TRUNC32(lo);
		hi = FPBITCAST32(hi);
		lo = FPBITCAST32(lo);		
		LETFPS(2*d ,lo);
		LETFPS(d*2 + 1 , hi);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSQRT */
/* cond 1110 1D11 0001 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vsqrt_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vsqrt_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vsqrt)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vsqrt_inst));
	vsqrt_inst *inst_cream = (vsqrt_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSQRT_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VSQRT :\n");
		
		vsqrt_inst *inst_cream = (vsqrt_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vsqrt_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vsqrt),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vsqrt)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vsqrt)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int dp_op = (BIT(8) == 1);
	int d = dp_op ? BITS(12,15) | BIT(22) << 4 : BIT(22) | BITS(12,15) << 1;
	int m = dp_op ? BITS(0,3) | BIT(5) << 4 : BIT(5) | BITS(0,3) << 1;
	Value* v;
	Value* tmp;
	if(dp_op){
		v = SHL(ZEXT64(IBITCAST32(FR32(2 * m + 1))),CONST64(32));
		tmp = ZEXT64(IBITCAST32(FR32(2 * m)));
		v = OR(v,tmp);
		v = FPSQRT(FPBITCAST64(v));
		tmp = TRUNC32(LSHR(IBITCAST64(v),CONST64(32)));
		v = TRUNC32(AND(IBITCAST64(v),CONST64( 0xffffffff)));		
		LETFPS(2 * d , FPBITCAST32(v));
		LETFPS(2 * d + 1, FPBITCAST32(tmp));
	}else {
		v = FR32(m);
		v = FPSQRT(FPEXT(64,v));
		v = FPTRUNC(32,v);
		LETFPS(d,v);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCMP VCMPE */
/* cond 1110 1D11 0100 Vd-- 101X E1M0 Vm-- Encoding 1 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vcmp_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vcmp_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vcmp)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vcmp_inst));
	vcmp_inst *inst_cream = (vcmp_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCMP_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;

		DBG("VCMP(1) :\n");

		vcmp_inst *inst_cream = (vcmp_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vcmp_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vcmp),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vcmp)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vcmp)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is executed out of JIT.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int dp_op = (BIT(8) == 1);
	int d = dp_op ? BITS(12,15) | BIT(22) << 4 : BIT(22) | BITS(12,15) << 1;
	int m = dp_op ? BITS(0,3) | BIT(5) << 4 : BIT(5) | BITS(0,3) << 1;
	Value* v;
	Value* tmp;
	Value* n;
	Value* z;
	Value* c;
	Value* vt;
	Value* v1;
	Value* nzcv;
	if(dp_op){
		v = SHL(ZEXT64(IBITCAST32(FR32(2 * m + 1))),CONST64(32));
		tmp = ZEXT64(IBITCAST32(FR32(2 * m)));
		v1 = OR(v,tmp);
		v = SHL(ZEXT64(IBITCAST32(FR32(2 * d + 1))),CONST64(32));
		tmp = ZEXT64(IBITCAST32(FR32(2 * d)));
		v = OR(v,tmp);
		z = FPCMP_OEQ(FPBITCAST64(v),FPBITCAST64(v1));
		n = FPCMP_OLT(FPBITCAST64(v),FPBITCAST64(v1));
		c = FPCMP_OGE(FPBITCAST64(v),FPBITCAST64(v1)); 
		tmp =  FPCMP_UNO(FPBITCAST64(v),FPBITCAST64(v1));
		v1 = tmp;
		c = OR(c,tmp);
		n = SHL(ZEXT32(n),CONST32(31));
		z = SHL(ZEXT32(z),CONST32(30));
		c = SHL(ZEXT32(c),CONST32(29));
		v1 = SHL(ZEXT32(v1),CONST(28));
		nzcv = OR(OR(OR(n,z),c),v1);	
		v = R(VFP_FPSCR);
		tmp = OR(nzcv,AND(v,CONST32(0x0fffffff)));
		LET(VFP_FPSCR,tmp);
	}else {
		z = FPCMP_OEQ(FR32(d),FR32(m));
		n = FPCMP_OLT(FR32(d),FR32(m));
		c = FPCMP_OGE(FR32(d),FR32(m)); 
		tmp = FPCMP_UNO(FR32(d),FR32(m));
		c = OR(c,tmp);
		v1 = tmp;
		n = SHL(ZEXT32(n),CONST32(31));
		z = SHL(ZEXT32(z),CONST32(30));
		c = SHL(ZEXT32(c),CONST32(29));
		v1 = SHL(ZEXT32(v1),CONST(28));
		nzcv = OR(OR(OR(n,z),c),v1);	
		v = R(VFP_FPSCR);
		tmp = OR(nzcv,AND(v,CONST32(0x0fffffff)));
		LET(VFP_FPSCR,tmp);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCMP VCMPE */
/* cond 1110 1D11 0100 Vd-- 101X E1M0 Vm-- Encoding 2 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vcmp2_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vcmp2_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vcmp2)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vcmp2_inst));
	vcmp2_inst *inst_cream = (vcmp2_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCMP2_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VCMP(2) :\n");

		vcmp2_inst *inst_cream = (vcmp2_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vcmp2_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vcmp2),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vcmp2)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vcmp2)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction will executed out of JIT.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int dp_op = (BIT(8) == 1);
	int d = dp_op ? BITS(12,15) | BIT(22) << 4 : BIT(22) | BITS(12,15) << 1;
	//int m = dp_op ? BITS(0,3) | BIT(5) << 4 : BIT(5) | BITS(0,3) << 1;
	Value* v;
	Value* tmp;
	Value* n;
	Value* z;
	Value* c;
	Value* vt;
	Value* v1;
	Value* nzcv;
	if(dp_op){
		v1 = CONST64(0);
		v = SHL(ZEXT64(IBITCAST32(FR32(2 * d + 1))),CONST64(32));
		tmp = ZEXT64(IBITCAST32(FR32(2 * d)));
		v = OR(v,tmp);
		z = FPCMP_OEQ(FPBITCAST64(v),FPBITCAST64(v1));
		n = FPCMP_OLT(FPBITCAST64(v),FPBITCAST64(v1));
		c = FPCMP_OGE(FPBITCAST64(v),FPBITCAST64(v1)); 
		tmp =  FPCMP_UNO(FPBITCAST64(v),FPBITCAST64(v1));
		v1 = tmp;
		c = OR(c,tmp);
		n = SHL(ZEXT32(n),CONST32(31));
		z = SHL(ZEXT32(z),CONST32(30));
		c = SHL(ZEXT32(c),CONST32(29));
		v1 = SHL(ZEXT32(v1),CONST(28));
		nzcv = OR(OR(OR(n,z),c),v1);	
		v = R(VFP_FPSCR);
		tmp = OR(nzcv,AND(v,CONST32(0x0fffffff)));
		LET(VFP_FPSCR,tmp);
	}else {
		v1 = CONST(0);
		v1 = FPBITCAST32(v1);
		z = FPCMP_OEQ(FR32(d),v1);
		n = FPCMP_OLT(FR32(d),v1);
		c = FPCMP_OGE(FR32(d),v1); 
		tmp = FPCMP_UNO(FR32(d),v1);
		c = OR(c,tmp);
		v1 = tmp;
		n = SHL(ZEXT32(n),CONST32(31));
		z = SHL(ZEXT32(z),CONST32(30));
		c = SHL(ZEXT32(c),CONST32(29));
		v1 = SHL(ZEXT32(v1),CONST(28));
		nzcv = OR(OR(OR(n,z),c),v1);	
		v = R(VFP_FPSCR);
		tmp = OR(nzcv,AND(v,CONST32(0x0fffffff)));
		LET(VFP_FPSCR,tmp);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBDS between double and single */
/* cond 1110 1D11 0111 Vd-- 101X 11M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vcvtbds_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vcvtbds_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbds)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbds_inst));
	vcvtbds_inst *inst_cream = (vcvtbds_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBDS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VCVT(BDS) :\n");

		vcvtbds_inst *inst_cream = (vcvtbds_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vcvtbds_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vcvtbds),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vcvtbds)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vcvtbds)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is executed out.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int dp_op = (BIT(8) == 1);
	int d = dp_op ? BITS(12,15) << 1 | BIT(22) : BIT(22) << 4 | BITS(12,15);
	int m = dp_op ? BITS(0,3) | BIT(5) << 4 : BIT(5) | BITS(0,3) << 1;
	int d2s = dp_op;
	Value* v;
	Value* tmp;
	Value* v1;
	if(d2s){
		v = SHL(ZEXT64(IBITCAST32(FR32(2 * m + 1))),CONST64(32));
		tmp = ZEXT64(IBITCAST32(FR32(2 * m)));
		v1 = OR(v,tmp);
		tmp = FPTRUNC(32,FPBITCAST64(v1));
		LETFPS(d,tmp);	
	}else {
		v = FR32(m);
		tmp = FPEXT(64,v);
		v = IBITCAST64(tmp);
		tmp = TRUNC32(AND(v,CONST64(0xffffffff)));
		v1 = TRUNC32(LSHR(v,CONST64(32)));
		LETFPS(2 * d, FPBITCAST32(tmp) );
		LETFPS(2 * d + 1, FPBITCAST32(v1));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBFF between floating point and fixed point */
/* cond 1110 1D11 1op2 Vd-- 101X X1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vcvtbff_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vcvtbff_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbff)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;VFP_DEBUG_UNTESTED(VCVTBFF);
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbff_inst));
	vcvtbff_inst *inst_cream = (vcvtbff_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBFF_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VCVT(BFF) :\n");

		vcvtbff_inst *inst_cream = (vcvtbff_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vcvtbff_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vcvtbff),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vcvtbff)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vcvtbff)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arch_arm_undef(cpu, bb, instr);
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VCVTBFI between floating point and integer */
/* cond 1110 1D11 1op2 Vd-- 101X X1M0 Vm-- */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vcvtbfi_inst {
	unsigned int instr;
	unsigned int dp_operation;
} vcvtbfi_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vcvtbfi)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vcvtbfi_inst));
	vcvtbfi_inst *inst_cream = (vcvtbfi_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->dp_operation = BIT(inst, 8);
	inst_cream->instr = inst;

	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VCVTBFI_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		DBG("VCVT(BFI) :\n");
		
		vcvtbfi_inst *inst_cream = (vcvtbfi_inst *)inst_base->component;

		int ret;
		
		if (inst_cream->dp_operation)
			ret = vfp_double_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
		else
			ret = vfp_single_cpdo(cpu, inst_cream->instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

		CHECK_VFP_CDP_RET;
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vcvtbfi_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vcvtbfi),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vcvtbfi)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	DBG("\t\tin %s, instruction will be executed out of JIT.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vcvtbfi)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s, instruction will be executed out of JIT.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	unsigned int opc2 = BITS(16,18);
	int to_integer = ((opc2 >> 2) == 1);	
	int dp_op =  (BIT(8) == 1);
	unsigned int op = BIT(7);
	int m,d;
	Value* v;
	Value* hi;
	Value* lo;
	Value* v64; 
	if(to_integer){
		d = BIT(22) | (BITS(12,15) << 1);
		if(dp_op)
			m = BITS(0,3) | BIT(5) << 4;
		else
			m = BIT(5) | BITS(0,3) << 1;
	}else {
		m = BIT(5) | BITS(0,3) << 1;
		if(dp_op)
			d = BITS(12,15) | BIT(22) << 4;
 		else
			d  = BIT(22) | BITS(12,15) << 1;		
	}
	if(to_integer){
		if(dp_op){
			lo = FR32(m * 2);
		        hi = FR32(m * 2 + 1);	
			hi = ZEXT64(IBITCAST32(hi));
			lo = ZEXT64(IBITCAST32(lo));
			v64 = OR(SHL(hi,CONST64(32)),lo);	
			if(BIT(16)){
				v = FPTOSI(32,FPBITCAST64(v64));
			}
			else
				v = FPTOUI(32,FPBITCAST64(v64));
				
				v = FPBITCAST32(v);
				LETFPS(d,v);
		}else {
			v = FR32(m);
			if(BIT(16)){
				
				v = FPTOSI(32,v);
			}
			else
				v = FPTOUI(32,v);
				LETFPS(d,FPBITCAST32(v));
		}
	}else {
		if(dp_op){	
			v = IBITCAST32(FR32(m));
			if(BIT(7))
				v64 = SITOFP(64,v); 
			else
				v64 = UITOFP(64,v);
			v = IBITCAST64(v64);
			hi = FPBITCAST32(TRUNC32(LSHR(v,CONST64(32))));
			lo = FPBITCAST32(TRUNC32(AND(v,CONST64(0xffffffff))));
			LETFPS(2 * d , lo);
			LETFPS(2 * d + 1, hi);
		}else {
			v = IBITCAST32(FR32(m));
			if(BIT(7))
				v = SITOFP(32,v);
			else
				v = UITOFP(32,v);
				LETFPS(d,v);
		}
	}
	return No_exp;
}

/**
* @brief The implementation of c language for vcvtbfi instruction of dyncom
*
* @param cpu
* @param instr
*
* @return 
*/
int vcvtbfi_instr_impl(arm_core_t* cpu, uint32 instr){
	int dp_operation = BIT(8);
	int ret;
	if (dp_operation)
		ret = vfp_double_cpdo(cpu, instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
	else
		ret = vfp_single_cpdo(cpu, instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);

	vfp_raise_exceptions(cpu, ret, instr, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
	return 0;
}
#endif

/* ----------------------------------------------------------------------- */
/* MRC / MCR instructions */
/* cond 1110 AAAL XXXX XXXX 101C XBB1 XXXX */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- */

/* ----------------------------------------------------------------------- */
/* VMOVBRS between register and single precision */
/* cond 1110 000o Vn-- Rt-- 1010 N001 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MRC */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovbrs_inst {
	unsigned int to_arm;
	unsigned int t;
	unsigned int n;
} vmovbrs_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrs)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrs_inst));
	vmovbrs_inst *inst_cream = (vmovbrs_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->to_arm   = BIT(inst, 20) == 1;
	inst_cream->t        = BITS(inst, 12, 15);
	inst_cream->n        = BIT(inst, 7) | BITS(inst, 16, 19)<<1;

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;

		vmovbrs_inst *inst_cream = (vmovbrs_inst *)inst_base->component;

		VMOVBRS(cpu, inst_cream->to_arm, inst_cream->t, inst_cream->n, &(cpu->Reg[inst_cream->t]));
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovbrs_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovbrs),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovbrs)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovbrs)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("VMOV(BRS) :\n");
	int to_arm   = BIT(20) == 1;
	int t        = BITS(12, 15);
	int n        = BIT(7) | BITS(16, 19)<<1;

	if (to_arm)
	{
		DBG("\tr%d <= s%d\n", t, n);
		LET(t, IBITCAST32(FR32(n)));
	}
	else
	{
		DBG("\ts%d <= r%d\n", n, t);
		LETFPS(n, FPBITCAST32(R(t)));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMSR */
/* cond 1110 1110 reg- Rt-- 1010 0001 0000 */
/* cond 1110 op10 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmsr_inst {
	unsigned int reg;
	unsigned int Rd;
} vmsr_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmsr)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmsr_inst));
	vmsr_inst *inst_cream = (vmsr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->reg  = BITS(inst, 16, 19);
	inst_cream->Rd   = BITS(inst, 12, 15);
   
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMSR_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		/* FIXME: special case for access to FPSID and FPEXC, VFP must be disabled ,
		   and in privilegied mode */
		/* Exceptions must be checked, according to v7 ref manual */
		CHECK_VFP_ENABLED;
           
		vmsr_inst *inst_cream = (vmsr_inst *)inst_base->component;

		VMSR(cpu, inst_cream->reg, inst_cream->Rd);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmsr_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmsr),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmsr)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmsr)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	DBG("VMSR :");
	if(RD == 15) {
		printf("in %s is not implementation.\n", __FUNCTION__);
		exit(-1);
	}
	
	Value *data = NULL;
	int reg = RN;
	int Rt   = RD;
	if (reg == 1)
	{
		LET(VFP_FPSCR, R(Rt));
		DBG("\tflags <= fpscr\n");
	}
	else
	{
		switch (reg)
		{
		case 8:
			LET(VFP_FPEXC, R(Rt));
			DBG("\tfpexc <= r%d \n", Rt);
			break;
		default:
			DBG("\tSUBARCHITECTURE DEFINED\n");
			break;
		}
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBRC register to scalar */
/* cond 1110 0XX0 Vd-- Rt-- 1011 DXX1 0000 */
/* cond 1110 op10 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovbrc_inst {
	unsigned int esize;
	unsigned int index;
	unsigned int d;
	unsigned int t;
} vmovbrc_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrc)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrc_inst));
	vmovbrc_inst *inst_cream = (vmovbrc_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->d     = BITS(inst, 16, 19)|BIT(inst, 7)<<4;
	inst_cream->t     = BITS(inst, 12, 15);
	/* VFP variant of instruction */
	inst_cream->esize = 32;
	inst_cream->index = BIT(inst, 21);
   
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRC_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovbrc_inst *inst_cream = (vmovbrc_inst *)inst_base->component;
		
		VFP_DEBUG_UNIMPLEMENTED(VMOVBRC);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovbrc_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovbrc),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovbrc)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovbrc)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arch_arm_undef(cpu, bb, instr);
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMRS */
/* cond 1110 1111 CRn- Rt-- 1010 0001 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MRC */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmrs_inst {
	unsigned int reg;
	unsigned int Rt;
} vmrs_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmrs)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmrs_inst));
	vmrs_inst *inst_cream = (vmrs_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->reg  = BITS(inst, 16, 19);
	inst_cream->Rt	 = BITS(inst, 12, 15);

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMRS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		/* FIXME: special case for access to FPSID and FPEXC, VFP must be disabled,
		   and in privilegied mode */
		/* Exceptions must be checked, according to v7 ref manual */
		CHECK_VFP_ENABLED;
		
		vmrs_inst *inst_cream = (vmrs_inst *)inst_base->component;
		
		DBG("VMRS :");
	
		if (inst_cream->reg == 1) /* FPSCR */
		{
			if (inst_cream->Rt != 15)
			{	
				cpu->Reg[inst_cream->Rt] = cpu->VFP[VFP_OFFSET(VFP_FPSCR)];
				DBG("\tr%d <= fpscr[%08x]\n", inst_cream->Rt, cpu->VFP[VFP_OFFSET(VFP_FPSCR)]);
			}
			else
			{	
				cpu->NFlag = (cpu->VFP[VFP_OFFSET(VFP_FPSCR)] >> 31) & 1;
				cpu->ZFlag = (cpu->VFP[VFP_OFFSET(VFP_FPSCR)] >> 30) & 1;
				cpu->CFlag = (cpu->VFP[VFP_OFFSET(VFP_FPSCR)] >> 29) & 1;
				cpu->VFlag = (cpu->VFP[VFP_OFFSET(VFP_FPSCR)] >> 28) & 1;
				DBG("\tflags <= fpscr[%1xxxxxxxx]\n", cpu->VFP[VFP_OFFSET(VFP_FPSCR)]>>28);
			}
		} 
		else
		{
			switch (inst_cream->reg)
			{
			case 0:
				cpu->Reg[inst_cream->Rt] = cpu->VFP[VFP_OFFSET(VFP_FPSID)];
				DBG("\tr%d <= fpsid[%08x]\n", inst_cream->Rt, cpu->VFP[VFP_OFFSET(VFP_FPSID)]);
				break;
			case 6:
				/* MVFR1, VFPv3 only ? */
				DBG("\tr%d <= MVFR1 unimplemented\n", inst_cream->Rt);
				break;
			case 7:
				/* MVFR0, VFPv3 only? */
				DBG("\tr%d <= MVFR0 unimplemented\n", inst_cream->Rt);
				break;
			case 8:
				cpu->Reg[inst_cream->Rt] = cpu->VFP[VFP_OFFSET(VFP_FPEXC)];
				DBG("\tr%d <= fpexc[%08x]\n", inst_cream->Rt, cpu->VFP[VFP_OFFSET(VFP_FPEXC)]);
				break;
			default:
				DBG("\tSUBARCHITECTURE DEFINED\n");
				break;
			}
		}
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmrs_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmrs),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmrs)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	DBG("\t\tin %s .\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmrs)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	
	Value *data = NULL;
	int reg = BITS(16, 19);;
	int Rt   = BITS(12, 15);
	DBG("VMRS : reg=%d, Rt=%d\n", reg, Rt);
	if (reg == 1)
	{
		if (Rt != 15)
		{
			LET(Rt, R(VFP_FPSCR));
			DBG("\tr%d <= fpscr\n", Rt);
		}
		else
		{
			//LET(Rt, R(VFP_FPSCR));
			update_cond_from_fpscr(cpu, instr, bb, pc);
			DBG("In %s, \tflags <= fpscr\n", __FUNCTION__);
		}
	}
	else
	{
		switch (reg)
		{
		case 0:
			LET(Rt, R(VFP_FPSID));
			DBG("\tr%d <= fpsid\n", Rt);
			break;
		case 6:
			/* MVFR1, VFPv3 only ? */
			DBG("\tr%d <= MVFR1 unimplemented\n", Rt);
			break;
		case 7:
			/* MVFR0, VFPv3 only? */
			DBG("\tr%d <= MVFR0 unimplemented\n", Rt);
			break;
		case 8:
			LET(Rt, R(VFP_FPEXC));
			DBG("\tr%d <= fpexc\n", Rt);
			break;
		default:
			DBG("\tSUBARCHITECTURE DEFINED\n");
			break;
		}
	}

	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBCR scalar to register */
/* cond 1110 XXX1 Vd-- Rt-- 1011 NXX1 0000 */
/* cond 1110 op11 CRn- Rt-- copr op21 CRm- MCR */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovbcr_inst {
	unsigned int esize;
	unsigned int index;
	unsigned int d;
	unsigned int t;
} vmovbcr_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbcr)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbcr_inst));
	vmovbcr_inst *inst_cream = (vmovbcr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->d     = BITS(inst, 16, 19)|BIT(inst, 7)<<4;
	inst_cream->t     = BITS(inst, 12, 15);
	/* VFP variant of instruction */
	inst_cream->esize = 32;
	inst_cream->index = BIT(inst, 21);
   
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBCR_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovbcr_inst *inst_cream = (vmovbcr_inst *)inst_base->component;
		
		VFP_DEBUG_UNIMPLEMENTED(VMOVBCR);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovbcr_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovbcr),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovbcr)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovbcr)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arch_arm_undef(cpu, bb, instr);
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* MRRC / MCRR instructions */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
/* cond 1100 0100 Rt2- Rt-- copr opc1 CRm- MCRR */

/* ----------------------------------------------------------------------- */
/* VMOVBRRSS between 2 registers to 2 singles */
/* cond 1100 010X Rt2- Rt-- 1010 00X1 Vm-- */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovbrrss_inst {
	unsigned int to_arm;
	unsigned int t;
	unsigned int t2;
	unsigned int m;
} vmovbrrss_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrrss)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrrss_inst));
	vmovbrrss_inst *inst_cream = (vmovbrrss_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->to_arm     = BIT(inst, 20) == 1;
	inst_cream->t          = BITS(inst, 12, 15);
	inst_cream->t2         = BITS(inst, 16, 19);
	inst_cream->m          = BITS(inst, 0, 3)<<1|BIT(inst, 5);
   
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRRSS_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovbrrss_inst *inst_cream = (vmovbrrss_inst *)inst_base->component;
		
		VFP_DEBUG_UNIMPLEMENTED(VMOVBRRSS);
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovbrrss_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif
#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovbrrss),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovbrrss)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovbrrss)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arch_arm_undef(cpu, bb, instr);
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VMOVBRRD between 2 registers and 1 double */
/* cond 1100 010X Rt2- Rt-- 1011 00X1 Vm-- */
/* cond 1100 0101 Rt2- Rt-- copr opc1 CRm- MRRC */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vmovbrrd_inst {
	unsigned int to_arm;
	unsigned int t;
	unsigned int t2;
	unsigned int m;
} vmovbrrd_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vmovbrrd)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vmovbrrd_inst));
	vmovbrrd_inst *inst_cream = (vmovbrrd_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->to_arm   = BIT(inst, 20) == 1;
	inst_cream->t        = BITS(inst, 12, 15);
	inst_cream->t2       = BITS(inst, 16, 19);
	inst_cream->m        = BIT(inst, 5)<<4 | BITS(inst, 0, 3);

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VMOVBRRD_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vmovbrrd_inst *inst_cream = (vmovbrrd_inst *)inst_base->component;
		
		VMOVBRRD(cpu, inst_cream->to_arm, inst_cream->t, inst_cream->t2, inst_cream->m, 
				&(cpu->Reg[inst_cream->t]), &(cpu->Reg[inst_cream->t2]));
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vmovbrrd_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vmovbrrd),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vmovbrrd)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;
	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vmovbrrd)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int to_arm   = BIT(20) == 1;
	int t        = BITS(12, 15);
	int t2       = BITS(16, 19);
	int n        = BIT(5)<<4 | BITS(0, 3);
	if(to_arm){
		LET(t, IBITCAST32(FR32(n * 2)));
		LET(t2, IBITCAST32(FR32(n * 2 + 1)));
	}
	else{
		LETFPS(n * 2, FPBITCAST32(R(t)));
		LETFPS(n * 2 + 1, FPBITCAST32(R(t2)));
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* LDC/STC between 2 registers and 1 double */
/* cond 110X XXX1 Rn-- CRd- copr imm- imm- LDC */
/* cond 110X XXX0 Rn-- CRd- copr imm8 imm8 STC */

/* ----------------------------------------------------------------------- */
/* VSTR */
/* cond 1101 UD00 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vstr_inst {
	unsigned int single;
	unsigned int n;
	unsigned int d;
	unsigned int imm32;
	unsigned int add;
} vstr_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vstr)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vstr_inst));
	vstr_inst *inst_cream = (vstr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;
	
	inst_cream->single = BIT(inst, 8) == 0;
	inst_cream->add	   = BIT(inst, 23);
	inst_cream->imm32  = BITS(inst, 0,7) << 2;
	inst_cream->d      = (inst_cream->single ? BITS(inst, 12, 15)<<1|BIT(inst, 22) : BITS(inst, 12, 15)|BIT(inst, 22)<<4);
	inst_cream->n	   = BITS(inst, 16, 19);

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSTR_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vstr_inst *inst_cream = (vstr_inst *)inst_base->component;
		
		unsigned int base = (inst_cream->n == 15 ? (cpu->Reg[inst_cream->n] & 0xFFFFFFFC) + 8 : cpu->Reg[inst_cream->n]);
		addr = (inst_cream->add ? base + inst_cream->imm32 : base - inst_cream->imm32);
		DBG("VSTR :\n");
		
		
		if (inst_cream->single)
		{
			fault = check_address_validity(cpu, addr, &phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d], 32);
			if (fault) goto MMU_EXCEPTION;
			DBG("\taddr[%x] <= s%d=[%x]\n", addr, inst_cream->d, cpu->ExtReg[inst_cream->d]);
		}
		else
		{
			fault = check_address_validity(cpu, addr, &phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;

			/* Check endianness */
			fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d*2], 32);
			if (fault) goto MMU_EXCEPTION;

			fault = check_address_validity(cpu, addr + 4, &phys_addr, 0);
			if (fault) goto MMU_EXCEPTION;

			fault = interpreter_write_memory(addr + 4, phys_addr, cpu->ExtReg[inst_cream->d*2+1], 32);
			if (fault) goto MMU_EXCEPTION;
			DBG("\taddr[%x-%x] <= s[%d-%d]=[%x-%x]\n", addr+4, addr, inst_cream->d*2+1, inst_cream->d*2, cpu->ExtReg[inst_cream->d*2+1], cpu->ExtReg[inst_cream->d*2]);
		}
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vstr_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vstr),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vstr)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vstr)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	int single = BIT(8) == 0;
	int add	   = BIT(23);
	int imm32  = BITS(0,7) << 2;
	int d      = (single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|(BIT(22)<<4));
	int n	   = BITS(16, 19);

	Value* base = (n == 15) ? ADD(AND(R(n), CONST(0xFFFFFFFC)), CONST(8)): R(n);
	Value* Addr = add ? ADD(base, CONST(imm32)) : SUB(base, CONST(imm32));
	DBG("VSTR :\n");
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, 4, 0, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, 8, 0, cpu->dyncom_engine->bb_trap);
	//Value* phys_addr;
	if(single){
		#if 0
		phys_addr = get_phys_addr(cpu, bb, Addr, 0);
		bb = cpu->dyncom_engine->bb;
		arch_write_memory(cpu, bb, phys_addr, RSPR(d), 32);
		#endif
		//memory_write(cpu, bb, Addr, RSPR(d), 32);
		memory_write(cpu, bb, Addr, IBITCAST32(FR32(d)), 32);
		bb = cpu->dyncom_engine->bb;
	}
	else{
		#if 0
		phys_addr = get_phys_addr(cpu, bb, Addr, 0);
		bb = cpu->dyncom_engine->bb;
		arch_write_memory(cpu, bb, phys_addr, RSPR(d * 2), 32);
		#endif
		//memory_write(cpu, bb, Addr, RSPR(d * 2), 32);
		memory_write(cpu, bb, Addr, IBITCAST32(FR32(d * 2)), 32);
		bb = cpu->dyncom_engine->bb;
		#if 0
		phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 0);
		bb = cpu->dyncom_engine->bb;
		arch_write_memory(cpu, bb, phys_addr, RSPR(d * 2 + 1), 32);
		#endif
		//memory_write(cpu, bb, ADD(Addr, CONST(4)), RSPR(d * 2 + 1), 32);
		memory_write(cpu, bb, ADD(Addr, CONST(4)), IBITCAST32(FR32(d * 2 + 1)), 32);
		bb = cpu->dyncom_engine->bb;
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VPUSH */
/* cond 1101 0D10 1101 Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vpush_inst {
	unsigned int single;
	unsigned int d;
	unsigned int imm32;
	unsigned int regs;
} vpush_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vpush)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vpush_inst));
	vpush_inst *inst_cream = (vpush_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->single  = BIT(inst, 8) == 0;
	inst_cream->d       = (inst_cream->single ? BITS(inst, 12, 15)<<1|BIT(inst, 22) : BITS(inst, 12, 15)|BIT(inst, 22)<<4);
	inst_cream->imm32   = BITS(inst, 0, 7)<<2;
	inst_cream->regs    = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VPUSH_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
				
		int i;

		vpush_inst *inst_cream = (vpush_inst *)inst_base->component;

		DBG("VPUSH :\n");
			
		addr = cpu->Reg[R13] - inst_cream->imm32;


		for (i = 0; i < inst_cream->regs; i++)
		{
			if (inst_cream->single)
			{
				fault = check_address_validity(cpu, addr, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\taddr[%x] <= s%d=[%x]\n", addr, inst_cream->d+i, cpu->ExtReg[inst_cream->d+i]);
				addr += 4;
			}
			else
			{
				/* Careful of endianness, little by default */
				fault = check_address_validity(cpu, addr, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2], 32);
				if (fault) goto MMU_EXCEPTION;

				fault = check_address_validity(cpu, addr + 4, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_write_memory(addr + 4, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2 + 1], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\taddr[%x-%x] <= s[%d-%d]=[%x-%x]\n", addr+4, addr, (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, cpu->ExtReg[(inst_cream->d+i)*2+1], cpu->ExtReg[(inst_cream->d+i)*2]);
				addr += 8;
			}
		}
		DBG("\tsp[%x]", cpu->Reg[R13]);
		cpu->Reg[R13] = cpu->Reg[R13] - inst_cream->imm32;
		DBG("=>[%x]\n", cpu->Reg[R13]);
	
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vpush_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif
#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vpush),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vpush)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vpush)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	int single  = BIT(8) == 0;
	int d       = (single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|(BIT(22)<<4));
	int imm32   = BITS(0, 7)<<2;
	int regs    = (single ? BITS(0, 7) : BITS(1, 7));

	DBG("\t\tin %s \n", __FUNCTION__);
	Value* Addr = SUB(R(13), CONST(imm32));
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 0, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 8, 0, cpu->dyncom_engine->bb_trap);
	//Value* phys_addr;
	int i;
	for (i = 0; i < regs; i++)
	{
		if (single)
		{
			//fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR(d + i), 32);
			#endif
			//memory_write(cpu, bb, Addr, RSPR(d + i), 32);
			memory_write(cpu, bb, Addr, IBITCAST32(FR32(d + i)), 32);
			bb = cpu->dyncom_engine->bb;
			Addr = ADD(Addr, CONST(4));
		}
		else
		{
			/* Careful of endianness, little by default */
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR((d + i) * 2), 32);
			#endif
			//memory_write(cpu, bb, Addr, RSPR((d + i) * 2), 32);
			memory_write(cpu, bb, Addr, IBITCAST32(FR32((d + i) * 2)), 32);
			bb = cpu->dyncom_engine->bb;
			#if 0
			phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR((d + i) * 2 + 1), 32);
			#endif
			//memory_write(cpu, bb, ADD(Addr, CONST(4)), RSPR((d + i) * 2 + 1), 32);
			memory_write(cpu, bb, ADD(Addr, CONST(4)), IBITCAST32(FR32((d + i) * 2 + 1)), 32);
			bb = cpu->dyncom_engine->bb;

			Addr = ADD(Addr, CONST(8));
		}
	}
	LET(13, SUB(R(13), CONST(imm32)));

	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VSTM */
/* cond 110P UDW0 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vstm_inst {
	unsigned int single;
	unsigned int add;
	unsigned int wback;
	unsigned int d;
	unsigned int n;
	unsigned int imm32;
	unsigned int regs;
} vstm_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vstm)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vstm_inst));
	vstm_inst *inst_cream = (vstm_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->single = BIT(inst, 8) == 0;
	inst_cream->add    = BIT(inst, 23);
	inst_cream->wback  = BIT(inst, 21);
	inst_cream->d      = (inst_cream->single ? BITS(inst, 12, 15)<<1|BIT(inst, 22) : BITS(inst, 12, 15)|BIT(inst, 22)<<4);
	inst_cream->n      = BITS(inst, 16, 19);
	inst_cream->imm32  = BITS(inst, 0, 7)<<2;
	inst_cream->regs   = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VSTM_INST: /* encoding 1 */
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		int i;
		
		vstm_inst *inst_cream = (vstm_inst *)inst_base->component;
		
		addr = (inst_cream->add ? cpu->Reg[inst_cream->n] : cpu->Reg[inst_cream->n] - inst_cream->imm32);
		DBG("VSTM : addr[%x]\n", addr);
		
		
		for (i = 0; i < inst_cream->regs; i++)
		{
			if (inst_cream->single)
			{
				fault = check_address_validity(cpu, addr, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\taddr[%x] <= s%d=[%x]\n", addr, inst_cream->d+i, cpu->ExtReg[inst_cream->d+i]);
				addr += 4;
			}
			else
			{
				/* Careful of endianness, little by default */
				fault = check_address_validity(cpu, addr, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2], 32);
				if (fault) goto MMU_EXCEPTION;

				fault = check_address_validity(cpu, addr + 4, &phys_addr, 0);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_write_memory(addr + 4, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2 + 1], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\taddr[%x-%x] <= s[%d-%d]=[%x-%x]\n", addr+4, addr, (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, cpu->ExtReg[(inst_cream->d+i)*2+1], cpu->ExtReg[(inst_cream->d+i)*2]);
				addr += 8;
			}
		}
		if (inst_cream->wback){
			cpu->Reg[inst_cream->n] = (inst_cream->add ? cpu->Reg[inst_cream->n] + inst_cream->imm32 : 
						   cpu->Reg[inst_cream->n] - inst_cream->imm32);
			DBG("\twback r%d[%x]\n", inst_cream->n, cpu->Reg[inst_cream->n]);
		}

	}
	cpu->Reg[15] += 4;
	INC_PC(sizeof(vstm_inst));

	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vstm),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vstm)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vstm)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	//arch_arm_undef(cpu, bb, instr);
	int single = BIT(8) == 0;
	int add    = BIT(23);
	int wback  = BIT(21);
	int d      = single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|(BIT(22)<<4);
	int n      = BITS(16, 19);
	int imm32  = BITS(0, 7)<<2;
	int regs   = single ? BITS(0, 7) : BITS(1, 7);

	Value* Addr = SELECT(CONST1(add), R(n), SUB(R(n), CONST(imm32)));
	DBG("VSTM \n");
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 0, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 8, 0, cpu->dyncom_engine->bb_trap);

	int i;	
	Value* phys_addr;
	for (i = 0; i < regs; i++)
	{
		if (single)
		{
			
			//fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
			/* if R(i) is R15? */
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR(d + i), 32);
			#endif
			//memory_write(cpu, bb, Addr, RSPR(d + i), 32);
			memory_write(cpu, bb, Addr, IBITCAST32(FR32(d + i)),32);
			bb = cpu->dyncom_engine->bb;
			//if (fault) goto MMU_EXCEPTION;
			//DBG("\taddr[%x] <= s%d=[%x]\n", addr, inst_cream->d+i, cpu->ExtReg[inst_cream->d+i]);
			Addr = ADD(Addr, CONST(4));
		}
		else
		{
		
			//fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2], 32);
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR((d + i) * 2), 32);
			#endif
			//memory_write(cpu, bb, Addr, RSPR((d + i) * 2), 32);
			memory_write(cpu, bb, Addr, IBITCAST32(FR32((d + i) * 2)),32);
			bb = cpu->dyncom_engine->bb;
			//if (fault) goto MMU_EXCEPTION;

			//fault = interpreter_write_memory(addr + 4, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2 + 1], 32);
			#if 0
			phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 0);
			bb = cpu->dyncom_engine->bb;
			arch_write_memory(cpu, bb, phys_addr, RSPR((d + i) * 2 + 1), 32);
			#endif
			//memory_write(cpu, bb, ADD(Addr, CONST(4)), RSPR((d + i) * 2 + 1), 32);
			memory_write(cpu, bb, ADD(Addr, CONST(4)), IBITCAST32(FR32((d + i) * 2 + 1)), 32);
			bb = cpu->dyncom_engine->bb;
			//if (fault) goto MMU_EXCEPTION;
			//DBG("\taddr[%x-%x] <= s[%d-%d]=[%x-%x]\n", addr+4, addr, (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, cpu->ExtReg[(inst_cream->d+i)*2+1], cpu->ExtReg[(inst_cream->d+i)*2]);
			//addr += 8;
			Addr = ADD(Addr, CONST(8));
		}
	}
	if (wback){
		//cpu->Reg[n] = (add ? cpu->Reg[n] + imm32 : 
		//			   cpu->Reg[n] - imm32);
		LET(n, SELECT(CONST1(add), ADD(R(n), CONST(imm32)), SUB(R(n), CONST(imm32))));
		DBG("\twback r%d, add=%d, imm32=%d\n", n, add, imm32);
	}
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VPOP */
/* cond 1100 1D11 1101 Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vpop_inst {
	unsigned int single;
	unsigned int d;
	unsigned int imm32;
	unsigned int regs;
} vpop_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vpop)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vpop_inst));
	vpop_inst *inst_cream = (vpop_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->single  = BIT(inst, 8) == 0;
	inst_cream->d       = (inst_cream->single ? (BITS(inst, 12, 15)<<1)|BIT(inst, 22) : BITS(inst, 12, 15)|(BIT(inst, 22)<<4));
	inst_cream->imm32   = BITS(inst, 0, 7)<<2;
	inst_cream->regs    = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));
	
	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VPOP_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		int i;
		unsigned int value1, value2;

		vpop_inst *inst_cream = (vpop_inst *)inst_base->component;
		
		DBG("VPOP :\n");
		
		addr = cpu->Reg[R13];
		

		for (i = 0; i < inst_cream->regs; i++)
		{
			if (inst_cream->single)
			{
				fault = check_address_validity(cpu, addr, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_read_memory(addr, phys_addr, value1, 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\ts%d <= [%x] addr[%x]\n", inst_cream->d+i, value1, addr);
				cpu->ExtReg[inst_cream->d+i] = value1;
				addr += 4;
			}
			else
			{
				/* Careful of endianness, little by default */
				fault = check_address_validity(cpu, addr, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_read_memory(addr, phys_addr, value1, 32);
				if (fault) goto MMU_EXCEPTION;

				fault = check_address_validity(cpu, addr + 4, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;

				fault = interpreter_read_memory(addr + 4, phys_addr, value2, 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\ts[%d-%d] <= [%x-%x] addr[%x-%x]\n", (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, value2, value1, addr+4, addr);
				cpu->ExtReg[(inst_cream->d+i)*2] = value1;
				cpu->ExtReg[(inst_cream->d+i)*2 + 1] = value2;
				addr += 8;
			}
		}
		DBG("\tsp[%x]", cpu->Reg[R13]);
		cpu->Reg[R13] = cpu->Reg[R13] + inst_cream->imm32;
		DBG("=>[%x]\n", cpu->Reg[R13]);
		
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vpop_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vpop),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vpop)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	/* Should check if PC is destination register */
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vpop)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	DBG("\t\tin %s instruction .\n", __FUNCTION__);
	//arch_arm_undef(cpu, bb, instr);
	int single  = BIT(8) == 0;
	int d       = (single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|(BIT(22)<<4));
	int imm32   = BITS(0, 7)<<2;
	int regs    = (single ? BITS(0, 7) : BITS(1, 7));

	int i;
	unsigned int value1, value2;

	DBG("VPOP :\n");
		
	Value* Addr = R(13);
	Value* val;
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 1, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 1, cpu->dyncom_engine->bb_trap);
	//Value* phys_addr;	
	for (i = 0; i < regs; i++)
	{
		if (single)
		{
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, Addr, 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			LETFPS(d + i, FPBITCAST32(val));
			Addr = ADD(Addr, CONST(4));
		}
		else
		{
			/* Careful of endianness, little by default */
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, Addr, 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			LETFPS((d + i) * 2, FPBITCAST32(val));
			#if 0
			phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, ADD(Addr, CONST(4)), 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			LETFPS((d + i) * 2 + 1, FPBITCAST32(val));

			Addr = ADD(Addr, CONST(8));
		}
	}
	LET(13, ADD(R(13), CONST(imm32)));
	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VLDR */
/* cond 1101 UD01 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vldr_inst {
	unsigned int single;
	unsigned int n;
	unsigned int d;
	unsigned int imm32;
	unsigned int add;
} vldr_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vldr)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vldr_inst));
	vldr_inst *inst_cream = (vldr_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx	 = index;
	inst_base->br	 = NON_BRANCH;
	inst_base->load_r15 = 0;
	
	inst_cream->single = BIT(inst, 8) == 0;
	inst_cream->add	   = BIT(inst, 23);
	inst_cream->imm32  = BITS(inst, 0,7) << 2;
	inst_cream->d      = (inst_cream->single ? BITS(inst, 12, 15)<<1|BIT(inst, 22) : BITS(inst, 12, 15)|BIT(inst, 22)<<4);
	inst_cream->n	   = BITS(inst, 16, 19);

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VLDR_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		vldr_inst *inst_cream = (vldr_inst *)inst_base->component;
		
		unsigned int base = (inst_cream->n == 15 ? (cpu->Reg[inst_cream->n] & 0xFFFFFFFC) + 8 : cpu->Reg[inst_cream->n]);
		addr = (inst_cream->add ? base + inst_cream->imm32 : base - inst_cream->imm32);
		DBG("VLDR :\n", addr);
		
		
		if (inst_cream->single)
		{
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_read_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d], 32);
			if (fault) goto MMU_EXCEPTION;
			DBG("\ts%d <= [%x] addr[%x]\n", inst_cream->d, cpu->ExtReg[inst_cream->d], addr);
		}
		else
		{
			unsigned int word1, word2;
			fault = check_address_validity(cpu, addr, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_read_memory(addr, phys_addr, word1, 32);
			if (fault) goto MMU_EXCEPTION;

			fault = check_address_validity(cpu, addr + 4, &phys_addr, 1);
			if (fault) goto MMU_EXCEPTION;
			fault = interpreter_read_memory(addr + 4, phys_addr, word2, 32);
			if (fault) goto MMU_EXCEPTION;
			/* Check endianness */
			cpu->ExtReg[inst_cream->d*2] = word1;
			cpu->ExtReg[inst_cream->d*2+1] = word2;
			DBG("\ts[%d-%d] <= [%x-%x] addr[%x-%x]\n", inst_cream->d*2+1, inst_cream->d*2, word2, word1, addr+4, addr);
		}
	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vldr_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vldr),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vldr)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	/* Should check if PC is destination register */
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vldr)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	int single = BIT(8) == 0;
	int add    = BIT(23);
	int wback  = BIT(21);
	int d      = (single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|(BIT(22)<<4));
	int n      = BITS(16, 19);
	int imm32  = BITS(0, 7)<<2;
	int regs   = (single ? BITS(0, 7) : BITS(1, 7));
	Value* base = R(n);
	DBG("\t\tin %s .\n", __FUNCTION__);
	if(n == 15){
		base = ADD(AND(base, CONST(0xFFFFFFFC)), CONST(8));
	}
	Value* Addr = add ? (ADD(base, CONST(imm32))) : (SUB(base, CONST(imm32)));
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, 4, 1, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, 8, 1, cpu->dyncom_engine->bb_trap);
	//Value* phys_addr;
	Value* val;
	if(single){
		#if 0
		phys_addr = get_phys_addr(cpu, bb, Addr, 1);
		bb = cpu->dyncom_engine->bb;
		val = arch_read_memory(cpu,bb,phys_addr,0,32);
		#endif
		memory_read(cpu, bb, Addr, 0, 32);
		bb = cpu->dyncom_engine->bb;
		val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
		//LETS(d, val);
		LETFPS(d,FPBITCAST32(val));
	}
	else{
		#if 0
		phys_addr = get_phys_addr(cpu, bb, Addr, 1);
		bb = cpu->dyncom_engine->bb;
		val = arch_read_memory(cpu,bb,phys_addr,0,32);
		#endif
		memory_read(cpu, bb, Addr, 0, 32);
		bb = cpu->dyncom_engine->bb;
		val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
		//LETS(d * 2, val);
		LETFPS(d * 2,FPBITCAST32(val));
		#if 0
		phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 1);
		bb = cpu->dyncom_engine->bb;
		val = arch_read_memory(cpu,bb,phys_addr,0,32);
		#endif
		memory_read(cpu, bb, ADD(Addr, CONST(4)), 0,32);
		bb = cpu->dyncom_engine->bb;
		val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
		//LETS(d * 2 + 1, val);
		LETFPS( d * 2 + 1,FPBITCAST32(val));
	}

	return No_exp;
}
#endif

/* ----------------------------------------------------------------------- */
/* VLDM */
/* cond 110P UDW1 Rn-- Vd-- 101X imm8 imm8 */
#ifdef VFP_INTERPRETER_STRUCT
typedef struct _vldm_inst {
	unsigned int single;
	unsigned int add;
	unsigned int wback;
	unsigned int d;
	unsigned int n;
	unsigned int imm32;
	unsigned int regs;
} vldm_inst;
#endif
#ifdef VFP_INTERPRETER_TRANS
ARM_INST_PTR INTERPRETER_TRANSLATE(vldm)(unsigned int inst, int index)
{
	VFP_DEBUG_TRANSLATE;
	
	arm_inst *inst_base = (arm_inst *)AllocBuffer(sizeof(arm_inst) + sizeof(vldm_inst));
	vldm_inst *inst_cream = (vldm_inst *)inst_base->component;

	inst_base->cond  = BITS(inst, 28, 31);
	inst_base->idx     = index;
	inst_base->br     = NON_BRANCH;
	inst_base->load_r15 = 0;

	inst_cream->single = BIT(inst, 8) == 0;
	inst_cream->add    = BIT(inst, 23);
	inst_cream->wback  = BIT(inst, 21);
	inst_cream->d      = (inst_cream->single ? BITS(inst, 12, 15)<<1|BIT(inst, 22) : BITS(inst, 12, 15)|BIT(inst, 22)<<4);
	inst_cream->n      = BITS(inst, 16, 19);
	inst_cream->imm32  = BITS(inst, 0, 7)<<2;
	inst_cream->regs   = (inst_cream->single ? BITS(inst, 0, 7) : BITS(inst, 1, 7));

	return inst_base;
}
#endif
#ifdef VFP_INTERPRETER_IMPL
VLDM_INST:
{
	INC_ICOUNTER;
	if ((inst_base->cond == 0xe) || CondPassed(cpu, inst_base->cond)) {
		CHECK_VFP_ENABLED;
		
		int i;
		
		vldm_inst *inst_cream = (vldm_inst *)inst_base->component;
		
		addr = (inst_cream->add ? cpu->Reg[inst_cream->n] : cpu->Reg[inst_cream->n] - inst_cream->imm32);
		DBG("VLDM : addr[%x]\n", addr);
		
		for (i = 0; i < inst_cream->regs; i++)
		{
			if (inst_cream->single)
			{
				fault = check_address_validity(cpu, addr, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_read_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\ts%d <= [%x] addr[%x]\n", inst_cream->d+i, cpu->ExtReg[inst_cream->d+i], addr);
				addr += 4;
			}
			else
			{
				/* Careful of endianness, little by default */
				fault = check_address_validity(cpu, addr, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_read_memory(addr, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2], 32);
				if (fault) goto MMU_EXCEPTION;

				fault = check_address_validity(cpu, addr + 4, &phys_addr, 1);
				if (fault) goto MMU_EXCEPTION;
				fault = interpreter_read_memory(addr + 4, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2 + 1], 32);
				if (fault) goto MMU_EXCEPTION;
				DBG("\ts[%d-%d] <= [%x-%x] addr[%x-%x]\n", (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, cpu->ExtReg[(inst_cream->d+i)*2+1], cpu->ExtReg[(inst_cream->d+i)*2], addr+4, addr);
				addr += 8;
			}
		}
		if (inst_cream->wback){
			cpu->Reg[inst_cream->n] = (inst_cream->add ? cpu->Reg[inst_cream->n] + inst_cream->imm32 : 
						   cpu->Reg[inst_cream->n] - inst_cream->imm32);
			DBG("\twback r%d[%x]\n", inst_cream->n, cpu->Reg[inst_cream->n]);
		}

	}
	cpu->Reg[15] += GET_INST_SIZE(cpu);
	INC_PC(sizeof(vldm_inst));
	FETCH_INST;
	GOTO_NEXT_INST;
}
#endif

#ifdef VFP_DYNCOM_TABLE
DYNCOM_FILL_ACTION(vldm),
#endif
#ifdef VFP_DYNCOM_TAG
int DYNCOM_TAG(vldm)(cpu_t *cpu, addr_t pc, uint32_t instr, tag_t *tag, addr_t *new_pc, addr_t *next_pc)
{
	int instr_size = INSTR_SIZE;
	//DBG("\t\tin %s instruction is not implemented.\n", __FUNCTION__);
	//arm_tag_trap(cpu, pc, instr, tag, new_pc, next_pc);
	arm_tag_continue(cpu, pc, instr, tag, new_pc, next_pc);
	DBG("In %s, pc=0x%x, next_pc=0x%x\n", __FUNCTION__, pc, *next_pc);
	*tag |= TAG_NEW_BB;
	if(instr >> 28 != 0xe)
		*tag |= TAG_CONDITIONAL;

	return instr_size;
}
#endif
#ifdef VFP_DYNCOM_TRANS
int DYNCOM_TRANS(vldm)(cpu_t *cpu, uint32_t instr, BasicBlock *bb, addr_t pc){
	int single = BIT(8) == 0;
	int add    = BIT(23);
	int wback  = BIT(21);
	int d      = single ? BITS(12, 15)<<1|BIT(22) : BITS(12, 15)|BIT(22)<<4;
	int n      = BITS(16, 19);
	int imm32  = BITS(0, 7)<<2;
	int regs   = single ? BITS(0, 7) : BITS(1, 7);

	Value* Addr = SELECT(CONST1(add), R(n), SUB(R(n), CONST(imm32)));
	//if(single)
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 1, cpu->dyncom_engine->bb_trap);
	//else
	//	bb = arch_check_mm(cpu, bb, Addr, regs * 4, 1, cpu->dyncom_engine->bb_trap);

	DBG("VLDM \n");
	int i;	
	//Value* phys_addr;
	Value* val;
	for (i = 0; i < regs; i++)
	{
		if (single)
		{
			
			//fault = interpreter_write_memory(addr, phys_addr, cpu->ExtReg[inst_cream->d+i], 32);
			/* if R(i) is R15? */
			#if 0
			phys_addr = get_phys_addr(cpu, bb, Addr, 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, Addr, 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			//LETS(d + i, val);
			LETFPS(d + i, FPBITCAST32(val));
			//if (fault) goto MMU_EXCEPTION;
			//DBG("\taddr[%x] <= s%d=[%x]\n", addr, inst_cream->d+i, cpu->ExtReg[inst_cream->d+i]);
			Addr = ADD(Addr, CONST(4));
		}
		else
		{
			#if 0	
			phys_addr = get_phys_addr(cpu, bb, Addr, 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, Addr, 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			LETFPS((d + i) * 2, FPBITCAST32(val));
			#if 0
			phys_addr = get_phys_addr(cpu, bb, ADD(Addr, CONST(4)), 1);
			bb = cpu->dyncom_engine->bb;
			val = arch_read_memory(cpu,bb,phys_addr,0,32);
			#endif
			memory_read(cpu, bb, Addr, 0, 32);
			bb = cpu->dyncom_engine->bb;
			val = new LoadInst(cpu->dyncom_engine->read_value, "", false, bb);
			LETFPS((d + i) * 2 + 1, FPBITCAST32(val));

			//fault = interpreter_write_memory(addr + 4, phys_addr, cpu->ExtReg[(inst_cream->d+i)*2 + 1], 32);
			//DBG("\taddr[%x-%x] <= s[%d-%d]=[%x-%x]\n", addr+4, addr, (inst_cream->d+i)*2+1, (inst_cream->d+i)*2, cpu->ExtReg[(inst_cream->d+i)*2+1], cpu->ExtReg[(inst_cream->d+i)*2]);
			//addr += 8;
			Addr = ADD(Addr, CONST(8));
		}
	}
	if (wback){
		//cpu->Reg[n] = (add ? cpu->Reg[n] + imm32 : 
		//			   cpu->Reg[n] - imm32);
		LET(n, SELECT(CONST1(add), ADD(R(n), CONST(imm32)), SUB(R(n), CONST(imm32))));
		DBG("\twback r%d, add=%d, imm32=%d\n", n, add, imm32);
	}
	return No_exp;
}
#endif
