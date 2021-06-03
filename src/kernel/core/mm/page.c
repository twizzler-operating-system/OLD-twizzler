#include <memory.h>
#include <page.h>
#include <stdatomic.h>

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
	uintptr_t oaddr = mm_objspace_kernel_reserve(INITIAL_NUM_PAGES * sizeof(struct page));
	mm_map(KERNEL_VIRTUAL_PAGES_BASE,
	  oaddr,
	  INITIAL_NUM_PAGES * sizeof(struct page),
	  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_TABLE_PREALLOC | MAP_READ | MAP_WRITE);
	allpages = (void *)KERNEL_VIRTUAL_PAGES_BASE;
	max_pages = INITIAL_NUM_PAGES;
	struct page pg[INITIAL_NUM_PAGES];
	struct page *pages[INITIAL_NUM_PAGES];
	for(size_t i = 0; i < INITIAL_NUM_PAGES; i++) {
		pg[i].addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
		pages[i] = &pg[i];
	}
	mm_objspace_kernel_fill(
	  oaddr, pages, INITIAL_NUM_PAGES, MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
}

static struct page *get_new_page_struct(void)
{
	if(!list_empty(&pagestruct_list)) {
		return list_entry(list_pop(&pagestruct_list), struct page, entry);
	}
	if(next_page == max_pages) {
		printk("new page page\n");
		assert(align_up((uintptr_t)&allpages[max_pages], mm_page_size(0))
		       == (uintptr_t)&allpages[max_pages]);
		uintptr_t oaddr = mm_objspace_kernel_reserve(INITIAL_NUM_PAGES * sizeof(struct page));
		mm_map((uintptr_t)&allpages[max_pages],
		  oaddr,
		  INITIAL_NUM_PAGES * sizeof(struct page),
		  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_TABLE_PREALLOC | MAP_READ | MAP_WRITE);
		max_pages += INITIAL_NUM_PAGES;
		struct page pg[INITIAL_NUM_PAGES];
		struct page *pages[INITIAL_NUM_PAGES];
		for(size_t i = 0; i < INITIAL_NUM_PAGES; i++) {
			pg[i].addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
			pages[i] = &pg[i];
		}
		spinlock_release_restore(&lock);
		mm_objspace_kernel_fill(oaddr,
		  pages,
		  INITIAL_NUM_PAGES,
		  MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
		spinlock_acquire_save(&lock);
		printk("done: new page page (%p %lx)\n", &allpages[next_page], oaddr);
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

struct page *mm_page_fake_create(uintptr_t phys, int flags)
{
	if(phys > 0xffff000000000000ul) {
		panic("A what2");
	}
	spinlock_acquire_save(&lock);
	struct page *page = get_new_page_struct();
	spinlock_release_restore(&lock);
	page->addr = phys;
	page->flags = flags | PAGE_FAKE;
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
	struct page *pages[] = { &page };
	void *vaddr = tmpmap_map_pages(pages, 1);
	memset(vaddr, 0, mm_page_size(0));
}

void mm_page_zero(struct page *page)
{
	mm_page_zero_addr(page->addr);
	page->flags |= PAGE_ZERO;
}

void mm_page_write(struct page *page, void *data, size_t len)
{
	if(len > mm_page_size(0))
		len = mm_page_size(0);
	if(unlikely(len == 0))
		return;
	if(page->addr < MEMORY_BOOTSTRAP_MAX) {
		memcpy(mm_early_ptov(page->addr), 0, len);
		return;
	}
	struct page *pages[] = { page };
	void *vaddr = tmpmap_map_pages(pages, 1);
	memcpy(vaddr, data, len);
}

struct page *mm_page_clone(struct page *page)
{
	struct page *newpage = mm_page_alloc(0);
	void *srcaddr = NULL;
	void *dstaddr = NULL;
	if(page->addr < MEMORY_BOOTSTRAP_MAX) {
		srcaddr = mm_early_ptov(page->addr);
	}
	if(newpage->addr < MEMORY_BOOTSTRAP_MAX) {
		dstaddr = mm_early_ptov(newpage->addr);
	}

	if(!srcaddr && !dstaddr) {
		struct page *pages[] = { page, newpage };
		void *addr = tmpmap_map_pages(pages, 2);
		srcaddr = addr;
		dstaddr = (char *)addr + mm_page_size(0);
	} else if(!srcaddr) {
		struct page *pages[] = { page };
		void *addr = tmpmap_map_pages(pages, 1);
		srcaddr = addr;
	} else if(!dstaddr) {
		struct page *pages[] = { newpage };
		void *addr = tmpmap_map_pages(pages, 1);
		dstaddr = addr;
	}

	memcpy(dstaddr, srcaddr, mm_page_size(0));
	atomic_thread_fence(memory_order_seq_cst);
	return newpage;
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
				printk("A\n");
				return (void *)mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
			}
			printk("B\n");
			page = fallback_page_alloc();
		} else {
			printk("C\n");
			page = list_entry(list_pop(&pagezero_list), struct page, entry);
		}
	} else {
		printk("D\n");
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
	if(page->addr > 0xffff000000000000ul) {
		panic("A what");
	}
	return page;
}

uintptr_t mm_page_alloc_addr(int flags)
{
	spinlock_acquire_save(&lock);
	void *p = __do_mm_page_alloc(flags | PAGE_ADDR);
	spinlock_release_restore(&lock);
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
	/* TODO A */
}
