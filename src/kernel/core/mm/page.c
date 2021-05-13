#include <memory.h>
#include <page.h>

/* TODO: percpu? finer locking? */

static DECLARE_LIST(page_list);
static DECLARE_LIST(pagezero_list);
static DECLARE_LIST(pagestruct_list);
static struct spinlock lock = SPINLOCK_INIT;

#define INITIAL_ALLOC_SIZE 4096
#define INITIAL_NUM_PAGES ({ INITIAL_ALLOC_SIZE / sizeof(struct page); })

static struct page *allpages;
static _Atomic size_t next_page = 0;
static _Atomic size_t max_pages = 0;

_Atomic uint64_t mm_page_alloc_count = 0;

void mm_page_init(void)
{
	uintptr_t oaddr = mm_objspace_reserve(INITIAL_NUM_PAGES * sizeof(struct page));
	mm_map(KERNEL_VIRTUAL_PAGES_BASE,
	  oaddr,
	  INITIAL_NUM_PAGES * sizeof(struct page),
	  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_TABLE_PREALLOC | MAP_READ | MAP_WRITE);
	allpages = (void *)KERNEL_VIRTUAL_PAGES_BASE;
	max_pages = INITIAL_NUM_PAGES;
	struct page pg[INITIAL_NUM_PAGES];
	for(size_t i = 0; i < INITIAL_NUM_PAGES; i++) {
		pg[i].addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
	}
	mm_objspace_fill(
	  oaddr, pg, INITIAL_NUM_PAGES, MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
}

static struct page *get_new_page_struct(void)
{
	if(next_page == max_pages) {
		assert(align_up((uintptr_t)&allpages[max_pages], mm_page_size(0))
		       == (uintptr_t)&allpages[max_pages]);
		uintptr_t oaddr = mm_objspace_reserve(INITIAL_NUM_PAGES * sizeof(struct page));
		mm_map((uintptr_t)&allpages[max_pages],
		  oaddr,
		  INITIAL_NUM_PAGES * sizeof(struct page),
		  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_TABLE_PREALLOC | MAP_READ | MAP_WRITE);
		max_pages += INITIAL_NUM_PAGES;
		struct page pg[INITIAL_NUM_PAGES];
		for(size_t i = 0; i < INITIAL_NUM_PAGES; i++) {
			pg[i].addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
		}
		mm_objspace_fill(
		  oaddr, pg, INITIAL_NUM_PAGES, MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
	}

	return &allpages[next_page++];
}

static struct page *fallback_page_alloc(void)
{
	uintptr_t addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
	struct page *page = get_new_page_struct();
	page->flags = PAGE_CACHE_WB;
	page->addr = addr;
	return page;
}

static void mm_page_zero_addr(uintptr_t addr)
{
	/* we might get lucky here! */
	if(addr < MEMORY_BOOTSTRAP_MAX) {
		memset(mm_early_ptov(addr), 0, mm_page_size(0));
		return;
	}
	struct page page = {
		.addr = addr,
		.flags = 0,
	};
	void *vaddr = tmpmap_map_pages(&page, 1);
	memset(vaddr, 0, mm_page_size(0));
}

void mm_page_zero(struct page *page)
{
	mm_page_zero_addr(page->addr);
	page->flags |= PAGE_ZERO;
}

#define PAGE_ADDR 0x1000

static inline struct page *RET(int flags, struct page *p)
{
	if(flags & PAGE_ADDR) {
		uintptr_t addr = p->addr;
		p->addr = 0;
		p->flags = 0;
		list_insert(&pagestruct_list, &p->entry);
		return (void *)addr;
	}
	return p;
}

static struct page *__do_mm_page_alloc(int flags)
{
	/* if we're requesting a zero'd page, try getting from the zero list first */
	if(flags & PAGE_ZERO) {
		if(!list_empty(&pagezero_list)) {
			struct page *ret = list_entry(list_pop(&pagezero_list), struct page, entry);
			return RET(flags, ret);
		}
	}

	struct page *page;
	/* prefer getting from non-zero list first. If we wanted a zero'd page and there was one
	 * available, we wouldn't be here. */
	if(list_empty(&page_list)) {
		if(list_empty(&pagezero_list)) {
			if(flags & PAGE_ADDR) {
				return (void *)mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
			}
			page = fallback_page_alloc();
		} else {
			page = list_entry(list_pop(&pagezero_list), struct page, entry);
		}
	} else {
		page = list_entry(list_pop(&page_list), struct page, entry);
	}
	assert(page);

	return RET(flags, page);
}

struct page *mm_page_alloc(int flags)
{
	spinlock_acquire_save(&lock);
	struct page *page = __do_mm_page_alloc(flags);
	spinlock_release_restore(&lock);
	if((flags & PAGE_ZERO) && !(page->flags & PAGE_ZERO)) {
		mm_page_zero(page);
	}
	return page;
}

uintptr_t mm_page_alloc_addr(int flags)
{
	void *p = __do_mm_page_alloc(flags | PAGE_ADDR);
	if(flags & PAGE_ZERO) {
		mm_page_zero_addr((uintptr_t)p);
	}
	return (uintptr_t)p;
}

void mm_page_print_stats(void)
{
	printk("TODO: page stats\n");
}

void mm_page_idle_zero(void)
{
	printk("TODO: page idle\n");
}
