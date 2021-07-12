#pragma once

#include <page.h>

/** @file
 * @brief Permanent physical memory mapping management.
 *
 * This is used to map physical memory into virtual memory in a long-lasting, sharable way. If you
 * just need to map in some pages temporarily, use \ref include/tmpmap.h instead.
 */

#define PMAP_WB PAGE_CACHE_WB
#define PMAP_UC PAGE_CACHE_UC
#define PMAP_WT PAGE_CACHE_WT
#define PMAP_WC PAGE_CACHE_WC

/** Allocate some virtual region that maps to physical memory.
 * @param phys the physical memory that we will map to. Does not need to be page aligned.
 * @param len length of the mapping. does not need to be page aligned.
 * @param flags. Caching flags, one of PMAP_*.
 * @return a virtual memory pointer that will be mapped to the start of physical memory referred to
 * by phys. This function may map more memory than requested to satisfy internal alignment
 * requirements.
 */
void *pmap_allocate(uintptr_t phys, size_t len, int flags);
struct memory_stats;
void pmap_collect_stats(struct memory_stats *stats);
