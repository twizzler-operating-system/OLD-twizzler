#include <lib/list.h>
#include <memory.h>

static _Atomic uintptr_t objspace_reservation = MEMORY_BOOTSTRAP_MAX;

static struct spinlock lock = SPINLOCK_INIT;
static DECLARE_LIST(region_list);

uintptr_t mm_objspace_reserve(size_t len)
{
	len = align_up(len, mm_objspace_region_size());
	uintptr_t ret = atomic_fetch_add(&objspace_reservation, len);
	return ret;
}

static struct objspace_region *new_objspace_region_struct(void)
{
	panic("A");
}

struct objspace_region *mm_objspace_allocate_region(void)
{
	spinlock_acquire_save(&lock);
	struct objspace_region *region;
	if(list_empty(&region_list)) {
		uintptr_t addr = atomic_fetch_add(&objspace_reservation, mm_objspace_region_size());
		if(addr >= arch_mm_objspace_max_address()) {
			spinlock_release_restore(&lock);
			return NULL;
		}
		region = new_objspace_region_struct();
		region->addr = addr;
	} else {
		region = list_entry(list_pop(&region_list), struct objspace_region, entry);
	}
	spinlock_release_restore(&lock);
	return region;
}

void mm_objspace_free_region(struct objspace_region *region)
{
	spinlock_acquire_save(&lock);
	list_insert(&region_list, &region->entry);
	spinlock_release_restore(&lock);
}

void mm_objspace_fill(uintptr_t addr, struct page *pages, size_t count, int flags)
{
	arch_objspace_map(NULL, addr, pages, count, flags);
}

struct omap *mm_objspace_get_object_map(struct object *obj, size_t page)
{
	panic("A");
}

struct omap *mm_objspace_lookup_omap_addr(uintptr_t addr)
{
	panic("A");
}
