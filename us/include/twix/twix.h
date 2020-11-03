#pragma once

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/security.h>

#define TWIX_GATE_OPEN_QUEUE 1
static inline int twix_open_queue(struct secure_api *api, int flags, objid_t *_qid, objid_t *_bid)
{
	size_t ctx = 0;
	void *qid = twz_secure_api_alloc_stackarg(sizeof(*_qid), &ctx);
	void *bid = twz_secure_api_alloc_stackarg(sizeof(*_bid), &ctx);
	int r = twz_secure_api_call3(api->hdr, TWIX_GATE_OPEN_QUEUE, flags, qid, bid);
	*_qid = *(objid_t *)qid;
	*_bid = *(objid_t *)bid;
	return r;
}

struct twix_queue_entry {
	struct queue_entry qe;
	int cmd;
	int flags;
	long arg0, arg1, arg2, arg3, arg4, arg5;
	long buflen;
	long ret;
};

struct unix_repr {
	int pid;
	int uid;
	int gid;
	int euid;
	int egid;
	int pgid;
	int sid;
	int tid;
};

#define TWIX_FLAGS_PIO_WRITE 1
#define TWIX_FLAGS_PIO_POS 2

enum twix_command {
	TWIX_CMD_GET_PROC_INFO,
	TWIX_CMD_REOPEN_V1_FD,
	TWIX_CMD_OPENAT,
	TWIX_CMD_PIO,
	TWIX_CMD_FCNTL,
	TWIX_CMD_STAT,
	TWIX_CMD_MMAP,
	TWIX_CMD_EXIT,
	TWIX_CMD_CLOSE,
	TWIX_CMD_GETDENTS,
	TWIX_CMD_READLINK,
	NUM_TWIX_COMMANDS,
};

struct proc_info {
	int pid;
	int ppid;
	int uid;
	int gid;
};
