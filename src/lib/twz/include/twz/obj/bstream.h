#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/mutex.h>
#include <twz/obj/event.h>
#include <twz/obj/io.h>

struct __twzobj;
typedef struct __twzobj twzobj;

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least32_t;
extern "C" {
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

struct bstream_hdr {
	// struct mutex rlock, wlock;
	struct mutex lock;
	uint32_t flags;
	atomic_uint_least32_t head;
	atomic_uint_least32_t tail;
	uint32_t nbits;
	struct evhdr ev;
	struct twzio_hdr io;
	unsigned char data[];
};

static inline size_t bstream_hdr_size(uint32_t nbits)
{
	return sizeof(struct bstream_hdr) + (1ul << nbits);
}

#define BSTREAM_METAEXT_TAG 0x00000000bbbbbbbb

ssize_t bstream_write(twzobj *obj, const void *ptr, size_t len, unsigned flags);
ssize_t bstream_read(twzobj *obj, void *ptr, size_t len, unsigned flags);
ssize_t bstream_hdr_write(twzobj *obj,
  struct bstream_hdr *,
  const void *ptr,
  size_t len,
  unsigned flags);
ssize_t bstream_hdr_read(twzobj *obj, struct bstream_hdr *, void *ptr, size_t len, unsigned flags);
int bstream_poll(twzobj *obj, uint64_t type, struct event *event);
int bstream_hdr_poll(twzobj *obj, struct bstream_hdr *hdr, uint64_t type, struct event *event);

int bstream_obj_init(twzobj *obj, struct bstream_hdr *hdr, uint32_t nbits);

#ifdef __cplusplus
}
#endif
