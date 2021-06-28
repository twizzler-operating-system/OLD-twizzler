#pragma once
#include <arch/memory.h>
#include <lib/list.h>
#include <machine/memory.h>
#include <spinlock.h>
#include <string.h>

#define VADDR_IS_KERNEL(x) ({ (x) >= KERNEL_REGION_START; })
#define VADDR_IS_USER(x) ({ (x) >= USER_REGION_START && (x) < USER_REGION_END; })
#define VADDR_IS_CANON(x) ({ VADDR_IS_KERNEL(x) || VADDR_IS_USER(x) })

enum memory_type {
	MEMORY_UNKNOWN,
	MEMORY_AVAILABLE,
	MEMORY_RESERVED,
	MEMORY_RECLAIMABLE,
	MEMORY_BAD,
	MEMORY_CODE,
	MEMORY_KERNEL_IMAGE,
};

enum memory_subtype {
	MEMORY_SUBTYPE_NONE,
	MEMORY_AVAILABLE_VOLATILE,
	MEMORY_AVAILABLE_PERSISTENT,
};

struct memregion {
	uintptr_t start;
	size_t length;
	enum memory_type type;
	enum memory_subtype subtype;
	struct list entry;
	struct spinlock lock;
};

void mm_init_phase_2(void);
void arch_mm_init(void);
bool mm_is_ready(void);

void mm_register_region(struct memregion *reg);
void mm_init_region(struct memregion *reg,
  uintptr_t start,
  size_t len,
  enum memory_type type,
  enum memory_subtype);

#define REGION_ALLOC_BOOTSTRAP 1
uintptr_t mm_region_alloc_raw(size_t len, size_t align, int flags);
void mm_early_alloc(uintptr_t *phys, void **virt, size_t len, size_t align);
void *mm_early_ptov(uintptr_t phys);

void mm_update_stats(void);
struct page_stats;
int page_build_stats(struct page_stats *stats, int idx);
void mm_print_stats(void);
void mm_print_kalloc_stats(void);
