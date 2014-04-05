/*  armos.h -- ARMulator OS definitions:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

//#include "bank_defs.h"
//#include "dyncom/defines.h"

//typedef struct mmap_area{
//	mem_bank_t bank;
//	void *mmap_addr;
//	struct mmap_area *next;
//}mmap_area_t;

#if FAST_MEMORY
/* in user mode, mmap_base will be on initial brk,
   set at the first mmap request */
#define mmap_base -1
#else
#define mmap_base 0x50000000
#endif
static long mmap_next_base = mmap_base;

//static mmap_area_t* new_mmap_area(int sim_addr, int len);
static char mmap_mem_write(short size, int addr, uint32_t value);
static char mmap_mem_read(short size, int addr, uint32_t * value);

/***************************************************************************\
*                               SWI numbers                                 *
\***************************************************************************/

#define SWI_Syscall                0x0
#define SWI_Exit                   0x1
#define SWI_Read                   0x3
#define SWI_Write                  0x4
#define SWI_Open                   0x5
#define SWI_Close                  0x6
#define SWI_Seek                   0x13
#define SWI_Rename                 0x26
#define SWI_Break                  0x11

#define SWI_Times		   0x2b
#define SWI_Brk			   0x2d

#define SWI_Mmap                   0x5a
#define SWI_Munmap                 0x5b
#define SWI_Mmap2                  0xc0

#define SWI_GetUID32               0xc7
#define SWI_GetGID32               0xc8
#define SWI_GetEUID32              0xc9
#define SWI_GetEGID32              0xca

#define SWI_ExitGroup		   0xf8

#if 0
#define SWI_Time                   0xd
#define SWI_Clock                  0x61
#define SWI_Time                   0x63
#define SWI_Remove                 0x64
#define SWI_Rename                 0x65
#define SWI_Flen                   0x6c
#endif

#define SWI_Uname		   0x7a
#define SWI_Fcntl                  0xdd 
#define SWI_Fstat64  		   0xc5
#define SWI_Gettimeofday           0x4e
#define SWI_Set_tls                0xf0005

#define SWI_Breakpoint             0x180000	/* see gdb's tm-arm.h */

/***************************************************************************\
*                             SWI structures                                *
\***************************************************************************/

/* Arm binaries (for now) only support 32 bit, and expect to receive
   32-bit compliant structure in return of a systen call. Because
   we use host system calls to emulate system calls, the returned
   structure can be 32-bit compliant or 64-bit compliant, depending
   on the OS running skyeye. Therefore, we need a fixed size structure
   adapted to arm.*/

/* Borrowed from qemu */
struct target_stat64 {
	unsigned short	st_dev;
	unsigned char	__pad0[10];
	uint32_t	__st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	uint32_t	st_uid;
	uint32_t	st_gid;
	unsigned short	st_rdev;
	unsigned char	__pad3[10];
	unsigned char	__pad31[4];
	long long	st_size;
	uint32_t	st_blksize;
	unsigned char	__pad32[4];
	uint32_t	st_blocks;
	uint32_t	__pad4;
	uint32_t	st32_atime;
	uint32_t	__pad5;
	uint32_t	st32_mtime;
	uint32_t	__pad6;
	uint32_t	st32_ctime;
	uint32_t	__pad7;
	unsigned long long	st_ino;
};// __attribute__((packed));

struct target_tms32 {
    uint32_t tms_utime;
    uint32_t tms_stime;
    uint32_t tms_cutime;
    uint32_t tms_cstime;
};

struct target_timeval32 {
	uint32_t tv_sec;     /* seconds */
	uint32_t tv_usec;    /* microseconds */
};

struct target_timezone32 {
	int32_t tz_minuteswest;     /* minutes west of Greenwich */
	int32_t tz_dsttime;         /* type of DST correction */
};

