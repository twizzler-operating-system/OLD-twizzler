#pragma once

/** @file
 * @brief Allocate and manage memory inside Twizzler objects.
 *
 * Memory is managed inside Twizzler objects according to the user, but can optionally be made
 * managed through this allocator. This allocator is able to allocation custom sized regions of
 * memory within objects to help construct data structures within objects.
 *
 * @par[Rationale]
 * The thing that makes this more complex than simple "malloc" and "free" is failure-atomicity and
 * ownership. Let's look at a simple memory allocator interface to see why it won't work:
 *
 *     void *alloc(size_t sz);
 *
 * This function returns a pointer to the newly allocated memory. Ideally, we would now write the
 * pointer returned into a persistent data structure (and then persisting that update properly). But
 * what if we fail after the call to alloc but before we can write the pointer? The memory will be
 * leaked. Instead, we must consider the ownership of the memory. Before it's allocated, its owned
 * by the allocator. When a region has been allocated, it is owned by a data structure. This
 * ownership change must be failure-atomic, that is, if we crash while performing the allocation, we
 * must either have not done the allocation, or the ownership of the region must be fully
 * transferred.
 *
 * To make this work, the allocation function must take in a pointer to the "owned pointer"
 * location, ie, where we plan to write the pointer to the newly allocated region. In addition, we
 * must also ensure that the object we are allocating is properly constructed before passing
 * ownership. This is because the ownership transfer marks the success of the allocation (and thus
 * the region is now pointed to by some user-controlled data structure). So the allocation function
 * must _also_ support construction as part of the interface, making the constructor of a memory
 * region part of the failure-atomicity of the allocation and ownership transfer.
 */

/// \cond DO_NOT_DOCUMENT
#define TWZ_ALLOC_ALIGN(x) ((uint64_t)(x) << 32)
#define TWZ_ALLOC_VOLATILE 1
#define TWZ_ALLOC_CTOR_ZERO (void *)1

#define ALLOC_METAINFO_TAG 0xaaaaaaaa11223344
#include <stddef.h>
#include <stdint.h>
struct __twzobj;
typedef struct __twzobj twzobj;
/// \endcond
/** Initialize an object for memory allocation, starting at offset bytes into the object, until
 * reaching the end of the object's data region.
 *
 * @par[Failure-Atomicity]
 * This function is failure-atomic.
 *
 * @param obj The object to initialize allocation for.
 * @param offset The starting point for allocator-owned memory in the object, with 0 being the base
 *        of the object.
 * @return 0 on success, -ERROR on failure.
 */
int twz_object_init_alloc(twzobj *obj, size_t offset);

/** Allocate region of memory within an object, returning it to the "owned pointer" owner.
 * Optionally initializes the memory region with a constructor. This function is thread-safe.
 *
 * @par[Failure-Atomicity]
 * The allocation, construction (with ctor), and
 * write of the final pointer into *owner are all failure-atomic. The contents of the newly
 * allocated region will be made durable after the constructor is run as part of the
 * failure-atomicity.
 *
 *
 * @param obj The object to allocate from.
 * @param len The length of the allocation. The final allocation may be larger than requested, but
 *        this cannot be relied on. If len is zero, the allocator will still return a valid
 * pointer.
 * @param[out] owner The result of the allocation will be written into *owner. This must be a
 *             location within the same object as the allocator.
 * @param flags Flags and alignment information. Bitwise OR of the following:
 *                - TWZ_ALLOC_ALIGN(x): Align on x. Default operation of the allocator without
 *                  alignment specified is the same as if x is 0: alignment is, at minimum, on 16.
 *                - TWZ_ALLOC_VOLATILE: Disable failure-atomicity for this operation. This is
 *                  dangerous for persistent objects, but can decrease allocation latency.
 * @param ctor Constructor to run during allocation. The first argument is a d-ptr to the new
 *        allocation, and the second argument is the passed-through data parameter. Special values
 *        include:
 *          - NULL: No constructor function will be run.
 *          - TWZ_ALLOC_CTOR_ZERO: Zeros the contents of the allocation.
 * @param data Opaque pointer to be passed to the constructor.
 * @return 0 on success, -ERROR on failure.
 *   - ENOMEM: no memory available
 *   - EINVAL: object was not setup for allocation
 */
int twz_alloc(twzobj *obj,
  size_t len,
  void **owner,
  uint64_t flags,
  void (*ctor)(void *, void *),
  void *data);

/** Free a previously allocated region of memory, and NULL-ing the owned pointer. May return memory
 * to the kernel and deallocate object pages. This function is thread-safe.
 *
 * @par[Failure-Atomicity]
 * Freeing the memory and NULL-ing the value in *owner are both
 * failure-atomic.
 *
 * @param obj The object for which we're freeing data.
 * @param p Pointer (d-ptr or p-ptr) to the memory region to free. If NULL, twz_free returns.
 * @param[out] Pointer to the owned location of the memory region. Will be NULL-ed as part of the
 *             free.
 * @param flags Flags affecting operation. Bitwise OR of of the following:
 *                - TWZ_ALLOC_VOLATILE: see twz_alloc()
 */
void twz_free(twzobj *obj, void *p, void **owner, uint64_t flags);

/** Perform a reallocation on a region of memory, replacing the value in the owned pointer to point
 * to a new region. This function is thread-safe.
 *
 * @par[Failure-Atomicity]
 * The reallocation, possibly copy of data, and update of the owned pointer
 * are all failure-atomic. If a new region is allocated and a copy is performed, the contents of the
 * new region are made durable as part of the transaction.
 *
 * @param obj The object in which the reallocation is taking place.
 * @param p Existing region of memory to reallocate.
 * @param[out] Pointer to the owned location of the memory region. Gets updated to point to the new
 *             location if reallocation is performed.
 * @param flags Flags affecting operation. Bitwise OR of the following:
 *                - TWZ_ALLOC_VOLATILE: see twz_alloc()
 * @return 0 on success, -ERROR on failure, with the same failure modes as twz_alloc().
 */
int twz_realloc(twzobj *obj, void *p, void **owner, size_t newlen, uint64_t flags);
