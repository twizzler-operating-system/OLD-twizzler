/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdlib.h>
#include <twz/meta.h>
#include <twz/obj.h>
#include <twz/obj/queue.h>

int queue_submit(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(
	  obj, twz_object_base(obj), SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_complete(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(
	  obj, twz_object_base(obj), SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_receive(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(
	  obj, twz_object_base(obj), SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_get_finished(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(
	  obj, twz_object_base(obj), SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

#define HDRVERSION(name)                                                                           \
	int queue_hdr_##name(struct queue_hdr *hdr, struct queue_entry *qe, int flags)                 \
	{                                                                                              \
		twzobj obj;                                                                                \
		twz_object_init_ptr(&obj, hdr);                                                            \
		return queue_##name(&obj, qe, flags);                                                      \
	}

HDRVERSION(submit);
HDRVERSION(complete);
HDRVERSION(receive);
HDRVERSION(get_finished);

ssize_t queue_dequeue_multiple(size_t count, struct queue_dequeue_multiple_spec *specs)
{
	return queue_sub_dequeue_multiple(count, specs);
}

/* this is a terrible hack to get things working with other languages that don't use the C twzobj
 * structure. */
ssize_t queue_hdr_dequeue_multiple(size_t count, struct queue_dequeue_multiple_spec *specs)
{
	twzobj *objs = calloc(count, sizeof(twzobj));
	void **hdrs = calloc(count, sizeof(void *));
	for(size_t i = 0; i < count; i++) {
		if(specs[i].ishdr == 1) {
			twz_object_init_ptr(&objs[i], specs[i].hdr);
			hdrs[i] = specs[i].hdr;
			specs[i].obj = &objs[i];
			specs[i].ishdr = 1;
		}
	}
	ssize_t ret = queue_dequeue_multiple(count, specs);
	for(size_t i = 0; i < count; i++) {
		if(specs[i].ishdr == 1) {
			specs[i].hdr = hdrs[i];
		}
	}
	free(hdrs);
	free(objs);
	return ret;
}

struct queue_dequeue_multiple_spec queue_hdr_build_multispec(struct queue_hdr *hdr,
  int direction,
  struct queue_entry *res)
{
	struct queue_dequeue_multiple_spec spec = {
		.hdr = hdr, .sq = direction, .result = res, .ishdr = 1
	};
	return spec;
}

int queue_hdr_init(struct queue_hdr *hdr,
  size_t sqlen,
  size_t sqstride,
  size_t cqlen,
  size_t cqstride)
{
	if(sqstride < sizeof(struct queue_entry))
		sqstride = sizeof(struct queue_entry);
	sqstride = (sqstride + 7) & ~7;
	if(cqstride < sizeof(struct queue_entry))
		cqstride = sizeof(struct queue_entry);
	if((1ul << sqlen) * sqstride + sizeof(struct queue_hdr) + (1ul << cqlen) * cqstride
	   >= OBJ_TOPDATA) {
		return -EINVAL;
	}
	cqstride = (cqstride + 7) & ~7;
	memset(hdr, 0, sizeof(*hdr));
	hdr->subqueue[SUBQUEUE_SUBM].l2length = sqlen;
	hdr->subqueue[SUBQUEUE_CMPL].l2length = cqlen;
	hdr->subqueue[SUBQUEUE_SUBM].stride = sqstride;
	hdr->subqueue[SUBQUEUE_CMPL].stride = cqstride;
	hdr->subqueue[SUBQUEUE_SUBM].queue = (void *)twz_ptr_local(hdr + 1);
	hdr->subqueue[SUBQUEUE_CMPL].queue =
	  (void *)twz_ptr_local((char *)(hdr + 1) + (1ul << sqlen) * sqstride);
	return 0;
}

int queue_init_hdr(twzobj *obj, size_t sqlen, size_t sqstride, size_t cqlen, size_t cqstride)
{
	struct queue_hdr *hdr = twz_object_base(obj);
	return queue_hdr_init(hdr, sqlen, sqstride, cqlen, cqstride);
}
