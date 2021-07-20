#pragma once

/** @file
 * @brief Functions for handling virtual memory.
 *
 * The kernel manages memory with views, vm_contexts, and vmaps. The relationship between these is
 * as follows:
 *   1. A view is a KSO (see \ref ksos). A given view may be used by multiple threads. A view has a
 *      number of vm_contexts associated with it.
 *   2. A vm_context is associated with a single view. A thread has exactly one vm_context active.
 *   3. A vm_context has a number of vmaps, each one specifying a mapping of a virtual region to an
 *      object space region.
 *
 * A vm_context also contains an arch-specific structure (arch_vm_context) which contains the actual
 * page tables that the CPU will be given on a switch.
 *
 * Currently, each thread runs in its own vm_context, which has a performance overhead. I want to
 * look into alleviating this. But also, with modern tagged TLB entries, maybe it's not too bad (we
 * should measure, of course). The reason that threads each get their own vm_context is that thread
 * has a special "fixed" mapping of objects for, eg, its own thread object to be at a fixed point.
 */

#include <__mm_bits.h>
#include <krc.h>
#include <lib/list.h>
#include <lib/rb.h>
#include <twz/sys/view.h>
struct object;
struct omap;

struct vm_context {
	struct arch_vm_context arch;
	struct object *viewobj;
	struct rbroot root;
	struct spinlock lock;
	struct list entry;
};

/** Switch virtual memory context. Immediately changes the CPU's page-tables to the new context.
 * This might trigger some of the TLB to be flushed, depending on the architecture.
 * @param vm The context to switch to. If NULL, the CPU will be switched to a "kernel context" that
 * is reserved for just the kernel.
 */
void arch_mm_switch_context(struct vm_context *vm);

/** Initialize the arch-specific aspects of a virtual memory context. Called as part of init for
 * vm_context slab objects (see \ref include/slab.h). Do not call this directly. */
void arch_vm_context_init(struct vm_context *ctx);

/** Called when a vm_context gets free'd back onto the slab (see \ref include/slab.h). Do not call
 * this directly. */
void arch_vm_context_dtor(struct vm_context *ctx);

/** Destroy the arch-specific part of a vm_context. Called as fini for slab objects (see \ref
 * include/slab.h). Do not call this directly.
 */
void arch_mm_context_destroy(struct vm_context *ctx);

/** Create a new vm_context, allocating from a slab internally. This new context is immediately
 * usable, will share kernel mappings in the kernel region, and will have nothing mapped in the user
 * region. */
struct vm_context *vm_context_create(void);

/** Free a vm_context. This will also free any internal mappings. You MUST NOT call this function on
 * the active vm_context. */
void vm_context_free(struct vm_context *ctx);

/** Print information about mappings in a context to the debug console. */
void arch_mm_print_ctx(struct vm_context *ctx);

/** Invalidate a region of virtual memory, causing the TLB to flush and shootdown.
 * TODO: we should add support for controlling how shootdown happens.
 *
 * @param ctx the context to invalidate. If NULL, invalidate all contexts.
 * @param virt the virtual address to start invalidating from. If this refers to kernel region
 * memory, then treat ctx as if it were NULL.
 * @param len the length of memory to invalidate. [virt, virt+len) must not cross the boundary
 * between user and kernel regions.
 */
void arch_mm_virtual_invalidate(struct vm_context *ctx, uintptr_t virt, size_t len);

/** A special context used during initialization or if a CPU has no threads to run. Contains shared
 * kernel mappings with all other contexts, and has no user mappings. */
extern struct vm_context kernel_ctx;

struct vmap {
	struct omap *omap;
	struct object *obj;
	size_t slot;
	uint32_t flags;
	struct rbnode node;
};

/** Write a view entry to a view object. Used during startup to construct an initial view for init.
 *
 * @param obj The view object to write to. If this object is not a view object, panic.
 * @param slot The slot to write to in the view object (see userspace documentation on Views).
 * @param v The viewentry to write (see userspace documentation on Views).
 */
void kso_view_write(struct object *obj, size_t slot, struct viewentry *v);

/** Set a thread's view. While the view becomes the thread's active view immediately, it does not
 * immediately update the page-tables (this happens next time the thread re-enters userspace). The
 * thread's previous view gets backed up as an optimization, making it easier to switch between a
 * small number of views, so replacing a view doesn't always free resources from the prior view.
 */
void vm_setview(struct thread *thr, struct object *viewobj);

/** Lookup an object in the current view based on address. Will look for existing mappings in the
 * current vm_context first. If it doesn't find anything, it will try to map in the object.
 *
 * @param addr The virtual address to use when looking up the object.
 * @param[out] off Calculated offset into the object based on addr.
 * @return The object mapped into the address `addr`. This pointer is a strong reference to the
 * object. If NULL, then no object could be found for that address.
 */
struct object *vm_vaddr_lookup_obj(void *addr, uint64_t *off);

/** Handle a fault that was caused by userspace.
 * @param ip Instruction pointer address that caused the fault.
 * @param addr Address of the fault.
 * @param flags Bitwise-OR of FAULT_ constants. */
void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags);

/** Lookup an object in the context `ctx` without mapping an an object if there is not current
 * mapping. Used during object-space fault handling, where an axiom is that if userspace has
 * generated a fault to a given object space address, there would have to be a virtual memory
 * mapping defined first.
 *
 * @param ctx the context to search
 * @param virt the virtual address to use. Must be a userspace address.
 * @return the object mapped at address `virt`. If not such mapping exists, return NULL.
 */
struct object *vm_context_lookup_object(struct vm_context *ctx, uintptr_t virt);

#define FAULT_EXEC 0x1        /**< Fault was caused by an instruction fetch */
#define FAULT_WRITE 0x2       /**< Fault was caused by a write */
#define FAULT_USER 0x4        /**< Fault took place in user-mode */
#define FAULT_ERROR_PERM 0x10 /**< Fault was a permission error */
#define FAULT_ERROR_PRES 0x20 /**< Fault was caused by a non-present mapping */

/** Entry point for fault handling. See \ref vm_context_fault for params. If the fault was caused by
 * the kernel, panic. If the fault was userspace trying to access kernel memory, kill the thread.
 * Otherwise, call vm_context_fault. */
void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags);

/** Unmap and invalidate from page-tables mappings for range [virt, virt + len). */
void arch_mm_unmap(struct vm_context *ctx, uintptr_t virt, size_t len);

/** Map virtual memory for the kernel. Internally, this will try to use the biggest page size
 * possible. For example, if we map 1GB on a 1GB-aligned address, it'll use huge pages to map. If we
 * map a several GB region that isn't aligned on 1GB, it'll map the first part with smaller pages
 * until it gets something 1GB aligned, and then it uses huge pages. This logic applies to 2MB large
 * pages too. Note that alignment requirements for this kind of optimization apply to `addr`,
 * `oaddr` _and_ `len`.
 *
 * @param addr The virtual address to start mapping. Must be page-aligned.
 * @param oaddr The output address of the mapping (object space address). Must be page-aligned.
 * @param len Length of the mapping. Must be page aligned.
 * @param flags See \ref include/__mm_bits.h. */
void mm_map(uintptr_t addr, uintptr_t oaddr, size_t len, int flags);

/** Map memory in page tables. Called by \ref mm_map, and the last 4 parameters are the same between
 * these two functions. The first argument is the context to do this mapping in. If NULL, use the
 * \ref kernel_ctx.
 */
int arch_mm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uint64_t mapflags);
