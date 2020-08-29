#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
extern "C" {
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#include <stdint.h>
#include <time.h>

#include <twz/_types.h>

struct evhdr {
	atomic_uint_least64_t point;
};

struct event {
	struct evhdr *hdr;
	uint64_t events;
	uint64_t result;
	uint64_t __resv[2];
	uint64_t flags;
};

void event_obj_init(twzobj *obj, struct evhdr *hdr);
void event_init(struct event *ev, struct evhdr *hdr, uint64_t events);
int event_wait(size_t count, struct event *ev, const struct timespec *);
int event_wake(struct evhdr *ev, uint64_t events, long wcount);
uint64_t event_clear(struct evhdr *hdr, uint64_t events);

static inline uint64_t event_poll(const struct evhdr *hdr, uint64_t events)
{
	return hdr->point & events;
}

#define EVENT_METAEXT_TAG 0x000000001122ee00eeee

#ifdef __cplusplus
}
#endif
