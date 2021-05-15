#pragma once

#include <debug.h>
#include <machine/memory.h>
#include <rwlock.h>

#define TABLE_RUN_ALLOCATED 1
struct kheap_run;
struct table_level {
	uintptr_t phys;  // physical address of this table
	uint64_t *table; // virtual address of this table (physical addresses of subtables or PTEs)
	struct table_level *parent;
	struct table_level **children; // pointers to children (virtuals)
	struct kheap_run *children_run;
	struct kheap_run *table_run;
	size_t count; // number of children
	struct rwlock lock;
	int parent_idx;
	int flags;
};

struct arch_vm_context {
	struct table_level root;
	int id;
};

/* Intel 3A 4.5 */
#define VM_MAP_USER (1ull << 2)
#define VM_MAP_WRITE (1ull << 1)
#define VM_MAP_ACCESSED (1ull << 5)
#define VM_MAP_DIRTY (1ull << 6)
#define VM_MAP_GLOBAL (1ull << 8)
#define VM_MAP_DEVICE (1ull << 4)
/* x86 has a no-execute bit, so we'll need to fix this up in the mapping functions */
#define VM_MAP_EXEC (1ull << 63)
#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE (1ull << 7)

#define VM_PHYS_MASK (0x7FFFFFFFFFFFF000)
#define VM_ADDR_SIZE (1ul << 48)
#define MAX_PGLEVEL 2

__attribute__((const)) static inline size_t mm_page_size(const int level)
{
	assert(level < 3);
	static const size_t __pagesizes[3] = { 0x1000, 2 * 1024 * 1024, 1024 * 1024 * 1024 };
	return __pagesizes[level];
}

__attribute__((const)) static inline int mm_page_max_level(size_t sz)
{
	if(sz >= (1024 * 1024 * 1024))
		return 2;
	if(sz >= 2 * 1024 * 1024)
		return 1;
	assert(sz >= 0x1000);
	return 0;
}

#define OM_ADDR_SIZE (1ul << 48)
void table_map(struct table_level *root,
  uintptr_t virt,
  uintptr_t phys,
  int level,
  uint64_t flags,
  uint64_t,
  bool);
void table_realize(struct table_level *table, bool);
struct table_level *table_get_next_level(struct table_level *table,
  int idx,
  uint64_t,
  bool,
  struct rwlock_result *);
void table_premap(struct table_level *table, uintptr_t virt, int level, uint64_t table_flags, bool);
bool table_readmap(struct table_level *table, uintptr_t addr, uint64_t *entry, int *level);
void table_unmap(struct table_level *table, uintptr_t virt, int flags);

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v) (((v) >> 21) & 0x1FF)
#define PT_IDX(v) (((v) >> 12) & 0x1FF)
