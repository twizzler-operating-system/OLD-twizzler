#include <__mm_bits.h>
#include <kheap.h>
#include <memory.h>
#include <slab.h>

static void allocate_early_or_late(uintptr_t *phys,
  uintptr_t *oaddr,
  void **virt,
  struct kheap_run **_run)
{
	if(mm_is_ready()) {
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
}

static void table_level_ctor(void *_p, void *obj)
{
	struct table_level *tl = obj;
	tl->parent = NULL;
	tl->parent_idx = -1;
	tl->count = 0;
	tl->flags = TABLE_RUN_ALLOCATED;
	if(_p == (void *)1) {
		tl->flags |= TABLE_OSPACE;
	}

	if(tl->table) {
		memset(tl->table, 0, 0x1000);
	}
	if(tl->children) {
		for(int i = 0; i < 512; i++) {
			tl->children[i] = 0;
		}
	}
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

static DECLARE_SLABCACHE(sc_table_level_virt,
  sizeof(struct table_level),
  table_level_init,
  table_level_ctor,
  NULL,
  table_level_fini,
  NULL);

static DECLARE_SLABCACHE(sc_table_level_ospace,
  sizeof(struct table_level),
  table_level_init,
  table_level_ctor,
  NULL,
  table_level_fini,
  NULL);

struct table_level *table_level_new(bool ospace)
{
	/* do this read to avoid extra writes */
	if(bootstrap_table_levels_idx < NR_BOOTSTRAP_TABLES) {
		size_t idx = bootstrap_table_levels_idx++;
		if(idx < NR_BOOTSTRAP_TABLES) {
			struct table_level *tl = &bootstrap_table_levels[idx];
			tl->lock = RWLOCK_INIT;
			tl->flags = ospace ? TABLE_OSPACE : 0;
			return tl;
		}
	}
	if(ospace)
		return slabcache_alloc(&sc_table_level_ospace, (void *)1);
	return slabcache_alloc(&sc_table_level_virt, (void *)0);
}

void table_realize(struct table_level *table)
{
	if(table->children == NULL) {
		allocate_early_or_late(NULL, NULL, (void **)&table->children, &table->children_run);
	}
	if(table->table == NULL) {
		if(table->flags & TABLE_OSPACE)
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
		assert(table->table[idx] != 0);
		assert((table->table[idx] & table->children[idx]->phys) == table->children[idx]->phys);
		struct table_level *next = table->children[idx];
		assert((table->flags & TABLE_OSPACE) == (next->flags & TABLE_OSPACE));
		table_realize(next);
		return next;
	}
	assert(table->table[idx] == 0);
	struct table_level *nt = table_level_new(ospace);
	table_realize(nt);
	table->children[idx] = nt;
	table->table[idx] = nt->phys | flags;
	table->count++;
	nt->parent = table;
	nt->parent_idx = idx;
	assert((table->flags & TABLE_OSPACE) == (nt->flags & TABLE_OSPACE));

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
		if(table->flags & TABLE_OSPACE)
			slabcache_free(&sc_table_level_ospace, table, NULL);
		else
			slabcache_free(&sc_table_level_virt, table, NULL);
	} else {
		table_level_fini(NULL, table);
	}
}

void table_free_downward(struct table_level *table)
{
	for(int i = 0; i < 512; i++) {
		if(table->children[i]) {
			table_free_downward(table->children[i]);
			table->count--;
		} else if(table->table[i]) {
			table->table[i] = 0;
			table->count--;
		}
	}
	table_free_table_level(table);
}

static void table_remove_entry(struct table_level *table, size_t idx, bool free_tables)
{
	assert(table->count);
	if(table->children[idx]) {
		table_free_downward(table->children[idx]);
		table->children[idx] = NULL;
		table->table[idx] = 0;
		table->count--;
	} else if(table->table[idx]) {
		assert(table->table[idx] & PAGE_PRESENT);
		table->table[idx] = 0;
		table->count--;
	}

	if(table->count == 0 && table->parent && free_tables) {
		table_remove_entry(table->parent, table->parent_idx, free_tables);
		/* TODO: A */
		// table_free_table_level(table);
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

	struct rwlock_result res = rwlock_wlock(&table->lock, RWLOCK_RECURSE);
	table_realize(table);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags, ospace, NULL);
	}

	int idx = idxs[i];

	if(table->children[idx]) {
		table_free_downward(table->children[idx]);
		table->count--;
		table->children[idx] = NULL;
	}
	if(level != 0)
		flags |= PAGE_LARGE;
	if(!table->table[idx])
		table->count++;
	table->table[idx] = phys | flags;
	rwlock_wunlock(&res);
}

void table_unmap(struct table_level *table, uintptr_t virt, int flags)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };
	int i;
	struct rwlock_result res = rwlock_wlock(&table->lock, RWLOCK_RECURSE);
	for(i = 0; i < 4; i++) {
		if(!table->table) {
			rwlock_wunlock(&res);
			return;
		}
		if(table->children[idxs[i]])
			table = table->children[idxs[i]];
		else {
			if(table->table[idxs[i]]) {
				table_remove_entry(table, idxs[i], !(flags & MAP_TABLE_PREALLOC));
			}

			rwlock_wunlock(&res);
			return;
		}
	}
	rwlock_wunlock(&res);
}

bool table_readmap(struct table_level *table, uintptr_t virt, uint64_t *entry, int *level)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	struct rwlock_result res = rwlock_wlock(&table->lock, RWLOCK_RECURSE);
	int i;
	for(i = 0; i < 4; i++) {
		if(!table->table) {
			rwlock_wunlock(&res);
			return false;
		}
		if(table->children[idxs[i]])
			table = table->children[idxs[i]];
		else {
			if(entry)
				*entry = table->table[idxs[i]];
			if(level)
				*level = 3 - i;
			rwlock_wunlock(&res);
			return true;
		}
	}
	rwlock_wunlock(&res);
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

	struct rwlock_result res = rwlock_wlock(&table->lock, RWLOCK_RECURSE);
	table_realize(table);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags, ospace, NULL);
	}
	rwlock_wunlock(&res);
}

void table_print_recur(struct table_level *table, int level, int indent, uintptr_t off)
{
	struct rwlock_result res = rwlock_rlock(&table->lock, RWLOCK_RECURSE);
	if(!table->table) {
		rwlock_runlock(&res);
		return;
	}
	assert(level >= 0);
	for(int i = 0; i < 512; i++) {
		uintptr_t co = off + ((level < 3) ? mm_page_size(level) : mm_page_size(2) * 512) * i;
		size_t len = level < 3 ? mm_page_size(level) : mm_page_size(2) * 512;
		if(table->table[i]) {
			printk("%*s%lx - %lx", indent, "", co, co + len);
			printk(":: %lx\n", table->table[i]);
		}

		if(table->children && table->children[i]) {
			assert((table->table[i] & PAGE_LARGE) == 0);
			assert(table->table[i]);
			assert((table->table[i] & table->children[i]->phys) == table->children[i]->phys);
			table_print_recur(table->children[i], level - 1, indent + 2, co);
		} else {
			assert(((table->table[i] & PAGE_LARGE) != 0) || level == 0 || table->table[i] == 0);
		}
	}
	rwlock_runlock(&res);
}
