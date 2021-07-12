#pragma once

/** @file
 * @brief Physical page tracking functions
 *
 * After bootstrapping, any physical memory that is used should be part of a struct page. This is
 * how object memory management manages physical memory, and ultimately how the kernel's memory
 * management system manages physical memory too. Cannot be used before finishing bootstrapping the
 * memory manager.
 */

#include <krc.h>
#include <spinlock.h>

struct page {
	/* store both addr and flags in 1 8-byte value to save space. */
	uintptr_t __addr_and_flags;
	/* We only need single-linked list semantics, so we'll manually implement that here to save
	 * space. */
	struct page *next;
};

#define MM_PAGE_ADDR_MASK 0xffffffffffffff00ul

/** Get address of a struct page */
static inline uintptr_t mm_page_addr(struct page *page)
{
	return page->__addr_and_flags & MM_PAGE_ADDR_MASK;
}

/** Get flags of a struct page */
static inline uint64_t mm_page_flags(struct page *page)
{
	return page->__addr_and_flags & (~MM_PAGE_ADDR_MASK);
}

/** Get the cache type of a page, one of PAGE_CACHE_* */
#define PAGE_CACHE_TYPE(p) (mm_page_flags(p) & 0x7)

/** Write-back */
#define PAGE_CACHE_WB 0
/** Uncachable */
#define PAGE_CACHE_UC 1
/** Write-through */
#define PAGE_CACHE_WT 2
/** Write-combining */
#define PAGE_CACHE_WC 3

/** This page is zero'd */
#define PAGE_ZERO 0x10

/** This page does not use "real" physical memory, it is created manually and should not be returned
 * to the page management system. */
#define PAGE_FAKE 0x20

void mm_page_print_stats(void);

/** Allocate a page. If flags sets PAGE_ZERO, the returned page will be zero'd. Otherwise, it may
 * or may not be zero'd. */
struct page *mm_page_alloc(int flags);

/** Allocate memory from the page system, but don't return a struct page, just allocate some
 * physical memory. */
uintptr_t mm_page_alloc_addr(int flags);

/** Zero a page. */
void mm_page_zero(struct page *page);

/** Initialize the page management system. Called as part of init for the memory manager */
void mm_page_init(void);

/** Zero some pages in the background. This function is not guaranteed to actually zero anything, it
 * just tries to if there is nothing else to do. */
void mm_page_idle_zero(void);

/** Create a "fake" page. Does not allocate physical memory, instead just allocates a struct page
 * and fills it out with the provided address (phys) and flags. */
struct page *mm_page_fake_create(uintptr_t phys, int flags);

/** Copy some data into a physical page. len must be less than mm_page_size(0). */
void mm_page_write(struct page *page, void *data, size_t len);

/** Clone a page. The returned page contains the same data as the original page, allocated in a new
 * place. Does not do anything to 'page'. */
struct page *mm_page_clone(struct page *page);

/** Free a page, returning both the struct page and the physical memory back to the unused pool. */
void mm_page_free(struct page *page);
