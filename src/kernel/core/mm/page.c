/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <memory.h>
#include <objspace.h>
#include <page.h>
#include <stdatomic.h>
#include <tmpmap.h>
#include <vmm.h>

/* TODO: percpu? finer locking? */

static struct page *page_list = NULL;
static struct page *pagezero_list = NULL;
static struct page *pagestruct_list = NULL;

static struct spinlock lock = SPINLOCK_INIT;

#define INITIAL_ALLOC_SIZE 4096
#define INITIAL_NUM_PAGE_STRUCTS ({ INITIAL_ALLOC_SIZE / sizeof(struct page); })
#define INITIAL_ALLOC_PAGES ({ align_up(INITIAL_ALLOC_SIZE, mm_page_size(0)) / mm_page_size(0); })

static struct page *allpages;
static _Atomic size_t next_page = 0;
static _Atomic size_t max_pages = 0;

_Atomic uint64_t mm_page_alloc_count = 0;

static struct page *pagelist_pop(struct page **list)
{
	if(*list) {
		struct page *page = *list;
		*list = page->next;
		return page;
	}
	return NULL;
}

static void pagelist_add(struct page **list, struct page *p)
{
	p->next = *list;
	*list = p;
}

static bool pagelist_empty(struct page **list)
{
	return *list == NULL;
}

static void page_make_addr(struct page *p, uintptr_t addr, uint64_t flags)
{
	assert((addr & ~MM_PAGE_ADDR_MASK) == 0);
	assert((flags & MM_PAGE_ADDR_MASK) == 0);
	p->__addr_and_flags = addr | flags;
}

static void page_set_flags(struct page *p, uint64_t flags)
{
	assert((flags & MM_PAGE_ADDR_MASK) == 0);
	p->__addr_and_flags |= flags;
}

static void page_clear_flags(struct page *p, uint64_t mask)
{
	assert((mask & MM_PAGE_ADDR_MASK) == 0);
	p->__addr_and_flags &= ~mask;
}

void mm_page_init(void)
{
	uintptr_t oaddr = mm_objspace_kernel_reserve(INITIAL_ALLOC_SIZE);
	mm_map(KERNEL_VIRTUAL_PAGES_BASE,
	  oaddr,
	  INITIAL_ALLOC_SIZE,
	  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_READ | MAP_WRITE);
	allpages = (void *)KERNEL_VIRTUAL_PAGES_BASE;
	max_pages = INITIAL_NUM_PAGE_STRUCTS;
	struct page pg[INITIAL_ALLOC_PAGES];
	struct page *pages[INITIAL_ALLOC_PAGES];
	for(size_t i = 0; i < INITIAL_ALLOC_PAGES; i++) {
		page_make_addr(
		  &pg[i], mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0), PAGE_CACHE_WB);
		pages[i] = &pg[i];
	}
	mm_objspace_kernel_fill(
	  oaddr, pages, INITIAL_ALLOC_PAGES, MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
}

static struct page *get_new_page_struct(void)
{
	if(!pagelist_empty(&pagestruct_list)) {
		return pagelist_pop(&pagestruct_list);
	}
	if(next_page == max_pages) {
		assert(align_up((uintptr_t)&allpages[max_pages], mm_page_size(0))
		       == (uintptr_t)&allpages[max_pages]);
		uintptr_t oaddr = mm_objspace_kernel_reserve(INITIAL_ALLOC_SIZE);
		mm_map((uintptr_t)&allpages[max_pages],
		  oaddr,
		  INITIAL_ALLOC_SIZE,
		  MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_READ | MAP_WRITE);
		max_pages += INITIAL_NUM_PAGE_STRUCTS;
		struct page pg[INITIAL_ALLOC_PAGES];
		struct page *pages[INITIAL_ALLOC_PAGES];
		for(size_t i = 0; i < INITIAL_ALLOC_PAGES; i++) {
			page_make_addr(
			  &pg[i], mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0), PAGE_CACHE_WB);
			pages[i] = &pg[i];
		}
		mm_objspace_kernel_fill(oaddr,
		  pages,
		  INITIAL_ALLOC_PAGES,
		  MAP_READ | MAP_WRITE | MAP_KERNEL | MAP_WIRE | MAP_GLOBAL);
	}

	return &allpages[next_page++];
}

