#ifndef _MMU_TLB_H_
#define _MMU_TLB_H_

typedef enum tlb_mapping_t
{
	TLB_INVALID = 0,
	TLB_SMALLPAGE = 1,
	TLB_LARGEPAGE = 2,
	TLB_SECTION = 3,
	TLB_ESMALLPAGE = 4,
	TLB_TINYPAGE = 5
} tlb_mapping_t;

extern ARMword tlb_masks[];

/* Permissions bits in a TLB entry:
 *
 *		 31         12 11 10  9  8  7  6  5  4  3   2   1   0
 *		+-------------+-----+-----+-----+-----+---+---+-------+
 * Page:|             | ap3 | ap2 | ap1 | ap0 | C | B |       |
 *		+-------------+-----+-----+-----+-----+---+---+-------+
 *
 *		 31         12 11 10  9              4  3   2   1   0
 *			+-------------+-----+-----------------+---+---+-------+
 * Section:	|             |  AP |                 | C | B |       |
 *			+-------------+-----+-----------------+---+---+-------+
 */

/*
section:
	section base address	[31:20]
	AP			- table 8-2, page 8-8
	domain
	C,B

page:
	page base address	[31:16] or [31:12]
	ap[3:0]
	domain (from L1)
	C,B
*/


typedef struct tlb_entry_t
{
	ARMword virt_addr;
	ARMword phys_addr;
	ARMword perms;
	ARMword domain;
	tlb_mapping_t mapping;
} tlb_entry_t;

typedef struct tlb_s
{
	int num;		/*num of tlb entry */
	int cycle;		/*current tlb cycle */
	tlb_entry_t *entrys;
} tlb_s;


#define tlb_c_flag(tlb) \
	((tlb)->perms & 0x8)
#define tlb_b_flag(tlb) \
	((tlb)->perms & 0x4)

#define  tlb_va_to_pa(tlb, va) \
(\
 {\
	ARMword mask = tlb_masks[tlb->mapping];      \
	(tlb->phys_addr & mask) | (va & ~mask);\
 }\
)

fault_t
check_access (ARMul_State * state, ARMword virt_addr, tlb_entry_t * tlb,
	      int read);

fault_t
translate (ARMul_State * state, ARMword virt_addr, tlb_s * tlb_t,
	   tlb_entry_t ** tlb);

int mmu_tlb_init (tlb_s * tlb_t, int num);

void mmu_tlb_exit (tlb_s * tlb_t);

void mmu_tlb_invalidate_all (ARMul_State * state, tlb_s * tlb_t);

void
mmu_tlb_invalidate_entry (ARMul_State * state, tlb_s * tlb_t, ARMword addr);

tlb_entry_t *mmu_tlb_search (ARMul_State * state, tlb_s * tlb_t,
			     ARMword virt_addr);

#endif	    /*_MMU_TLB_H_*/
