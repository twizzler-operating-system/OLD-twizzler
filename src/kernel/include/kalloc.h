#pragma once

/** @file
 * @brief Generic malloc-like allocation routines */

/** Zero out allocated memory during allocation */
#define KALLOC_ZERO 1

/** Allocate sz bytes of memory, aligned on (at minimum) 8 bytes.
 * @param sz Number of bytes to allocate.
 * @param flags Flags, bitwise or of KALLOC_*.
 * @return Newly allocated region of memory. If flags does not have KALLOC_ZERO, this memory is
 * uninitialized. */
void *kalloc(size_t sz, int flags);

/** Similar to kalloc, but allocate num * sz bytes. */
void *kcalloc(size_t num, size_t sz, int flags);

/** Reallocate a region of memory.
 * @param p Existing allocated region. If NULL, this acts like kalloc.
 * @param sz New size. If larger, the new memory will be zero'd or not depending on flags.
 * @param flags bitwise or of KALLOC_*.
 * @return Reallocated region of memory. Not guaranteed to be the same address as p. */
void *krealloc(void *p, size_t sz, int flags);
/* Similar to krealloc, except num * sz for allocation size. */
void *krecalloc(void *p, size_t num, size_t sz, int flags);
/** Free an allocation. */
void kfree(void *p);

/** Initialize kalloc system. Called when moving from bootstrap to primary memory management system
 */
void kalloc_system_init(void);
