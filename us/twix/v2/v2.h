#pragma once

#include <stddef.h>
#include <stdint.h>

#include <twix/twix.h>

struct twix_register_frame;
struct syscall_args {
	long a0, a1, a2, a3, a4, a5;
	long num;
	struct twix_register_frame *frame;
};

void extract_bufdata(void *ptr, size_t len, size_t off);
void write_bufdata(const void *ptr, size_t len, size_t off);
struct twix_queue_entry build_tqe(enum twix_command cmd, int flags, size_t bufsz, int nr_va, ...);
void twix_sync_command(struct twix_queue_entry *tqe);
