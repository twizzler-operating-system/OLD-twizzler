#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <pmap.h>
#include <processor.h>
#include <spinlock.h>
#include <tmpmap.h>
#include <twz/sys/dev/memory.h>
#include <vmm.h>

/* tmpmap -- support mapping any physical page into a virtual address so that we can access it.
 * While _some_ physical memory is identity mapped in the system, not all of it is because of the
 * limitations of the object space. On systems where object space is not limited by physical address
 * width of the CPU, perhaps most tmpmap operations can be avoided. That optimization will be
 * handled in the page system (mm/page.c).
 *
 * To reduce the number of invalidations, we reserve a reasonably large space for tmpmaps per cpu.
 * Thus we never have to worry about cross-CPU invalidations at the expense of these temporary
 * mappings being ephemeral and not sharable (which is fine, we have other mechanisms for that, see
 * mm/pmap.c). Additionally, we consider the reserved region like a ring-buffer, trying to map pages
 * to new addresses until we run out. When we run out of space, we unmap everything and invalidate.
 * This means we don't have to flush the TLB everytime.
 *
 * Because we have a reserved max amount of space, we can't map more than TMPMAP_MAX_PAGES pages at
 * once.
 *
 * To reduce overhead and bookkeeping, tmpmaps are not kept track of and previous maps can be
 * potentially invalidated whenever tmpmap_map_pages is called. This means: 1) Any time this
 * function is called, prior maps cannot be used. If you need to map multiple pages simultaneously,
 * you must do it in one call to the function. 2) tmpmap_map_pages may not be called in interrupt
 * context.
 */

#define TMPMAP_LEN mm_page_size(0) * TMPMAP_MAX_PAGES

static _Atomic uintptr_t tmpmap_start = KERNEL_VIRTUAL_TMPMAP_BASE;

struct tmpmap {
	uintptr_t virt;
	uintptr_t oaddr;
	size_t current;
};

static DECLARE_PER_CPU(struct tmpmap, tmpmap) = {};

void tmpmap_collect_stats(struct memory_stats *stats)
{
}

static void tmpmap_init(struct tmpmap *tmpmap)
{
	if(tmpmap->virt == 0) {
		tmpmap->virt = atomic_fetch_add(&tmpmap_start, TMPMAP_LEN);
		tmpmap->oaddr = mm_objspace_kernel_reserve(TMPMAP_LEN);
		printk("mapping tmpmap: %lx -> %lx\n", tmpmap->virt, tmpmap->oaddr);
		mm_map(tmpmap->virt,
		  tmpmap->oaddr,
		  TMPMAP_LEN,
		  MAP_READ | MAP_WRITE | MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_REPLACE);
		mm_objspace_kernel_fill(tmpmap->oaddr,
		  NULL,
		  TMPMAP_LEN / mm_page_size(0),
		  MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_GLOBAL | MAP_WIRE | MAP_REPLACE
		    | MAP_TABLE_PREALLOC);
		tmpmap->current = 0;
	}
}

static void tmpmap_reset(struct tmpmap *tmpmap)
{
	mm_objspace_kernel_unmap(tmpmap->oaddr, TMPMAP_LEN / mm_page_size(0), MAP_TABLE_PREALLOC);
	tmpmap->current = 0;
	arch_mm_objspace_invalidate(NULL, tmpmap->oaddr, TMPMAP_LEN, INVL_SELF);
}

void *tmpmap_map_pages(struct page *pages[], size_t count)
{
	/* TODO: if we are mapping only a single page, we can do the "is bootstrap" possible
	 * optimization */
	struct tmpmap *tmpmap = per_cpu_get(tmpmap);
	tmpmap_init(tmpmap);

	const size_t max = TMPMAP_LEN / mm_page_size(0);
	if(tmpmap->current + count > max) {
		tmpmap_reset(tmpmap);
	}

	if(count > max) {
		panic("cannot tmpmap %ld pages at the same time", count);
	}

	mm_objspace_kernel_fill(tmpmap->oaddr + tmpmap->current * mm_page_size(0),
	  pages,
	  count,
	  MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_GLOBAL | MAP_WIRE | MAP_REPLACE);

	void *ret = (void *)(tmpmap->virt + tmpmap->current * mm_page_size(0));
	tmpmap->current += count;
	return ret;
}
