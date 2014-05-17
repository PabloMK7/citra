#include <assert.h>

#include "core/arm/interpreter/armdefs.h"

ARMword tlb_masks[] = {
	0x00000000,		/* TLB_INVALID */
	0xFFFFF000,		/* TLB_SMALLPAGE */
	0xFFFF0000,		/* TLB_LARGEPAGE */
	0xFFF00000,		/* TLB_SECTION */
	0xFFFFF000,		/*TLB_ESMALLPAGE, have TEX attirbute, only for XScale */
	0xFFFFFC00		/* TLB_TINYPAGE */
};

/* This function encodes table 8-2 Interpreting AP bits,
   returning non-zero if access is allowed. */
static int
check_perms (ARMul_State * state, int ap, int read)
{
	int s, r, user;

	s = state->mmu.control & CONTROL_SYSTEM;
	r = state->mmu.control & CONTROL_ROM;
	//chy 2006-02-15 , should consider system mode, don't conside 26bit mode
	user = (state->Mode == USER32MODE) || (state->Mode == USER26MODE) || (state->Mode == SYSTEM32MODE);

	switch (ap) {
	case 0:
		return read && ((s && !user) || r);
	case 1:
		return !user;
	case 2:
		return read || !user;
	case 3:
		return 1;
	}
	return 0;
}

fault_t
check_access (ARMul_State * state, ARMword virt_addr, tlb_entry_t * tlb,
	      int read)
{
	int access;

	state->mmu.last_domain = tlb->domain;
	access = (state->mmu.domain_access_control >> (tlb->domain * 2)) & 3;
	if ((access == 0) || (access == 2)) {
		/* It's unclear from the documentation whether this
		   should always raise a section domain fault, or if
		   it should be a page domain fault in the case of an
		   L1 that describes a page table.  In the ARM710T
		   datasheets, "Figure 8-9: Sequence for checking faults"
		   seems to indicate the former, while "Table 8-4: Priority
		   encoding of fault status" gives a value for FS[3210] in
		   the event of a domain fault for a page.  Hmm. */
		return SECTION_DOMAIN_FAULT;
	}
	if (access == 1) {
		/* client access - check perms */
		int subpage, ap;

		switch (tlb->mapping) {
			/*ks 2004-05-09   
			 *   only for XScale
			 *   Extend Small Page(ESP) Format
			 *   31-12 bits    the base addr of ESP
			 *   11-10 bits    SBZ
			 *   9-6   bits    TEX
			 *   5-4   bits    AP
			 *   3     bit     C
			 *   2     bit     B
			 *   1-0   bits    11
			 * */
		case TLB_ESMALLPAGE:	//xj
			subpage = 0;
			//printf("TLB_ESMALLPAGE virt_addr=0x%x  \n",virt_addr );
			break;

		case TLB_TINYPAGE:
			subpage = 0;
			//printf("TLB_TINYPAGE virt_addr=0x%x  \n",virt_addr );
			break;

		case TLB_SMALLPAGE:
			subpage = (virt_addr >> 10) & 3;
			break;
		case TLB_LARGEPAGE:
			subpage = (virt_addr >> 14) & 3;
			break;
		case TLB_SECTION:
			subpage = 3;
			break;
		default:
			assert (0);
			subpage = 0;	/* cleans a warning */
		}
		ap = (tlb->perms >> (subpage * 2 + 4)) & 3;
		if (!check_perms (state, ap, read)) {
			if (tlb->mapping == TLB_SECTION) {
				return SECTION_PERMISSION_FAULT;
			}
			else {
				return SUBPAGE_PERMISSION_FAULT;
			}
		}
	}
	else {			/* access == 3 */
		/* manager access - don't check perms */
	}
	return NO_FAULT;
}

