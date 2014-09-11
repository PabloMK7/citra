#ifndef CORE_ARM_SKYEYE_DEFS_H_
#define CORE_ARM_SKYEYE_DEFS_H_

#include "common/common.h"

#define MODE32
#define MODET

typedef struct
{
	const char *cpu_arch_name;	/*cpu architecture version name.e.g. armv4t */
	const char *cpu_name;	/*cpu name. e.g. arm7tdmi or arm720t */
	u32 cpu_val;	/*CPU value; also call MMU ID or processor id;see
				   ARM Architecture Reference Manual B2-6 */
	u32 cpu_mask;	/*cpu_val's mask. */
	u32 cachetype;	/*this cpu has what kind of cache */
} cpu_config_t;

typedef struct conf_object_s{
	char* objname;
	void* obj;
	char* class_name;
}conf_object_t;

typedef enum{
	/* No exception */
	No_exp = 0,
	/* Memory allocation exception */
	Malloc_exp,
	/* File open exception */
	File_open_exp,
	/* DLL open exception */
	Dll_open_exp,
	/* Invalid argument exception */
	Invarg_exp,
	/* Invalid module exception */
	Invmod_exp,
	/* wrong format exception for config file parsing */
	Conf_format_exp,
	/* some reference excess the predefiend range. Such as the index out of array range */
	Excess_range_exp,
	/* Can not find the desirable result */
	Not_found_exp,

	/* Unknown exception */
	Unknown_exp
}exception_t;

typedef enum {
	Align = 0,
	UnAlign	
}align_t;

typedef enum {
	Little_endian = 0,
	Big_endian
}endian_t;
//typedef int exception_t;

typedef enum{
	Phys_addr = 0,
	Virt_addr
}addr_type_t;

typedef exception_t(*read_byte_t)(conf_object_t* target, u32 addr, void *buf, size_t count);
typedef exception_t(*write_byte_t)(conf_object_t* target, u32 addr, const void *buf, size_t count);

typedef struct memory_space{
	conf_object_t* conf_obj;
	read_byte_t read;
	write_byte_t write;
}memory_space_intf;


/*
 * a running instance for a specific archteciture.
 */
typedef struct generic_arch_s
{
	char* arch_name;
	void (*init) (void);
	void (*reset) (void);
	void (*step_once) (void);
	void (*set_pc)(u32 addr);
	u32 (*get_pc)(void);
	u32 (*get_step)(void);
	//chy 2004-04-15 
	//int (*ICE_write_byte) (u32 addr, uint8_t v);
	//int (*ICE_read_byte)(u32 addr, uint8_t *pv);
	u32 (*get_regval_by_id)(int id);
	u32 (*get_regnum)(void);
	char* (*get_regname_by_id)(int id);
	exception_t (*set_regval_by_id)(int id, u32 value);
	/*
	 * read a data by virtual address.
	 */
	exception_t (*mmu_read)(short size, u32 addr, u32 * value);
	/*
	 * write a data by a virtual address.
	 */
	exception_t (*mmu_write)(short size, u32 addr, u32 value);
	/**
	 * get a signal from external
	 */
	//exception_t (*signal)(interrupt_signal_t* signal);

	endian_t endianess;
	align_t alignment;
} generic_arch_t;

#endif