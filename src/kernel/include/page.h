#pragma once

#include <krc.h>
#include <spinlock.h>

struct page {
	uintptr_t addr;
	uint32_t flags;
	uint32_t resv;
	struct list entry;
};

#define PAGE_CACHE_TYPE(p) ((p)->flags & 0x7)
#define PAGE_CACHE_WB 0
#define PAGE_CACHE_UC 1
#define PAGE_CACHE_WT 2
#define PAGE_CACHE_WC 3

#define PAGE_ZERO 0x10
void mm_page_print_stats(void);
struct page *mm_page_alloc(int flags);
uintptr_t mm_page_alloc_addr(int flags);
void mm_page_zero(struct page *page);
void mm_page_init(void);
void mm_page_idle_zero(void);
