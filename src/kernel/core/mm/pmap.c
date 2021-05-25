#include <memory.h>
#include <object.h>
#include <pmap.h>
#include <slots.h>
#include <spinlock.h>
#include <twz/sys/dev/memory.h>

/* TODO: put this in header */
extern struct vm_context kernel_ctx;

struct pmap {
	uintptr_t phys;
	uintptr_t virt;
	uintptr_t oaddr;
	struct rbnode node;
	struct page page;
};

static struct spinlock lock = SPINLOCK_INIT;

#define PMAP_MAX 1024 * 1024 * 1024ul

static uintptr_t start = KERNEL_VIRTUAL_PMAP_BASE;
static uintptr_t oaddr_start = 0;
static struct rbroot root = RBINIT;

void pmap_collect_stats(struct memory_stats *stats)
{
	stats->pmap_used = start;
}

static int __pmap_compar_key(struct pmap *a, uintptr_t n)
{
	if((a->phys | a->page.flags) > n)
		return 1;
	else if((a->phys | a->page.flags) < n)
		return -1;
	return 0;
}

static int __pmap_compar(struct pmap *a, struct pmap *b)
{
	assert((b->phys & b->page.flags) == 0);
	return __pmap_compar_key(a, b->phys | b->page.flags);
}

static struct pmap *pmap_get(uintptr_t phys, int cache_type, bool remap)
{
	if(oaddr_start == 0) {
		oaddr_start = mm_objspace_reserve(PMAP_MAX);
	}
	struct pmap *pmap;
	struct rbnode *node = rb_search(&root, phys, struct pmap, node, __pmap_compar_key);
	if(!node) {
		pmap = kalloc(sizeof(struct pmap), 0);
		pmap->phys = phys;
		pmap->virt = start;
		start += mm_page_size(0);
		rb_insert(&root, pmap, struct pmap, node, __pmap_compar);

		uintptr_t oaddr = oaddr_start;
		oaddr_start += mm_page_size(0);
		mm_map(pmap->virt,
		  oaddr,
		  mm_page_size(0),
		  MAP_GLOBAL | MAP_KERNEL | MAP_WRITE | MAP_REPLACE | MAP_READ);
		pmap->oaddr = oaddr;
		struct page page = {
			.addr = phys,
			.flags = cache_type,
		};
		struct page *pages[] = { &page };
		printk("TODO: dont ignore PAT type??\n");
		mm_objspace_fill(
		  oaddr, pages, 1, MAP_GLOBAL | MAP_KERNEL | MAP_WRITE | MAP_REPLACE | MAP_READ);
	} else {
		pmap = rb_entry(node, struct pmap, node);
		if(remap) {
			uintptr_t oaddr = oaddr_start;
			oaddr_start += mm_page_size(0);
			mm_map(pmap->virt,
			  oaddr,
			  mm_page_size(0),
			  MAP_GLOBAL | MAP_KERNEL | MAP_WRITE | MAP_REPLACE | MAP_READ);
			pmap->oaddr = oaddr;
			struct page page = {
				.addr = phys,
				.flags = cache_type,
			};
			struct page *pages[] = { &page };
			mm_objspace_fill(
			  oaddr, pages, 1, MAP_GLOBAL | MAP_KERNEL | MAP_WRITE | MAP_REPLACE | MAP_READ);
			pmap->virt = start;
			start += mm_page_size(0);
		}
	}

	return pmap;
}

void *pmap_allocate(uintptr_t phys, size_t len, int cache_type)
{
	size_t off = phys % mm_page_size(0);
	len += phys % mm_page_size(0);
	phys = align_down(phys, mm_page_size(0));

	spinlock_acquire_save(&lock);
	uintptr_t virt = 0;
	uintptr_t oaddr = 0;
	for(size_t i = 0; i < len; i += mm_page_size(0)) {
		struct pmap *pmap = pmap_get(phys + i, cache_type, len > mm_page_size(0));
		assert(pmap != NULL);
		if(!virt)
			virt = pmap->virt;
		if(!oaddr)
			oaddr = pmap->oaddr;
	}
	spinlock_release_restore(&lock);
#if 0
	printk("[mm] pmap mapping [%lx - %lx] -> [%lx - %lx] -> [%lx - %lx]\n",
	  virt,
	  virt + len,
	  oaddr,
	  oaddr + len,
	  phys,
	  phys + len);
#endif
	return (void *)(virt + off);
}
