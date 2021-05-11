#include <memory.h>

static void allocate_early_or_late(uintptr_t *phys, void **virt, struct kheap_run **_run)
{
	if(mm_ready) {
		struct kheap_run *run = kheap_allocate(0x1000);
		if(phys)
			*phys = kheap_run_get_objspace(run);
		*_run = run;
		*virt = run->start;
	} else {
		mm_early_alloc(phys, virt, 0x1000, 0x1000);
	}
}

#define NR_BOOTSTRAP_TABLES 64

static struct table_level bootstrap_table_levels[NR_BOOTSTRAP_TABLES];
static _Atomic size_t bootstrap_table_levels_idx = 0;

static struct table_level *allocate_table_level(void)
{
	/* do this read to avoid extra writes */
	if(bootstrap_table_levels_idx < NR_BOOTSTRAP_TABLES) {
		size_t idx = bootstrap_table_levels_idx++;
		if(idx < NR_BOOTSTRAP_TABLES) {
			return &bootstrap_table_levels[idx];
		}
	}
	panic("A");
}

void table_realize(struct table_level *table)
{
	if(table->children == NULL) {
		allocate_early_or_late(NULL, (void **)&table->children, &table->children_run);
	}
	if(table->table == NULL) {
		allocate_early_or_late(&table->phys, (void **)&table->table, &table->table_run);
	}
}

struct table_level *table_get_next_level(struct table_level *table, int idx, uint64_t flags)
{
	if(table->children[idx]) {
		struct table_level *next = table->children[idx];
		table_realize(next);
		return next;
	}
	struct table_level *nt = allocate_table_level();
	table_realize(nt);
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
	panic("A"); // TODO actually free
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
  uint64_t table_flags)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	table_realize(table);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags);
	}

	int idx = idxs[i];

	if(table->children[idx]) {
		table_free_downward(table->children[idx]);
		table->count--;
		table->children[idx] = NULL;
	}
	table->table[idx] = phys | flags | ((level == 0) ? 0 : PAGE_LARGE) | PAGE_PRESENT;
	table->count++;
}

void table_premap(struct table_level *table, uintptr_t virt, int level, uint64_t table_flags)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	int idxs[] = { pml4_idx, pdpt_idx, pd_idx, pt_idx };

	table_realize(table);

	int i;
	for(i = 0; i < (3 - level); i++) {
		table = table_get_next_level(table, idxs[i], table_flags);
	}
}
