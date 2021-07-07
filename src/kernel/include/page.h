#pragma once

#include <krc.h>
#include <spinlock.h>

struct page {
	uintptr_t __addr_and_flags;
	struct page *next;
};

#define MM_PAGE_ADDR_MASK 0xffffffffffffff00ul

static inline uintptr_t mm_page_addr(struct page *page)
{
	return page->__addr_and_flags & MM_PAGE_ADDR_MASK;
}

static inline uint64_t mm_page_flags(struct page *page)
{
	return page->__addr_and_flags & (~MM_PAGE_ADDR_MASK);
}

#define PAGE_CACHE_TYPE(p) (mm_page_flags(p) & 0x7)
#define PAGE_CACHE_WB 0
#define PAGE_CACHE_UC 1
#define PAGE_CACHE_WT 2
#define PAGE_CACHE_WC 3

#define PAGE_ZERO 0x10
#define PAGE_FAKE 0x20

void mm_page_print_stats(void);
struct page *mm_page_alloc(int flags);
uintptr_t mm_page_alloc_addr(int flags);
void mm_page_zero(struct page *page);
void mm_page_init(void);
void mm_page_idle_zero(void);
struct page *mm_page_fake_create(uintptr_t phys, int flags);
void mm_page_write(struct page *page, void *data, size_t len);
struct page *mm_page_clone(struct page *page);
void mm_page_free(struct page *page);
