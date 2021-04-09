#pragma once
#include <lib/rb.h>

/** @file
 *  @brief Manage timers and callbacks for timers
 */

typedef uint64_t dur_nsec;

struct timer {
	dur_nsec time;
	size_t id;
	void (*fn)(void *);
	void *data;
	struct rbnode node;
	bool active;
};

/**
 * Add a timer for calling a function after a certain time. The timer will be automatically removed
 * after the callback is called, and the callback is guaranteed to be called exactly once.
 *
 * @param t The timer struct that will be filled out and used internally. It must live long enough
 *     for the callback to fire, or the timer to be removed.
 * @param time Duration in nanoseconds. The callback will happen sometime (but not necessarily
 *     immediately) after this duration.
 * @param fn The function to call.
 * @param data An opaque pointer to pass to the callback.
 */
void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data);

/**
 * Remove a previously registered timer (with timer_add). The callback of the timer may race with
 * this function, but will not be triggered once this function returns.
 *
 * @param t The timer to remove
 */
void timer_remove(struct timer *t);

/**
 * Check existing registered timers and trigger their callbacks if their time has expired. This
 * function does not need to be called manually, as it is invoked by the scheduler.
 */
uint64_t timer_check_timers(void);