fault_t
translate (ARMul_State * state, ARMword virt_addr, tlb_s * tlb_t,
	   tlb_entry_t ** tlb)
{
	*tlb = mmu_tlb_search (state, tlb_t, virt_addr);
	if (!*tlb) {
		/* walk the translation tables */
		ARMword l1addr, l1desc;
		tlb_entry_t entry;

		l1addr = state->mmu.translation_table_base & 0xFFFFC000;
		l1addr = (l1addr | (virt_addr >> 18)) & ~3;
		//l1desc = mem_read_word (state, l1addr);
		bus_read(32, l1addr, &l1desc);
		switch (l1desc & 3) {
		case 0:
			/* 
			 * according to Figure 3-9 Sequence for checking faults in arm manual,
			 * section translation fault should be returned here. 
			 */
			{
				return SECTION_TRANSLATION_FAULT;
			}
		case 3:
			/* fine page table */
			// dcl 2006-01-08
			{
				ARMword l2addr, l2desc;

				l2addr = l1desc & 0xFFFFF000;
				l2addr = (l2addr |
					  ((virt_addr & 0x000FFC00) >> 8)) &
					~3;
				//l2desc = mem_read_word (state, l2addr);				 
				bus_read(32, l2addr, &l2desc);

				entry.virt_addr = virt_addr;
				entry.phys_addr = l2desc;
				entry.perms = l2desc & 0x00000FFC;
				entry.domain = (l1desc >> 5) & 0x0000000F;
				switch (l2desc & 3) {
				case 0:
					state->mmu.last_domain = entry.domain;
					return PAGE_TRANSLATION_FAULT;
				case 3:
					entry.mapping = TLB_TINYPAGE;
					break;
				case 1:
					// this is untested
					entry.mapping = TLB_LARGEPAGE;
					break;
				case 2:
					// this is untested
					entry.mapping = TLB_SMALLPAGE;
					break;
				}
			}
			break;
		case 1:
			/* coarse page table */
			{
				ARMword l2addr, l2desc;

				l2addr = l1desc & 0xFFFFFC00;
				l2addr = (l2addr |
					  ((virt_addr & 0x000FF000) >> 10)) &
					~3;
				//l2desc = mem_read_word (state, l2addr);
				bus_read(32, l2addr, &l2desc);

				entry.virt_addr = virt_addr;
				entry.phys_addr = l2desc;
				entry.perms = l2desc & 0x00000FFC;
				entry.domain = (l1desc >> 5) & 0x0000000F;
				//printf("SKYEYE:PAGE virt_addr = %x,l1desc=%x,phys_addr=%x\n",virt_addr,l1desc,entry.phys_addr);
				//chy 2003-09-02 for xscale
				switch (l2desc & 3) {
				case 0:
					state->mmu.last_domain = entry.domain;
					return PAGE_TRANSLATION_FAULT;
				case 3:
					if (!state->is_XScale) {
						state->mmu.last_domain =
							entry.domain;
						return PAGE_TRANSLATION_FAULT;
					};
					//ks 2004-05-09 xscale shold use Extend Small Page
					//entry.mapping = TLB_SMALLPAGE;
					entry.mapping = TLB_ESMALLPAGE;	//xj
					break;
				case 1:
					entry.mapping = TLB_LARGEPAGE;
					break;
				case 2:
					entry.mapping = TLB_SMALLPAGE;
					break;
				}
			}
			break;
		case 2:
			/* section */
			//printf("SKYEYE:WARNING: not implement section mapping incompletely\n");
			//printf("SKYEYE:SECTION virt_addr = %x,l1desc=%x\n",virt_addr,l1desc);
			//return SECTION_DOMAIN_FAULT;
			//#if 0
			entry.virt_addr = virt_addr;
			entry.phys_addr = l1desc;
			entry.perms = l1desc & 0x00000C0C;
			entry.domain = (l1desc >> 5) & 0x0000000F;
			entry.mapping = TLB_SECTION;
			break;
			//#endif
		}
		entry.virt_addr &= tlb_masks[entry.mapping];
		entry.phys_addr &= tlb_masks[entry.mapping];

		/* place entry in the tlb */
		*tlb = &tlb_t->entrys[tlb_t->cycle];
		tlb_t->cycle = (tlb_t->cycle + 1) % tlb_t->num;
		**tlb = entry;
	}
	state->mmu.last_domain = (*tlb)->domain;
	return NO_FAULT;
}

int
mmu_tlb_init (tlb_s * tlb_t, int num)
{
	tlb_entry_t *e;
	int i;

	e = (tlb_entry_t *) malloc (sizeof (*e) * num);
	if (e == NULL) {
		ERROR_LOG(ARM11, "malloc size %d\n", sizeof (*e) * num);
		goto tlb_malloc_error;
	}
	tlb_t->entrys = e;
	for (i = 0; i < num; i++, e++)
		e->mapping = TLB_INVALID;
	tlb_t->cycle = 0;
	tlb_t->num = num;
	return 0;

      tlb_malloc_error:
	return -1;
}

void
mmu_tlb_exit (tlb_s * tlb_t)
{
	free (tlb_t->entrys);
};

void
mmu_tlb_invalidate_all (ARMul_State * state, tlb_s * tlb_t)
{
	int entry;

	for (entry = 0; entry < tlb_t->num; entry++) {
		tlb_t->entrys[entry].mapping = TLB_INVALID;
	}
	tlb_t->cycle = 0;
}

void
mmu_tlb_invalidate_entry (ARMul_State * state, tlb_s * tlb_t, ARMword addr)
{
	tlb_entry_t *tlb;

	tlb = mmu_tlb_search (state, tlb_t, addr);
	if (tlb) {
		tlb->mapping = TLB_INVALID;
	}
}

tlb_entry_t *
mmu_tlb_search (ARMul_State * state, tlb_s * tlb_t, ARMword virt_addr)
{
	int entry;

	for (entry = 0; entry < tlb_t->num; entry++) {
		tlb_entry_t *tlb;
		ARMword mask;

		tlb = &(tlb_t->entrys[entry]);
		if (tlb->mapping == TLB_INVALID) {
			continue;
		}
		mask = tlb_masks[tlb->mapping];
		if ((virt_addr & mask) == (tlb->virt_addr & mask)) {
			return tlb;
		}
	}
	return NULL;
}
