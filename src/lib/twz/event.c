/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/obj.h>
#include <twz/obj/event.h>
#include <twz/sys/sync.h>
#include <twz/sys/sys.h>
#include <twz/sys/syscall.h>

void event_obj_init(twzobj *obj, struct evhdr *hdr)
{
	(void)obj;
	hdr->point = 0;
}

static _Atomic uint64_t *__event_point(struct event *ev)
{
	return (ev->flags & EVENT_OTHER) ? ev->other : &ev->hdr->point;
}

void event_init(struct event *ev, struct evhdr *hdr, uint64_t events)
{
	ev->hdr = hdr;
	ev->events = events;
	ev->flags = 0;
}

void event_init_other(struct event *ev, atomic_uint_least64_t *hdr, uint64_t events)
{
	ev->other = hdr;
	ev->events = events;
	ev->flags = EVENT_OTHER;
}

uint64_t event_clear(struct evhdr *hdr, uint64_t events)
{
	uint64_t old = atomic_fetch_and(&hdr->point, ~events);
	return old & events;
}

uint64_t event_clear_other(atomic_uint_least64_t *hdr, uint64_t events)
{
	uint64_t old = atomic_fetch_and(hdr, ~events);
	return old & events;
}

int event_wait(size_t count, struct event *ev, const struct timespec *timeout)
{
	if(count > 4096) {
		return -EINVAL;
	}
	int again = 0;
	while(true) {
		struct sys_thread_sync_args args[count];
		size_t ready = 0;
		for(size_t i = 0; i < count; i++) {
			_Atomic uint64_t *point = __event_point(&ev[i]);
			args[i].arg = *point;
			args[i].addr = (uint64_t *)point;
			args[i].op = THREAD_SYNC_SLEEP;
			ev[i].result = args[i].arg & ev[i].events;
			if(ev[i].result) {
				ready++;
			}
		}
		if(ready > 0 || (again && timeout))
			return ready;

		struct timespec ts = timeout ? *timeout : (struct timespec){};
		int r = sys_thread_sync(count, args, timeout ? &ts : NULL);
		if(r < 0)
			return r;
		again = 1;
	}
}

int event_wake(struct evhdr *ev, uint64_t events, long wcount)
{
	uint64_t old = atomic_fetch_or(&ev->point, events);
	if(((old & events) != events)) {
		struct sys_thread_sync_args args = {
			.op = THREAD_SYNC_WAKE,
			.addr = (uint64_t *)&ev->point,
			.arg = wcount,
		};
		return sys_thread_sync(1, &args, NULL);
	}
	return 0;
}

int event_wake_other(atomic_uint_least64_t *other, uint64_t events, long wcount)
{
	uint64_t old = atomic_fetch_or(other, events);
	if(((old & events) != events)) {
		struct sys_thread_sync_args args = {
			.op = THREAD_SYNC_WAKE,
			.addr = (uint64_t *)other,
			.arg = wcount,
		};
		return sys_thread_sync(1, &args, NULL);
	}
	return 0;
}
