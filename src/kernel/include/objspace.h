#pragma once

#include <__mm_bits.h>
#include <arch/objspace.h>
#include <krc.h>
#include <lib/rb.h>

struct objspace_region {
	struct arch_objspace_region arch;
	uintptr_t addr;
	struct list entry;
};

struct omap {
	struct object *obj;
	struct objspace_region *region;
	size_t regnr;
	_Atomic size_t refs;
	struct rbnode objnode;
};

struct page;

struct omap *mm_objspace_get_object_map(struct object *obj, size_t page);
void omap_free(struct omap *omap);
int omap_compar(struct omap *a, struct omap *b);
int omap_compar_key(struct omap *v, size_t slot);

struct objspace_region *mm_objspace_allocate_region(void);
void mm_objspace_free_region(struct objspace_region *region);
void arch_objspace_region_cow(struct objspace_region *region, size_t start, size_t len);
void arch_objspace_region_unmap(struct objspace_region *region, size_t start, size_t len);
void arch_objspace_region_init(struct objspace_region *region);
bool arch_objspace_region_map_page(struct objspace_region *,
  size_t idx,
  struct page *page,
  uint64_t flags);

struct object_space {
	struct arch_object_space arch;
	struct krc refs;
};

#define mm_objspace_region_size arch_mm_objspace_region_size
__attribute__((const, pure)) uintptr_t arch_mm_objspace_max_address(void);
__attribute__((const, pure)) size_t arch_mm_objspace_region_size(void);
__attribute__((const, pure)) uintptr_t arch_mm_objspace_kernel_size(void);

void arch_object_space_fini(struct object_space *space);
void arch_object_space_init(struct object_space *space);
void object_space_free(struct object_space *space);
struct object_space *object_space_alloc(void);
void arch_objspace_print_mapping(struct object_space *space, uintptr_t virt);
void mm_objspace_kernel_fill(uintptr_t addr, struct page *pages[], size_t count, int flags);
void arch_objspace_region_map(struct object_space *, struct objspace_region *region, uint64_t);
uintptr_t mm_objspace_kernel_reserve(size_t len);
uintptr_t mm_objspace_get_phys(struct object_space *, uintptr_t oaddr);
uintptr_t arch_mm_objspace_get_phys(struct object_space *, uintptr_t oaddr);
#define INVL_SELF 0
#define INVL_ALL 1
void arch_mm_objspace_invalidate(struct object_space *, uintptr_t start, size_t len, int flags);
void mm_objspace_kernel_unmap(uintptr_t addr, size_t nrpages, int flags);
void arch_objspace_unmap(struct object_space *, uintptr_t addr, size_t nrpages, int flags);
void arch_objspace_map(struct object_space *space,
  uintptr_t virt,
  struct page *pages[],
  size_t,
  uint64_t flags);
