#include "core/arm/interpreter/armdefs.h"

/*chy 2004-06-06, fix bug found by wenye@cs.ucsb.edu*/
ARMword rb_masks[] = {
	0x0,			//RB_INVALID
	4,			//RB_1
	16,			//RB_4
	32,			//RB_8
};

/*mmu_rb_init
 * @rb_t	:rb_t to init
 * @num		:number of entry
 * */
int
mmu_rb_init (rb_s * rb_t, int num)
{
	int i;
	rb_entry_t *entrys;

	entrys = (rb_entry_t *) malloc (sizeof (*entrys) * num);
	if (entrys == NULL) {
		printf ("SKYEYE:mmu_rb_init malloc error\n");
		return -1;
	}
	for (i = 0; i < num; i++) {
		entrys[i].type = RB_INVALID;
		entrys[i].fault = NO_FAULT;
	}

	rb_t->entrys = entrys;
	rb_t->num = num;
	return 0;
}

/*mmu_rb_exit*/
void
mmu_rb_exit (rb_s * rb_t)
{
	free (rb_t->entrys);
};

/*mmu_rb_search
 * @rb_t	:rb_t to serach
 * @va		:va address to math
 *
 * $	NULL :not match
 * 		NO-NULL:
 * */
rb_entry_t *
mmu_rb_search (rb_s * rb_t, ARMword va)
{
	int i;
	rb_entry_t *rb = rb_t->entrys;

	DEBUG_LOG(ARM11, "va = %x\n", va);
	for (i = 0; i < rb_t->num; i++, rb++) {
		//2004-06-06 lyh  bug from wenye@cs.ucsb.edu
		if (rb->type) {
			if ((va >= rb->va)
			    && (va < (rb->va + rb_masks[rb->type])))
				return rb;
		}
	}
	return NULL;
};

void
mmu_rb_invalidate_entry (rb_s * rb_t, int i)
{
	rb_t->entrys[i].type = RB_INVALID;
}

void
mmu_rb_invalidate_all (rb_s * rb_t)
{
	int i;

	for (i = 0; i < rb_t->num; i++)
		mmu_rb_invalidate_entry (rb_t, i);
};

void
mmu_rb_load (ARMul_State * state, rb_s * rb_t, int i_rb, int type, ARMword va)
{
	rb_entry_t *rb;
	int i;
	ARMword max_start, min_end;
	fault_t fault;
	tlb_entry_t *tlb;

	/*align va according to type */
	va &= ~(rb_masks[type] - 1);
	/*invalidate all RB match [va, va + rb_masks[type]] */
	for (rb = rb_t->entrys, i = 0; i < rb_t->num; i++, rb++) {
		if (rb->type) {
			max_start = max (va, rb->va);
			min_end =
				min (va + rb_masks[type],
				     rb->va + rb_masks[rb->type]);
			if (max_start < min_end)
				rb->type = RB_INVALID;
		}
	}
	/*load word */
	rb = &rb_t->entrys[i_rb];
	rb->type = type;
	fault = translate (state, va, D_TLB (), &tlb);
	if (fault) {
		rb->fault = fault;
		return;
	}
	fault = check_access (state, va, tlb, 1);
	if (fault) {
		rb->fault = fault;
		return;
	}

	rb->fault = NO_FAULT;
	va = tlb_va_to_pa (tlb, va);
	//2004-06-06 lyh  bug from wenye@cs.ucsb.edu
	for (i = 0; i < (rb_masks[type] / 4); i++, va += WORD_SIZE) {
		//rb->data[i] = mem_read_word (state, va);
		bus_read(32, va, &rb->data[i]);
	};
}
