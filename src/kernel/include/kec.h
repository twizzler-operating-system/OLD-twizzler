#pragma once

#define KEC_WRITE_DISCARD_ON_FULL 1
#define KEC_WRITE_EMERGENCY 2
#define KEC_ALLOWED_USER_WRITE_FLAGS KEC_WRITE_DISCARD_ON_FULL
#define KEC_ALLOWED_USER_READ_FLAGS KEC_READ_NONBLOCK

#define KEC_READ_NONBLOCK 1

#define KEC_BUFFER_SZ 16384
#define KEC_INPUT_BUFFER_SZ 4096
#include <lib/list.h>
#include <spinlock.h>
struct kec_buffer {
	_Atomic uint64_t state;
	uint8_t buffer[KEC_BUFFER_SZ];
	struct spinlock read_lock, write_lock;
	/* input buffer is not a ring buffer. it's not intended to be especially fast. */
	size_t read_pos;
	uint8_t read_buffer[KEC_INPUT_BUFFER_SZ];
	struct list readblocked;

	void (*write)(const void *, size_t, int);
};
ssize_t kec_read_buffer(void *_buffer, size_t len, int flags);
ssize_t kec_read(void *buffer, size_t len, int flags);
ssize_t kec_write_buffer(const void *_buffer, size_t len, int flags);
void kec_init(void);
void kec_write(const void *buffer, size_t len, int flags);
void kec_add_to_read_buffer(void *buffer, size_t len);
void machine_kec_init(struct kec_buffer *);
ssize_t syscall_kec_read(void *buffer, size_t len, int flags);
long syscall_kec_write(const void *buffer, size_t len, int flags);
