#ifndef _MMU_CACHE_H_
#define _MMU_CACHE_H_

typedef struct cache_line_t
{
	ARMword tag;		/*      cache line align address |
				   bit2: last half dirty
				   bit1: first half dirty
				   bit0: cache valid flag
				 */
	ARMword pa;		/*physical address */
	ARMword *data;		/*array of cached data */
} cache_line_t;
#define TAG_VALID_FLAG 0x00000001
#define TAG_FIRST_HALF_DIRTY 0x00000002
#define TAG_LAST_HALF_DIRTY	0x00000004

/*cache set association*/
typedef struct cache_set_s
{
	cache_line_t *lines;
	int cycle;
} cache_set_t;

enum
{
	CACHE_WRITE_BACK,
	CACHE_WRITE_THROUGH,
};

typedef struct cache_s
{
	int width;		/*bytes in a line */
	int way;		/*way of set asscociate */
	int set;		/*num of set */
	int w_mode;		/*write back or write through */
	//int a_mode;   /*alloc mode: random or round-bin*/
	cache_set_t *sets;
  /**/} cache_s;

typedef struct cache_desc_s
{
	int width;
	int way;
	int set;
	int w_mode;
//      int a_mode;
} cache_desc_t;


/*virtual address to cache set index*/
#define va_cache_set(va, cache_t) \
	(((va) / (cache_t)->width) & ((cache_t)->set - 1))
/*virtual address to cahce line aligned*/
#define va_cache_align(va, cache_t) \
		((va) & ~((cache_t)->width - 1))
/*virtaul address to cache line word index*/
#define va_cache_index(va, cache_t) \
		(((va) & ((cache_t)->width - 1)) >> WORD_SHT)

/*see Page 558 in arm manual*/
/*set/index format value to cache set value*/
#define index_cache_set(index, cache_t) \
	(((index) / (cache_t)->width) & ((cache_t)->set - 1))

/*************************cache********************/
/* mmu cache init
 *
 * @cache_t :cache_t to init
 * @width	:cache line width in byte
 * @way		:way of each cache set
 * @set		:cache set num
 * @w_mode	:cache w_mode
 *
 * $ -1: error
 * 	 0: sucess
 */
int
mmu_cache_init (cache_s * cache_t, int width, int way, int set, int w_mode);

/* free a cache_t's inner data, the ptr self is not freed,
 * when needed do like below:
 * 		mmu_cache_exit(cache);
 * 		free(cache_t);
 *
 * @cache_t : the cache_t to free
 */
void mmu_cache_exit (cache_s * cache_t);

/* mmu cache search
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to search
 * @va		:virtual address
 *
 * $	NULL:	no cache match
 * 		cache	:cache matched
 * */
cache_line_t *mmu_cache_search (ARMul_State * state, cache_s * cache_t,
				ARMword va);

/*  mmu cache search by set/index 
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to search
 * @index       :set/index value. 
 *
 * $	NULL:	no cache match
 * 		cache	:cache matched
 * */

cache_line_t *mmu_cache_search_by_index (ARMul_State * state,
					 cache_s * cache_t, ARMword index);

/* mmu cache alloc
 *
 * @state :ARMul_State
 * @cache_t	:cache_t to alloc from
 * @va		:virtual address that require cache alloc, need not cache aligned
 * @pa		:physical address of va
 *
 * $	cache_alloced, always alloc OK
 */
cache_line_t *mmu_cache_alloc (ARMul_State * state, cache_s * cache_t,
			       ARMword va, ARMword pa);

/* mmu_cache_write_back write cache data to memory
 *
 * @state:
 * @cache_t :cache_t of the cache line
 * @cache : cache line
 */
void
mmu_cache_write_back (ARMul_State * state, cache_s * cache_t,
		      cache_line_t * cache);

/* mmu_cache_clean: clean a cache of va in cache_t
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to clean
 * @va		:virtaul address
 */
void mmu_cache_clean (ARMul_State * state, cache_s * cache_t, ARMword va);
void
mmu_cache_clean_by_index (ARMul_State * state, cache_s * cache_t,
			  ARMword index);

/* mmu_cache_invalidate : invalidate a cache of va
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to invalid
 * @va		:virt_addr to invalid
 */
void
mmu_cache_invalidate (ARMul_State * state, cache_s * cache_t, ARMword va);

void
mmu_cache_invalidate_by_index (ARMul_State * state, cache_s * cache_t,
			       ARMword index);

void mmu_cache_invalidate_all (ARMul_State * state, cache_s * cache_t);

void
mmu_cache_soft_flush (ARMul_State * state, cache_s * cache_t, ARMword pa);

cache_line_t* mmu_cache_dirty_cache(ARMul_State * state, cache_s * cache_t);

#endif /*_MMU_CACHE_H_*/
