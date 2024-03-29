/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <string.h>
#include <twz/alloc.h>
#include <twz/meta.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/obj/bstream.h>
#include <twz/obj/event.h>
#include <twz/obj/io.h>
#include <twz/ptr.h>

#include <twz/debug.h>

static size_t free_space(size_t head, size_t tail, size_t length)
{
	return (tail > head) ? tail - head : length - head + tail;
}

ssize_t bstream_hdr_read(twzobj *obj,
  struct bstream_hdr *hdr,
  void *ptr,
  size_t len,
  unsigned flags)
{
	(void)obj;
	(void)flags;
	mutex_acquire(&hdr->lock);

	size_t count = 0;
	unsigned char *data = ptr;
	while(count < len) {
		if(hdr->head == hdr->tail) {
			if(count == 0 && !(flags & TWZIO_NONBLOCK)) {
				if(event_clear(&hdr->ev, TWZIO_EVENT_READ)) {
					continue;
				}
				mutex_release(&hdr->lock);
				struct event e;
				event_init(&e, &hdr->ev, TWZIO_EVENT_READ);
				event_wait(1, &e, NULL);
				mutex_acquire(&hdr->lock);
				continue;
			}
			break;
		}
		data[count] = hdr->data[hdr->tail];
		hdr->tail = (hdr->tail + 1) & ((1 << hdr->nbits) - 1);
		count++;
	}

	if(hdr->head == hdr->tail) {
		event_clear(&hdr->ev, TWZIO_EVENT_READ);
	}

	event_wake(&hdr->ev, TWZIO_EVENT_WRITE, -1);
	mutex_release(&hdr->lock);
	return (count == 0 && (flags & TWZIO_NONBLOCK)) ? -EAGAIN : (ssize_t)count;
}

ssize_t bstream_hdr_write(twzobj *obj,
  struct bstream_hdr *hdr,
  const void *ptr,
  size_t len,
  unsigned flags)
{
	(void)flags;
	(void)obj;
	mutex_acquire(&hdr->lock);

	size_t count = 0;
	const unsigned char *data = ptr;
	while(count < len) {
		if(free_space(hdr->head, hdr->tail, 1 << hdr->nbits) <= 1) {
			if(count == 0 && !(flags & TWZIO_NONBLOCK)) {
				if(event_clear(&hdr->ev, TWZIO_EVENT_WRITE)) {
					continue;
				}
				mutex_release(&hdr->lock);
				struct event e;
				event_init(&e, &hdr->ev, TWZIO_EVENT_WRITE);
				event_wait(1, &e, NULL);
				mutex_acquire(&hdr->lock);
				continue;
			}
			break;
		}
		hdr->data[hdr->head] = data[count];
		hdr->head = (hdr->head + 1) & ((1 << hdr->nbits) - 1);
		count++;
	}
	if(free_space(hdr->head, hdr->tail, 1 << hdr->nbits) <= 1) {
		event_clear(&hdr->ev, TWZIO_EVENT_WRITE);
	}

	event_wake(&hdr->ev, TWZIO_EVENT_READ, -1);
	mutex_release(&hdr->lock);
	return (count == 0 && (flags & TWZIO_NONBLOCK)) ? -EAGAIN : (ssize_t)count;
}

int bstream_hdr_poll(twzobj *obj, struct bstream_hdr *hdr, uint64_t type, struct event *event)
{
	(void)obj;
	if(event) {
		event_init(event, &hdr->ev, type);
	}
	// debug_printf(
	//"bstream_hdr_poll %p:  %p %p:  %p: %lx\n", hdr, event, &hdr->ev, event->hdr, hdr->ev.point);
	if(type == TWZIO_EVENT_READ)
		mutex_acquire(&hdr->lock);
	else if(type == TWZIO_EVENT_WRITE)
		mutex_acquire(&hdr->lock);
	else
		return -ENOTSUP;
	int r = !!event_poll(&hdr->ev, type);
	if(type == TWZIO_EVENT_READ)
		mutex_release(&hdr->lock);
	else if(type == TWZIO_EVENT_WRITE)
		mutex_release(&hdr->lock);
	return r;
}

int bstream_poll(twzobj *obj, uint64_t type, struct event *event)
{
	return bstream_hdr_poll(obj, twz_object_base(obj), type, event);
}

ssize_t bstream_ioread(twzobj *obj, void *ptr, size_t len, size_t off, unsigned flags)
{
	(void)off;
	return bstream_hdr_read(obj, twz_object_base(obj), ptr, len, flags);
}

ssize_t bstream_iowrite(twzobj *obj, const void *ptr, size_t len, size_t off, unsigned flags)
{
	(void)off;
	return bstream_hdr_write(obj, twz_object_base(obj), ptr, len, flags);
}

ssize_t bstream_read(twzobj *obj, void *ptr, size_t len, unsigned flags)
{
	return bstream_hdr_read(obj, twz_object_base(obj), ptr, len, flags);
}

ssize_t bstream_write(twzobj *obj, const void *ptr, size_t len, unsigned flags)
{
	return bstream_hdr_write(obj, twz_object_base(obj), ptr, len, flags);
}

int bstream_obj_init(twzobj *obj, struct bstream_hdr *hdr, uint32_t nbits)
{
	if(hdr == NULL) {
		hdr = twz_object_base(obj);
	}
	twz_object_init_alloc(obj, bstream_hdr_size(nbits));
	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		goto cleanup;
	if((r = twz_object_addext(obj, EVENT_METAEXT_TAG, &hdr->ev)))
		goto cleanup;
	memset(hdr, 0, sizeof(*hdr));
	mutex_init(&hdr->lock);
	hdr->nbits = nbits;
	event_obj_init(obj, &hdr->ev);

	if((r = twz_ptr_store_name(obj,
	      &hdr->io.read,
	      "/usr/lib/libtwz.so::bstream_ioread",
	      NULL,
	      TWZ_NAME_RESOLVER_SOFN,
	      FE_READ | FE_EXEC))) {
		goto cleanup;
	}

	if((r = twz_ptr_store_name(obj,
	      &hdr->io.write,
	      "/usr/lib/libtwz.so::bstream_iowrite",
	      NULL,
	      TWZ_NAME_RESOLVER_SOFN,
	      FE_READ | FE_EXEC))) {
		goto cleanup;
	}

	if((r = twz_ptr_store_name(obj,
	      &hdr->io.poll,
	      "/usr/lib/libtwz.so::bstream_poll",
	      NULL,
	      TWZ_NAME_RESOLVER_SOFN,
	      FE_READ | FE_EXEC))) {
		goto cleanup;
	}

	return 0;
cleanup:
	twz_object_delext(obj, TWZIO_METAEXT_TAG, &hdr->io);
	twz_object_delext(obj, EVENT_METAEXT_TAG, &hdr->ev);
	return r;
}
