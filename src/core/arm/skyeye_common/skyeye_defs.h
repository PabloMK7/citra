#pragma once

#include "common/common.h"

#define MODE32
#define MODET

typedef struct
{
	const char *cpu_arch_name; /* CPU architecture version name.e.g. armv4t */
	const char *cpu_name;      /* CPU name. e.g. arm7tdmi or arm720t */
	u32 cpu_val;               /*CPU value; also call MMU ID or processor id;see
	                             ARM Architecture Reference Manual B2-6 */
	u32 cpu_mask;              /* cpu_val's mask. */
	u32 cachetype;             /* this CPU has what kind of cache */
} cpu_config_t;

typedef enum {
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
} exception_t;

typedef enum {
	Align = 0,
	UnAlign	
} align_t;

typedef enum {
	Little_endian = 0,
	Big_endian
} endian_t;

typedef enum {
	Phys_addr = 0,
	Virt_addr
} addr_type_t;

typedef u32 addr_t;
