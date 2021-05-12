#include <memory.h>
#include <slab.h>

static void allocate_early_or_late(uintptr_t *phys,
  uintptr_t *oaddr,
  void **virt,
  struct kheap_run **_run)
{
	if(mm_ready) {
		struct kheap_run *run = kheap_allocate(0x1000);
		if(oaddr)
			*oaddr = kheap_run_get_objspace(run);
		if(phys)
			*phys = kheap_run_get_phys(run);
		*_run = run;
		*virt = run->start;
		memset(run->start, 0, 0x1000);
	} else {
		uintptr_t p;
		mm_early_alloc(&p, virt, 0x1000, 0x1000);
		if(oaddr)
			*oaddr = p;
		if(phys)
			*phys = p;
	}
}

#define NR_BOOTSTRAP_TABLES 1024

static struct table_level bootstrap_table_levels[NR_BOOTSTRAP_TABLES];
static _Atomic size_t bootstrap_table_levels_idx = 0;

static void table_level_init(__unused void *_p, void *obj)
{
	struct table_level *tl = obj;
	tl->lock = RWLOCK_INIT;
	tl->flags |= TABLE_RUN_ALLOCATED;
}

static void table_level_ctor(__unused void *_p, void *obj)
{
	struct table_level *tl = obj;
	tl->parent = NULL;
	tl->parent_idx = -1;
	tl->count = 0;
}

static void table_level_fini(__unused void *_p, void *obj)
{
	struct table_level *tl = obj;
	if(tl->table_run) {
		kheap_free(tl->table_run);
	}
	if(tl->children_run) {
		kheap_free(tl->children_run);
	}
	memset(tl, 0, sizeof(*tl));
}

static DECLARE_SLABCACHE(sc_table_level,
  sizeof(struct table_level),
  table_level_init,
  table_level_ctor,
  NULL,
  table_level_fini,
  NULL);

static struct table_level *allocate_table_level(void)
{
	/* do this read to avoid extra writes */
	if(bootstrap_table_levels_idx < NR_BOOTSTRAP_TABLES) {
		size_t idx = bootstrap_table_levels_idx++;
		if(idx < NR_BOOTSTRAP_TABLES) {
			struct table_level *tl = &bootstrap_table_levels[idx];
			tl->lock = RWLOCK_INIT;
			return tl;
		}
	}
	return slabcache_alloc(&sc_table_level);
}

void table_realize(struct table_level *table, bool ospace)
{
	if(table->children == NULL) {
		allocate_early_or_late(NULL, NULL, (void **)&table->children, &table->children_run);
	}
	if(table->table == NULL) {
		if(ospace)
			allocate_early_or_late(&table->phys, NULL, (void **)&table->table, &table->table_run);
		else
			allocate_early_or_late(NULL, &table->phys, (void **)&table->table, &table->table_run);
	}
}

static bool need_realize(struct table_level *table)
{
	return !table->children || !table->table;
}

struct table_level *table_get_next_level(struct table_level *table,
  int idx,
  uint64_t flags,
  bool ospace,
  struct rwlock_result *lock_state)
{
	if(table->children[idx]) {
		struct table_level *next = table->children[idx];
		if(need_realize(table)) {
			if(lock_state)
				*lock_state = rwlock_downgrade(lock_state);
			table_realize(next, ospace);
			if(lock_state)
				*lock_state = rwlock_upgrade(lock_state, 0);
		}
		return next;
	}
	struct table_level *nt = allocate_table_level();
	if(need_realize(nt)) {
		if(lock_state)
			*lock_state = rwlock_downgrade(lock_state);
		table_realize(nt, ospace);
		if(lock_state)
			*lock_state = rwlock_upgrade(lock_state, 0);
	}
	table->children[idx] = nt;
	table->table[idx] = nt->phys | flags;
	table->count++;
	nt->parent = table;
	nt->parent_idx = idx;

	return nt;
}

static void table_free_table_level(struct table_level *table)
{
	for(int i = 0; i < 512; i++) {
		if(table->table) {
			table->table[i] = 0;
		}
		if(table->children[i]) {
			table->children[i] = NULL;
		}
	}
	if(table->flags & TABLE_RUN_ALLOCATED) {
		slabcache_free(&sc_table_level, table);
	} else {
		table_level_fini(NULL, table);
	}
}

static void table_free_downward(struct table_level *table)
{
	for(int i = 0; i < 512; i++) {
		if(table->children[i]) {
			table_free_downward(table->children[i]);
		}
	}
	assert(table->count == 0);
	table_free_table_level(table);
}

static void table_remove_entry(struct table_level *table, size_t idx)
{
	assert(table->count);
	if(table->children[idx]) {
		table_free_downward(table->children[idx]);
		table->children[idx] = NULL;
		table->count--;
	} else if(table->table[idx]) {
		assert(table->table[idx] & PAGE_PRESENT);
		table->table[idx] = 0;
		table->count--;
	}

	if(table->count == 0 && table->parent) {
		table_remove_entry(table->parent, table->parent_idx);
		table_free_table_level(table);
	}
}

void table_map(struct table_level *table,
  uintptr_t virt,
  uintptr_t phys,
  int level,
  uint64_t flags,
  uint64_t table_flags,
  bool ospace)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	struct rwlock_result res = rwlock_rlock(&table->lock, 0);

	table_realize(table, ospace);
	res = rwlock_upgrade(&res, 0);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags, ospace, &res);
	}

	int idx = idxs[i];

	if(table->children[idx]) {
		table_free_downward(table->children[idx]);
		table->count--;
		table->children[idx] = NULL;
	}
	if(level != 0)
		flags |= PAGE_LARGE;
	// printk("traversed %d\n", (3 - level));
	// printk(":: %lx: %lx %lx %lx\n", virt, phys, flags, table_flags);
	table->table[idx] = phys | flags;
	table->count++;

	rwlock_wunlock(&res);
}

bool table_readmap(struct table_level *table, uintptr_t virt, uint64_t *entry, int *level)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	struct rwlock_result res = rwlock_rlock(&table->lock, 0);

	int i;
	for(i = 0; i < 4; i++) {
		if(!table->table) {
			rwlock_runlock(&res);
			return false;
		}
		if(table->children[idxs[i]])
			table = table->children[idxs[i]];
		else {
			if(entry)
				*entry = table->table[idxs[i]];
			if(level)
				*level = 3 - i;
			rwlock_runlock(&res);
			return true;
		}
	}
	rwlock_runlock(&res);
	return false;
}

void table_premap(struct table_level *table,
  uintptr_t virt,
  int level,
  uint64_t table_flags,
  bool ospace)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	struct rwlock_result res = rwlock_rlock(&table->lock, 0);
	table_realize(table, ospace);
	res = rwlock_upgrade(&res, 0);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags, ospace, &res);
	}
	rwlock_wunlock(&res);
}
