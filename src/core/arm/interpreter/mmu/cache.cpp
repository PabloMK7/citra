#include "core/arm/interpreter/armdefs.h"

/* mmu cache init
 *
 * @cache_t :cache_t to init
 * @width	:cache line width in byte
 * @way		:way of each cache set
 * @set		:cache set num
 *
 * $ -1: error
 * 	 0: sucess
 */
int
mmu_cache_init (cache_s * cache_t, int width, int way, int set, int w_mode)
{
	int i, j;
	cache_set_t *sets;
	cache_line_t *lines;

	/*alloc cache set */
	sets = NULL;
	lines = NULL;
	//fprintf(stderr, "mmu_cache_init: mallloc beg size %d,sets 0x%x\n", sizeof(cache_set_t) * set,sets);
	//exit(-1);
	sets = (cache_set_t *) malloc (sizeof (cache_set_t) * set);
	if (sets == NULL) {
		ERROR_LOG(ARM11, "set malloc size %d\n", sizeof (cache_set_t) * set);
		goto sets_error;
	}
	//fprintf(stderr, "mmu_cache_init: mallloc end sets 0x%x\n", sets);
	cache_t->sets = sets;

	/*init cache set */
	for (i = 0; i < set; i++) {
		/*alloc cache line */
		lines = (cache_line_t *) malloc (sizeof (cache_line_t) * way);
		if (lines == NULL) {
			ERROR_LOG(ARM11, "line malloc size %d\n",
				 sizeof (cache_line_t) * way);
			goto lines_error;
		}
		/*init cache line */
		for (j = 0; j < way; j++) {
			lines[j].tag = 0;	//invalid
			lines[j].data = (ARMword *) malloc (width);
			if (lines[j].data == NULL) {
				ERROR_LOG(ARM11, "data alloc size %d\n", width);
				goto data_error;
			}
		}

		sets[i].lines = lines;
		sets[i].cycle = 0;

	}
	cache_t->width = width;
	cache_t->set = set;
	cache_t->way = way;
	cache_t->w_mode = w_mode;
	return 0;

      data_error:
	/*free data */
	while (j-- > 0)
		free (lines[j].data);
	/*free data error line */
	free (lines);
      lines_error:
	/*free lines already alloced */
	while (i-- > 0) {
		for (j = 0; j < way; j++)
			free (sets[i].lines[j].data);
		free (sets[i].lines);
	}
	/*free sets */
	free (sets);
      sets_error:
	return -1;
};

/* free a cache_t's inner data, the ptr self is not freed,
 * when needed do like below:
 * 		mmu_cache_exit(cache);
 * 		free(cache_t);
 *
 * @cache_t : the cache_t to free
 */

void
mmu_cache_exit (cache_s * cache_t)
{
	int i, j;
	cache_set_t *sets, *set;
	cache_line_t *lines, *line;

	/*free all set */
	sets = cache_t->sets;
	for (set = sets, i = 0; i < cache_t->set; i++, set++) {
		/*free all line */
		lines = set->lines;
		for (line = lines, j = 0; j < cache_t->way; j++, line++)
			free (line->data);
		free (lines);
	}
	free (sets);
}

/* mmu cache search
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to search
 * @va		:virtual address
 *
 * $	NULL:	no cache match
 * 		cache	:cache matched
 */
cache_line_t *
mmu_cache_search (ARMul_State * state, cache_s * cache_t, ARMword va)
{
	int i;
	int set = va_cache_set (va, cache_t);
	ARMword tag = va_cache_align (va, cache_t);
	cache_line_t *cache;

	cache_set_t *cache_set = cache_t->sets + set;
	for (i = 0, cache = cache_set->lines; i < cache_t->way; i++, cache++) {
		if ((cache->tag & TAG_VALID_FLAG)
		    && (tag == va_cache_align (cache->tag, cache_t)))
			return cache;
	}
	return NULL;
}

/* mmu cache search by set/index
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to search
 * @index	:set/index value. 
 *
 * $	NULL:	no cache match
 * 		cache	:cache matched
 */
cache_line_t *
mmu_cache_search_by_index (ARMul_State * state, cache_s * cache_t,
			   ARMword index)
{
	int way = cache_t->way;
	int set_v = index_cache_set (index, cache_t);
	int i = 0, index_v = 0;
	cache_set_t *set;

	while ((way >>= 1) >= 1)
		i++;
	index_v = index >> (32 - i);
	set = cache_t->sets + set_v;

	return set->lines + index_v;
}


/* mmu cache alloc
 *
 * @state :ARMul_State
 * @cache_t	:cache_t to alloc from
 * @va		:virtual address that require cache alloc, need not cache aligned
 * @pa		:physical address of va
 *
 * $	cache_alloced, always alloc OK
 */
