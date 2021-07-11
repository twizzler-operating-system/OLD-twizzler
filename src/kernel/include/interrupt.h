#pragma once

/** @file
 * @brief Interrupt management routines */

#include <arch/interrupt.h>
#include <lib/list.h>

/** A single interrupt handler struct, used to specify callbacks for a given interrupt. Multiple
 * interrupt_handlers may be associated with a single vector, however an interrupt_handler struct
 * may only be associated with a single vector. */
struct interrupt_handler {
	/** Callback function when handling interrupt */
	void (*fn)(int vec, struct interrupt_handler *_this);
	/** Device object associated with this interrupt handler */
	struct object *devobj;
	/** User-defined data */
	long arg;
	/** (private) */
	struct list entry;
};

/** Vector allocation priority */
enum iv_priority {
	/** Allow vector reuse */
	IVP_NORMAL,
	/** Fail if we cannot find an unallocated vector */
	IVP_UNIQUE,
};

/** The interrupt_alloc_req is valid (it will be processed). */
#define INTERRUPT_ALLOC_REQ_VALID 1
/** The interrupt request is enabled (it successfully allocated a vector) */
#define INTERRUPT_ALLOC_REQ_ENABLED 2

/** An interrupt allocation request. Used by device code to allocate sets of vectors */
struct interrupt_alloc_req {
	/** The interrupt handler to use when handling interrupts from this allocation. */
	struct interrupt_handler handler;
	/** Flags (INTERRUPT_ALLOC_REQ_*) */
	uint8_t flags;
	/** Vector priority */
	enum iv_priority pri;
	/** [out] Vector allocated */
	int vec;
};

/** Try to allocate a number of vectors.
 * @param count The number of elements in the req array.
 * @param req Array of interrupt_alloc_req containing allocation requests. Each interrupt_alloc_req
 * may be modified inside this function (to set vec and flags)
 * @return 0 on success, -1 on failure */
int interrupt_allocate_vectors(size_t count, struct interrupt_alloc_req *req);

/** Entry point into the main kernel from arch-specific code for interrupts.
 * @param vec Vector number
 */
void kernel_interrupt_entry(int vec);

/** Register an interrupt handler. Multiple handlers per vector may be specified. If an interrupt
 * comes in for a vector, all handlers will be run.
 * @param vector The vector number to subscribe to.
 * @param handler The handler to run.
 */
void interrupt_register_handler(int vector, struct interrupt_handler *handler);

/** Unregister a specific interrupt_handler. */
void interrupt_unregister_handler(int vector, struct interrupt_handler *handler);

/** Unmask an interrupt. This is architecture-specific */
void arch_interrupt_unmask(int v);

/** Mask an interrupt. This is architecture-specific */
void arch_interrupt_mask(int v);
