#pragma once

/** @file
 * @brief Temporary physical page mapping services
 *
 * It's common to need to map physical pages into kernel memory so that the kernel can access
 * physical memory. While modern CPUs typically operate in an environment where all physical memory
 * can be mapped into the virtual address space at all times, we choose to avoid relying on this,
 * partially because of our use of 2-level address translation, and because physical memory sizes
 * may continue to grow. Or maybe we don't have a 64-bit CPU.
 *
 * These functions operate on a per-cpu basis, so a mapping CANNOT be safely shared between CPUs.
 * If you want to create a shared physical mapping (probably a long-term mapping) see
 * \ref include/pmap.h. This is done because we want to avoid coordination costs and shootdown
 * costs, so each CPU is allocated a region of virtual memory that it can tmpmap into.
 */

/** The maximum number of physical pages that can be mapped by a single call to tmpmap_map_pages at
 * a time (per cpu) */
#define TMPMAP_MAX_PAGES 1024

struct page;

/** Map some number of pages into a contiguous virtual region.
 *
 * <b>IMPORTANT</b>: This mapping is percpu ONLY, and it CANNOT be shared between CPUs.
 *
 * <b>IMPORTANT</b>: Each call to this function may invalidate any previous mapping established by
 * this function. Thus, if you need to map multiple phyiscal pages, it must be done by a SINGLE call
 * to this function (hence the array style arguments). Additionally, this means that this function
 * cannot be called from interrupt context or fault context unless that fault came from userspace.
 *
 * @param pages An array of pointers to struct page, each of which is a physical page we want to
 * map.
 * @param count Length of the pages array.
 * @return A virtual address pointing to the start of the mapped pages, contiguous, in order as
 * specified in the pages array.
 */
void *tmpmap_map_pages(struct page *pages[], size_t count);

struct memory_stats;

/** Fill the memory_stats struct with information about tmpmaps. */
void tmpmap_collect_stats(struct memory_stats *stats);
