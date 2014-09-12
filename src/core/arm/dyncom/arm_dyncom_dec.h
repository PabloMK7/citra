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
* @file arm_dyncom_dec.h
* @brief Some common utility for arm instruction decoder
* @author Michael.Kang blackfin.kang@gmail.com
* @version 7849
* @date 2012-03-15
*/

#ifndef __ARM_DYNCOM_DEC__
#define __ARM_DYNCOM_DEC__

#define BITS(a,b) ((instr >> (a)) & ((1 << (1+(b)-(a)))-1))
#define BIT(n) ((instr >> (n)) & 1)
#define BAD	do{printf("meet BAD at %s, instr is %x\n", __FUNCTION__, instr ); /*exit(0);*/}while(0);
#define ptr_N	cpu->ptr_N
#define ptr_Z	cpu->ptr_Z
#define ptr_C	cpu->ptr_C
#define ptr_V	cpu->ptr_V
#define ptr_I 	cpu->ptr_I
#define ptr_T 	cpu->ptr_T
#define	ptr_CPSR cpu->ptr_gpr[16]

/* for MUL instructions */
/*xxxx xxxx xxxx 1111 xxxx xxxx xxxx xxxx */
#define RDHi ((instr >> 16) & 0xF)
/*xxxx xxxx xxxx xxxx 1111 xxxx xxxx xxxx */
#define RDLo ((instr >> 12) & 0xF)
/*xxxx xxxx xxxx 1111 xxxx xxxx xxxx xxxx */
#define MUL_RD ((instr >> 16) & 0xF)
/*xxxx xxxx xxxx xxxx 1111 xxxx xxxx xxxx */
#define MUL_RN ((instr >> 12) & 0xF)
/*xxxx xxxx xxxx xxxx xxxx 1111 xxxx xxxx */
#define RS ((instr >> 8) & 0xF)

/*xxxx xxxx xxxx xxxx 1111 xxxx xxxx xxxx */
#define RD ((instr >> 12) & 0xF)
/*xxxx xxxx xxxx 1111 xxxx xxxx xxxx xxxx */
#define RN ((instr >> 16) & 0xF)
/*xxxx xxxx xxxx xxxx xxxx xxxx xxxx 1111 */
#define RM (instr & 0xF)
#define BIT(n) ((instr >> (n)) & 1)
#define BITS(a,b) ((instr >> (a)) & ((1 << (1+(b)-(a)))-1))

/* CP15 registers */
#define OPCODE_1        BITS(21, 23)
#define CRn             BITS(16, 19)
#define CRm             BITS(0, 3)
#define OPCODE_2        BITS(5, 7)

/*xxxx xx1x xxxx xxxx xxxx xxxx xxxx xxxx */
#define I BIT(25)
/*xxxx xxxx xxx1 xxxx xxxx xxxx xxxx xxxx */
#define S BIT(20)

#define SHIFT BITS(5,6)
#define SHIFT_IMM BITS(7,11)
#define IMMH BITS(8,11)
#define IMML BITS(0,3)

#define LSPBIT  BIT(24)
#define LSUBIT  BIT(23)
#define LSBBIT  BIT(22)
#define LSWBIT  BIT(21)
#define LSLBIT  BIT(20)
#define LSSHBITS BITS(5,6)
#define OFFSET12 BITS(0,11)
#define SBIT  BIT(20)
#define DESTReg (BITS (12, 15))

/* they are in unused state, give a corrent value when using */
#define IS_V5E 0
#define IS_V5  0
#define IS_V6  0
#define LHSReg 0

/* temp define the using the pc reg need implement a flow */
#define STORE_CHECK_RD_PC	ADD(R(RD), CONST(INSTR_SIZE * 2))

#define OPERAND operand(cpu,instr,bb,NULL)
#define SCO_OPERAND(sco) operand(cpu,instr,bb,sco)
#define BOPERAND boperand(instr)

#define CHECK_RN_PC  (RN==15? ADD(AND(R(RN), CONST(~0x1)), CONST(INSTR_SIZE * 2)):R(RN))
#define CHECK_RN_PC_WA  (RN==15? ADD(AND(R(RN), CONST(~0x3)), CONST(INSTR_SIZE * 2)):R(RN))

#define GET_USER_MODE() (OR(ICMP_EQ(R(MODE_REG), CONST(USER32MODE)), ICMP_EQ(R(MODE_REG), CONST(SYSTEM32MODE))))

int decode_arm_instr(uint32_t instr, int32_t *idx);

enum DECODE_STATUS {
	DECODE_SUCCESS,
	DECODE_FAILURE
};

struct instruction_set_encoding_item {
        const char *name;
        int attribute_value;
        int version;
        u32 content[21];
};

typedef struct instruction_set_encoding_item ISEITEM;

#define RECORD_WB(value, flag) {cpu->dyncom_engine->wb_value = value;cpu->dyncom_engine->wb_flag = flag;}
#define INIT_WB(wb_value, wb_flag) RECORD_WB(wb_value, wb_flag)

#define EXECUTE_WB(base_reg)		{if(cpu->dyncom_engine->wb_flag) \
                                               LET(base_reg, cpu->dyncom_engine->wb_value);}
inline int get_reg_count(uint32_t instr){
	int i =  BITS(0,15);
	int count = 0;
	while(i){
		if(i & 1)
			count ++;
		i = i >> 1;
	}
	return count;
}

enum ARMVER {
	INVALID = 0,
        ARMALL,
        ARMV4,
        ARMV4T,
        ARMV5T,
        ARMV5TE,
        ARMV5TEJ,
        ARMV6,
	ARM1176JZF_S,
        ARMVFP2,
        ARMVFP3
};

//extern const INSTRACT arm_instruction_action[];
extern const ISEITEM arm_instruction[];

#endif
