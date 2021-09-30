#pragma once

#include <stddef.h>
#include <stdio.h>

#include <twz/_types.h>

struct __twzobj;
typedef struct __twzobj twzobj;

#ifdef __cplusplus
extern "C" {
#endif

enum twzio_type {
	UNKNOWN = 0,
	BSTREAM = 1,
	PTY_CLIENT = 2,
	PTY_SERVER = 3,
};

struct event;
struct twzio_hdr {
	uint32_t resv;
	uint32_t type;
	ssize_t (*read)(twzobj *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(twzobj *, const void *, size_t len, size_t off, unsigned flags);
	int (*ioctl)(twzobj *, int request, long);
	int (*poll)(twzobj *, uint64_t type, struct event *event);
};

#define TWZIO_METAEXT_TAG 0x0000000010101010

#define TWZIO_EVENT_READ 1
#define TWZIO_EVENT_WRITE 2
#define TWZIO_EVENT_IOCTL 4

ssize_t twzio_read(twzobj *obj, void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_write(twzobj *obj, const void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_ioctl(twzobj *obj, int req, ...);
int twzio_poll(twzobj *, uint64_t, struct event *);

#define TWZIO_NONBLOCK 1

#ifdef __cplusplus
}
#endif
