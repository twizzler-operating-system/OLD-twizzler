/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/meta.h>
#include <twz/obj.h>
#include <twz/obj/io.h>
#include <twz/ptr.h>

#include <twz/obj/bstream.h>
#include <twz/obj/pty.h>
static struct twzio_hdr defaults[] = { [UNKNOWN] = {},
	[BSTREAM] = {
	  .read = bstream_ioread,
	  .write = bstream_iowrite,
	  .ioctl = NULL,
	  .poll = bstream_poll,
	},
	[PTY_SERVER] = {
	  .read = pty_ioread_server,
	  .write = pty_iowrite_server,
	  .ioctl = pty_ioctl_server,
	  .poll = pty_poll_server,
	},
	[PTY_CLIENT] = {
	  .read = pty_ioread_client,
	  .write = pty_iowrite_client,
	  .ioctl = pty_ioctl_client,
	  .poll = pty_poll_client,
	},
};

#define array_len(x) ({ sizeof(x) / sizeof((x)[0]); })

#define TRY_DEFAULT(iotype, hdr, ...)                                                              \
	do {                                                                                           \
		if(hdr->type < array_len(defaults) && hdr->type > 0) {                                     \
			if(defaults[hdr->type].iotype) {                                                       \
				return defaults[hdr->type].iotype(__VA_ARGS__);                                    \
			}                                                                                      \
		}                                                                                          \
	} while(0);

ssize_t twzio_read(twzobj *obj, void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr)
		return -ENOTSUP;

	TRY_DEFAULT(read, hdr, obj, buf, len, off, flags);

	void *_fn = twz_object_lea(obj, hdr->read);
	if(!hdr->write || !_fn)
		return -ENOTSUP;
	ssize_t (*fn)(twzobj *, void *, size_t, size_t, unsigned) = _fn;
	return fn(obj, buf, len, off, flags);
}

ssize_t twzio_write(twzobj *obj, const void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr)
		return -ENOTSUP;

	TRY_DEFAULT(write, hdr, obj, buf, len, off, flags);

	void *_fn = twz_object_lea(obj, hdr->write);
	if(!hdr->write || !_fn)
		return -ENOTSUP;
	ssize_t (*fn)(twzobj *, const void *, size_t, size_t, unsigned) = _fn;
	ssize_t ret = fn(obj, buf, len, off, flags);
	return ret;
}

ssize_t twzio_ioctl(twzobj *obj, int req, ...)
{
	va_list vp;
	va_start(vp, req);
	long arg = va_arg(vp, long);
	va_end(vp);
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr)
		return -ENOTSUP;

	TRY_DEFAULT(ioctl, hdr, obj, req, arg);

	void *_fn = twz_object_lea(obj, hdr->ioctl);
	if(!hdr->ioctl || !_fn)
		return -ENOTSUP;
	ssize_t (*fn)(twzobj *, int, long) = _fn;
	return fn(obj, req, arg);
}

struct event;
int twzio_poll(twzobj *obj, uint64_t type, struct event *event)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr)
		return -ENOTSUP;

	TRY_DEFAULT(poll, hdr, obj, type, event);

	void *_fn = twz_object_lea(obj, hdr->poll);
	if(!hdr->poll || !_fn)
		return -ENOTSUP;
	ssize_t (*fn)(twzobj *, uint64_t, struct event *) = _fn;
	return fn(obj, type, event);
}
