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

long hook_fork(struct syscall_args *args);
void resetup_queue(long);

uint32_t get_new_twix_conn_id(void);
void release_twix_conn_id(uint32_t id);
struct twix_conn *get_twix_conn(void);
__attribute__((const)) uint32_t get_twix_thr_id(void);
bool setup_queue(void);
void __twix_signal_handler(int fault, void *data, void *userdata);
void check_signals(struct twix_conn *conn);

struct twix_conn {
	twzobj cmdqueue, buffer;
	objid_t qid, bid;
	_Atomic size_t info;

	_Atomic int block_count;
	_Atomic size_t pending_count;
	struct {
		_Atomic long flags;
		long args[3];
	} pending_sigs[];
};
