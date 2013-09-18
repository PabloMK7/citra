#ifndef _MMU_RB_H
#define _MMU_RB_H

enum rb_type_t
{
	RB_INVALID = 0,		//invalid
	RB_1,			//1     word
	RB_4,			//4 word
	RB_8,			//8 word
};

/*bytes of each rb_type*/
extern ARMword rb_masks[];

#define RB_WORD_NUM 8
typedef struct rb_entry_s
{
	ARMword data[RB_WORD_NUM];	//array to store data
	ARMword va;		//first word va
	int type;		//rb type
	fault_t fault;		//fault set by rb alloc
} rb_entry_t;

typedef struct rb_s
{
	int num;
	rb_entry_t *entrys;
} rb_s;

/*mmu_rb_init
 * @rb_t	:rb_t to init
 * @num		:number of entry
 * */
int mmu_rb_init (rb_s * rb_t, int num);

/*mmu_rb_exit*/
void mmu_rb_exit (rb_s * rb_t);


/*mmu_rb_search
 * @rb_t	:rb_t to serach
 * @va		:va address to math
 *
 * $	NULL :not match
 * 		NO-NULL:
 * */
rb_entry_t *mmu_rb_search (rb_s * rb_t, ARMword va);


void mmu_rb_invalidate_entry (rb_s * rb_t, int i);
void mmu_rb_invalidate_all (rb_s * rb_t);
void mmu_rb_load (ARMul_State * state, rb_s * rb_t, int i_rb,
		  int type, ARMword va);

#endif /*_MMU_RB_H_*/