static struct page *fallback_page_alloc(void)
{
	uintptr_t addr = mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
	struct page *page = get_new_page_struct();
	page_make_addr(page, addr, PAGE_CACHE_WB);
	return page;
}

struct page *mm_page_fake_create(uintptr_t phys, int flags)
{
	spinlock_acquire_save_recur(&lock);
	struct page *page = get_new_page_struct();
	spinlock_release_restore(&lock);
	page_make_addr(page, phys, flags | PAGE_FAKE);
	return page;
}

static void mm_page_zero_addr(uintptr_t addr)
{
	/* we might get lucky here! */
	if(addr < MEMORY_BOOTSTRAP_MAX) {
		memset(mm_early_ptov(addr), 0, mm_page_size(0));
		atomic_thread_fence(memory_order_seq_cst);
		return;
	}
	struct page page = {};
	page_make_addr(&page, addr, PAGE_CACHE_WB);
	struct page *pages[] = { &page };
	void *vaddr = tmpmap_map_pages(pages, 1);
	memset(vaddr, 0, mm_page_size(0));
	atomic_thread_fence(memory_order_seq_cst);
}

void mm_page_zero(struct page *page)
{
	mm_page_zero_addr(mm_page_addr(page));
	page_set_flags(page, PAGE_ZERO);
}

void mm_page_write(struct page *page, void *data, size_t len)
{
	if(len > mm_page_size(0))
		len = mm_page_size(0);
	if(unlikely(len == 0))
		return;
	if(mm_page_addr(page) < MEMORY_BOOTSTRAP_MAX) {
		memcpy(mm_early_ptov(mm_page_addr(page)), 0, len);
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
	if(mm_page_addr(page) < MEMORY_BOOTSTRAP_MAX) {
		srcaddr = mm_early_ptov(mm_page_addr(page));
	}
	if(mm_page_addr(newpage) < MEMORY_BOOTSTRAP_MAX) {
		dstaddr = mm_early_ptov(mm_page_addr(newpage));
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
		uintptr_t addr = mm_page_addr(p);
		p->__addr_and_flags = 0;
		pagelist_add(&pagestruct_list, p);
		return (void *)addr;
	}
	return p;
}

static struct page *__do_mm_page_alloc(int flags)
{
	/* if we're requesting a zero'd page, try getting from the zero list first */
	if(flags & PAGE_ZERO) {
		if(!pagelist_empty(&pagezero_list)) {
			struct page *ret = pagelist_pop(&pagezero_list);
			return RET(flags, ret);
		}
	}

	struct page *page;
	/* prefer getting from non-zero list first. If we wanted a zero'd page and there was one
	 * available, we wouldn't be here. */
	if(pagelist_empty(&page_list)) {
		if(pagelist_empty(&pagezero_list)) {
			if(flags & PAGE_ADDR) {
				return (void *)mm_region_alloc_raw(mm_page_size(0), mm_page_size(0), 0);
			}
			page = fallback_page_alloc();
		} else {
			page = pagelist_pop(&pagezero_list);
		}
	} else {
		page = pagelist_pop(&page_list);
	}
	assert(page);

	return RET(flags, page);
}

void mm_page_free(struct page *page)
{
	/* TODO: determine if page is zero or not (with MMU dirty bit) */
	page_clear_flags(page, PAGE_ZERO);

	spinlock_acquire_save(&lock);
	if(mm_page_flags(page) & PAGE_ZERO) {
		pagelist_add(&pagezero_list, page);
	} else {
		pagelist_add(&page_list, page);
	}
	spinlock_release_restore(&lock);
}

struct page *mm_page_alloc(int flags)
{
	spinlock_acquire_save_recur(&lock);
	struct page *page = __do_mm_page_alloc(flags);
	spinlock_release_restore(&lock);
	if((flags & PAGE_ZERO) && !(mm_page_flags(page) & PAGE_ZERO)) {
		mm_page_zero(page);
	}
	return page;
}

uintptr_t mm_page_alloc_addr(int flags)
{
	spinlock_acquire_save_recur(&lock);
	void *p = __do_mm_page_alloc(flags | PAGE_ADDR);
	spinlock_release_restore(&lock);
	if(flags & PAGE_ZERO) {
		mm_page_zero_addr((uintptr_t)p);
	}
	return (uintptr_t)p;
}

void mm_page_print_stats(void)
{
	/* TODO (low): print page stats */
}

void mm_page_idle_zero(void)
{
	/* TODO (opt): background-zero pages */
}
