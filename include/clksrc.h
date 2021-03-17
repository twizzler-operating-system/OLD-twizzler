#pragma once

/** @file
 * @brief Functions for dealing with time and clocks
 */

#include <lib/list.h>

#define CLKSRC_MONOTONIC 1
#define CLKSRC_INTERRUPT 2
#define CLKSRC_ONESHOT 4
#define CLKSRC_PERIODIC 8

struct clksrc {
	uint64_t flags;
	uint64_t precision;
	uint64_t read_time;
	uint64_t period_ps;
	const char *name;
	void *priv;

	uint64_t (*read_counter)(struct clksrc *);
	void (*set_timer)(struct clksrc *, uint64_t ns, bool periodic);
	void (*set_active)(struct clksrc *, bool enable);

	struct list entry;
};

/** Register a clock source. This is intended to be called by lower-level system-specific code that
 * knows how to source time information from hardware. */
void clksrc_register(struct clksrc *cs);

/** Remove a clock source. This is unlikely to be needed. */
void clksrc_deregister(struct clksrc *cs);

/** Return a monotonically increasing value that increases by 1 per nanosecond. This will be derived
 * from the best "monotonic" clock source so far registered. */
uint64_t clksrc_get_nanoseconds(void);

/** Set a clock source as active, if the source supports it. Used primarily for setting interrupt
 * timers. */
void clksrc_set_active(struct clksrc *cs, bool active);

/** Enable a clock source as an interrupt timer.
 * @param cs The clock source
 * @param ns Nanoseconds for timer
 * @param periodic true if the interrupt should repeat, false for one-shot.
 * @return true for success, false for failure
 */
bool clksrc_set_timer(struct clksrc *cs, uint64_t ns, bool periodic);

/** Set an interrupt timer. Chooses the "best" interrupt timer so far registered.
 * @see clksrc_set_timer
 */
void clksrc_set_interrupt_countdown(uint64_t ns, bool periodic);

/** Get the nanoseconds until next time interrupt is expected to fire.
 */
uint64_t clksrc_get_interrupt_countdown(void);
