#ifndef _MMU_WB_H_
#define _MMU_WB_H_

typedef struct wb_entry_s
{
	ARMword pa;		//phy_addr
	ARMbyte *data;		//data
	int nb;			//number byte to write
} wb_entry_t;

typedef struct wb_s
{
	int num;		//number of wb_entry
	int nb;			//number of byte of each entry
	int first;		//
	int last;		//
	int used;		//
	wb_entry_t *entrys;
} wb_s;

typedef struct wb_desc_s
{
	int num;
	int nb;
} wb_desc_t;

/* wb_init
 * @wb_t	:wb_t to init
 * @num		:num of entrys
 * @nw		:num of word of each entry
 *
 * $	-1:error
 * 		 0:ok
 * */
int mmu_wb_init (wb_s * wb_t, int num, int nb);


/* wb_exit
 * @wb_t :wb_t to exit
 * */
void mmu_wb_exit (wb_s * wb);


/* wb_write_bytes :put bytess in Write Buffer
 * @state:	ARMul_State
 * @wb_t:	write buffer
 * @pa:		physical address
 * @data:	data ptr
 * @n		number of byte to write
 *
 * Note: write buffer merge is not implemented, can be done late
 * */
void
mmu_wb_write_bytess (ARMul_State * state, wb_s * wb_t, ARMword pa,
		     ARMbyte * data, int n);


/* wb_drain_all
 * @wb_t wb_t to drain
 * */
void mmu_wb_drain_all (ARMul_State * state, wb_s * wb_t);

#endif /*_MMU_WB_H_*/
