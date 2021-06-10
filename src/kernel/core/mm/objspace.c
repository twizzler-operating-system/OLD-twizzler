#include <lib/list.h>
#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <slab.h>

static _Atomic uintptr_t objspace_kernel_reservation = MEMORY_BOOTSTRAP_MAX;

static _Atomic uintptr_t objspace_user_reservation;

static struct objspace_region *region_page = NULL;
static size_t region_page_idx;

static struct spinlock lock = SPINLOCK_INIT;
static DECLARE_LIST(region_list);

static struct rbroot slotroot = RBINIT;

uintptr_t mm_objspace_kernel_reserve(size_t len)
{
	/* TODO: remove this align */
	len = align_up(len, mm_objspace_region_size());
	uintptr_t ret = atomic_fetch_add(&objspace_kernel_reservation, len);
	if(ret >= arch_mm_objspace_kernel_size()) {
		panic("out of kernel object space memory");
	}
	return ret;
}

static struct objspace_region *new_objspace_region_struct(void)
{
	if(region_page_idx >= mm_page_size(0) / sizeof(struct objspace_region) || region_page == NULL) {
		region_page_idx = 0;
		struct kheap_run *run = kheap_allocate(mm_page_size(0));
		region_page = run->start;
	}

	return &region_page[region_page_idx++];
}

struct objspace_region *mm_objspace_allocate_region(void)
{
	spinlock_acquire_save(&lock);
	struct objspace_region *region;
	if(list_empty(&region_list)) {
		if(objspace_user_reservation == 0) {
			objspace_user_reservation = arch_mm_objspace_kernel_size();
		}
		uintptr_t addr = atomic_fetch_add(&objspace_user_reservation, mm_objspace_region_size());
		if(addr >= arch_mm_objspace_max_address()) {
			spinlock_release_restore(&lock);
			return NULL;
		}
		region = new_objspace_region_struct();
		arch_objspace_region_init(region);
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

static void omap_init(void *p __unused, void *obj)
{
	struct omap *om = obj;
	om->region = mm_objspace_allocate_region();
}

static void omap_fini(void *p __unused, void *obj)
{
	struct omap *om = obj;
	mm_objspace_free_region(om->region);
}

static DECLARE_SLABCACHE(sc_omap, sizeof(struct omap), omap_init, NULL, NULL, omap_fini, NULL);

#include <processor.h>
#include <secctx.h>
void mm_objspace_kernel_fill(uintptr_t addr, struct page *pages[], size_t count, int flags)
{
	arch_objspace_map(NULL, addr, pages, count, flags);
}

void mm_objspace_kernel_unmap(uintptr_t addr, size_t nrpages, int flags)
{
	arch_objspace_unmap(NULL, addr, nrpages, flags);
}

int omap_compar_key(struct omap *v, size_t slot)
{
	if(v->regnr > slot)
		return 1;
	if(v->regnr < slot)
		return -1;
	return 0;
}

int omap_compar(struct omap *a, struct omap *b)
{
	return omap_compar_key(a, b->regnr);
}

struct omap *mm_objspace_get_object_map(struct object *obj, size_t page)
{
	size_t regnr = page / (mm_objspace_region_size() / mm_page_size(0));
	spinlock_acquire_save(&obj->lock);

	struct rbnode *node = rb_search(&obj->omap_root, regnr, struct omap, objnode, omap_compar_key);

	if(node) {
		struct omap *omap = rb_entry(node, struct omap, objnode);
		omap->refs++;
		spinlock_release_restore(&obj->lock);
		return omap;
	}

	struct omap *omap = slabcache_alloc(&sc_omap, NULL);
	omap->obj = obj;
	omap->refs = 1;
	omap->regnr = regnr;

	rb_insert(&obj->omap_root, omap, struct omap, objnode, omap_compar);
	spinlock_release_restore(&obj->lock);
	/* TODO: fix race with unmapping */
	return omap;
}

struct omap *mm_objspace_lookup_omap_addr(uintptr_t addr)
{
	panic("A");
}

uintptr_t mm_objspace_get_phys(struct object_space *space, uintptr_t oaddr)
{
	return arch_mm_objspace_get_phys(space, oaddr);
}

static void object_space_init(void *data __unused, void *ptr)
{
	struct object_space *space = ptr;
	arch_object_space_init(space);
}

static void object_space_fini(void *data __unused, void *ptr)
{
	struct object_space *space = ptr;
	arch_object_space_fini(space);
}

static DECLARE_SLABCACHE(sc_objspace,
  sizeof(struct object_space),
  object_space_init,
  NULL,
  NULL,
  object_space_fini,
  NULL);

struct object_space *object_space_alloc(void)
{
	return slabcache_alloc(&sc_objspace, NULL);
}

void object_space_free(struct object_space *space)
{
	printk("TODO: A free objspaces\n");
	// slabcache_free(&sc_objspace, space);
}
