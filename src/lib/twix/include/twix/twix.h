#pragma once

#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TWIX_GATE_OPEN_QUEUE 1
static inline int twix_open_queue(struct secure_api *api, int flags, objid_t *_qid, objid_t *_bid)
{
	size_t ctx = 0;
	void *qid = twz_secure_api_alloc_stackarg(sizeof(*_qid), &ctx);
	void *bid = twz_secure_api_alloc_stackarg(sizeof(*_bid), &ctx);
	int r = twz_secure_api_call3(api, TWIX_GATE_OPEN_QUEUE, flags, qid, bid);
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
#ifdef __cplusplus
	twix_queue_entry(const twix_queue_entry &other)
	{
		qe.cmd_id = other.qe.cmd_id.load();
		qe.info = other.qe.info;
		cmd = other.cmd;
		flags = other.flags;
		arg0 = other.arg0;
		arg1 = other.arg1;
		arg2 = other.arg2;
		arg3 = other.arg3;
		arg4 = other.arg4;
		arg5 = other.arg5;
		buflen = other.buflen;
		ret = other.ret;
	}

	twix_queue_entry &operator=(const twix_queue_entry &other)
	{
		qe.cmd_id = other.qe.cmd_id.load();
		qe.info = other.qe.info;
		cmd = other.cmd;
		flags = other.flags;
		arg0 = other.arg0;
		arg1 = other.arg1;
		arg2 = other.arg2;
		arg3 = other.arg3;
		arg4 = other.arg4;
		arg5 = other.arg5;
		buflen = other.buflen;
		ret = other.ret;
		return *this;
	}

	twix_queue_entry() = default;
#endif
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

#define TWIX_FLAGS_CLONE_PROCESS 1

#define TWIX_FLAGS_EXIT_THREAD 1
#define TWIX_FLAGS_EXIT_SIGNAL 2
#define TWIX_FLAGS_EXIT_GROUP 4

#define TWIX_PRLIMIT_SET 1
#define TWIX_PRLIMIT_GET 2

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
	TWIX_CMD_CLONE,
	TWIX_CMD_SEND_SIGNAL,
	TWIX_CMD_WAIT_READY,
	TWIX_CMD_SIGDONE,
	TWIX_CMD_SUSPEND,
	TWIX_CMD_WAIT,
	TWIX_CMD_UNAME,
	TWIX_CMD_FACCESSAT,
	TWIX_CMD_DUP,
	TWIX_CMD_PRLIMIT,
	TWIX_CMD_EXEC,
	TWIX_CMD_POLL,
	NUM_TWIX_COMMANDS,
};

#define TWIX_POLL_TIMEOUT 1
#define TWIX_POLL_SIGMASK 2

#include <poll.h>
#include <signal.h>
#include <time.h>
struct twix_poll_info {
	struct timespec timeout;
	sigset_t sigmask;
	size_t nr_polls;
	struct pollfd polls[];
};

#define UNAME_LEN 1024
struct twix_uname_info {
	char sysname[UNAME_LEN];
	char nodename[UNAME_LEN];
	char release[UNAME_LEN];
	char machine[UNAME_LEN];
	char version[UNAME_LEN];
};

struct proc_info {
	int pid;
	int ppid;
	int uid;
	int gid;
	int euid;
	int egid;
	int pgid;
};

#ifdef __cplusplus
}
#endif