cache_line_t *
mmu_cache_alloc (ARMul_State * state, cache_s * cache_t, ARMword va,
		 ARMword pa)
{
	cache_line_t *cache;
	cache_set_t *set;
	int i;

	va = va_cache_align (va, cache_t);
	pa = va_cache_align (pa, cache_t);

	set = &cache_t->sets[va_cache_set (va, cache_t)];

	/*robin-round */
	cache = &set->lines[set->cycle++];
	if (set->cycle == cache_t->way)
		set->cycle = 0;

	if (cache_t->w_mode == CACHE_WRITE_BACK) {
		ARMword t;

		/*if cache valid, try to write back */
		if (cache->tag & TAG_VALID_FLAG) {
			mmu_cache_write_back (state, cache_t, cache);
		}
		/*read in cache_line */
		t = pa;
		for (i = 0; i < (cache_t->width >> WORD_SHT);
		     i++, t += WORD_SIZE) {
			//cache->data[i] = mem_read_word (state, t);
			bus_read(32, t, &cache->data[i]);
		}
	}
	/*store tag and pa */
	cache->tag = va | TAG_VALID_FLAG;
	cache->pa = pa;

	return cache;
};

/* mmu_cache_write_back write cache data to memory
 * @state
 * @cache_t :cache_t of the cache line
 * @cache : cache line
 */
void
mmu_cache_write_back (ARMul_State * state, cache_s * cache_t,
		      cache_line_t * cache)
{
	ARMword pa = cache->pa;
	int nw = cache_t->width >> WORD_SHT;
	ARMword *data = cache->data;
	int i;
	int t0, t1, t2;

	if ((cache->tag & 1) == 0)
		return;

	switch (cache->
		tag & ~1 & (TAG_FIRST_HALF_DIRTY | TAG_LAST_HALF_DIRTY)) {
	case 0:
		return;
	case TAG_FIRST_HALF_DIRTY:
		nw /= 2;
		break;
	case TAG_LAST_HALF_DIRTY:
		nw /= 2;
		pa += nw << WORD_SHT;
		data += nw;
		break;
	case TAG_FIRST_HALF_DIRTY | TAG_LAST_HALF_DIRTY:
		break;
	}
	for (i = 0; i < nw; i++, data++, pa += WORD_SIZE)
		//mem_write_word (state, pa, *data);
		bus_write(32, pa, *data);

	cache->tag &= ~(TAG_FIRST_HALF_DIRTY | TAG_LAST_HALF_DIRTY);
};


/* mmu_cache_clean: clean a cache of va in cache_t
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to clean
 * @va		:virtaul address
 */
void
mmu_cache_clean (ARMul_State * state, cache_s * cache_t, ARMword va)
{
	cache_line_t *cache;

	cache = mmu_cache_search (state, cache_t, va);
	if (cache)
		mmu_cache_write_back (state, cache_t, cache);
}

/* mmu_cache_clean_by_index: clean a cache by set/index format value
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to clean
 * @va		:set/index format value
 */
void
mmu_cache_clean_by_index (ARMul_State * state, cache_s * cache_t,
			  ARMword index)
{
	cache_line_t *cache;

	cache = mmu_cache_search_by_index (state, cache_t, index);
	if (cache)
		mmu_cache_write_back (state, cache_t, cache);
}

/* mmu_cache_invalidate : invalidate a cache of va
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to invalid
 * @va		:virt_addr to invalid
 */
void
mmu_cache_invalidate (ARMul_State * state, cache_s * cache_t, ARMword va)
{
	cache_line_t *cache;

	cache = mmu_cache_search (state, cache_t, va);
	if (cache) {
		mmu_cache_write_back (state, cache_t, cache);
		cache->tag = 0;
	}
}

/* mmu_cache_invalidate_by_index : invalidate a cache by index format
 *
 * @state	:ARMul_State
 * @cache_t	:cache_t to invalid
 * @index	:set/index data
 */
void
mmu_cache_invalidate_by_index (ARMul_State * state, cache_s * cache_t,
			       ARMword index)
{
	cache_line_t *cache;

	cache = mmu_cache_search_by_index (state, cache_t, index);
	if (cache) {
		mmu_cache_write_back (state, cache_t, cache);
		cache->tag = 0;
	}
}

/* mmu_cache_invalidate_all
 *
 * @state:
 * @cache_t
 * */
void
mmu_cache_invalidate_all (ARMul_State * state, cache_s * cache_t)
{
	int i, j;
	cache_set_t *set;
	cache_line_t *cache;

	set = cache_t->sets;
	for (i = 0; i < cache_t->set; i++, set++) {
		cache = set->lines;
		for (j = 0; j < cache_t->way; j++, cache++) {
			mmu_cache_write_back (state, cache_t, cache);
			cache->tag = 0;
		}
	}
};

void
mmu_cache_soft_flush (ARMul_State * state, cache_s * cache_t, ARMword pa)
{
	ARMword set, way;
	cache_line_t *cache;
	pa = (pa / cache_t->width);
	way = pa & (cache_t->way - 1);
	set = (pa / cache_t->way) & (cache_t->set - 1);
	cache = &cache_t->sets[set].lines[way];

	mmu_cache_write_back (state, cache_t, cache);
	cache->tag = 0;
}

cache_line_t*  mmu_cache_dirty_cache(ARMul_State *state,cache_s *cache){
	int i;
	int j;
	cache_line_t *cache_line = NULL;
	cache_set_t *cache_set = cache->sets;
	int sets = cache->set;
	for (i = 0; i < sets; i++){
		for(j = 0,cache_line = &cache_set[i].lines[0]; j < cache->way; j++,cache_line++){
			if((cache_line->tag & TAG_FIRST_HALF_DIRTY) || (cache_line->tag & TAG_LAST_HALF_DIRTY))
				return cache_line;
		}
	}
	return NULL;
}
