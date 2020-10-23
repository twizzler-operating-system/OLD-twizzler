#include <stdbool.h>
#include <twix/twix.h>
#include <twz/debug.h>

#include "../syscalls.h"

struct unix_server {
	twzobj cmdqueue, buffer;
	struct secure_api api;
	_Atomic bool inited;
	_Atomic bool ok;
};

static struct unix_server userver;

void twix_sync_command(struct twix_queue_entry *tqe)
{
	queue_submit(&userver.cmdqueue, (struct queue_entry *)tqe, 0);
	queue_get_finished(&userver.cmdqueue, (struct queue_entry *)tqe, 0);
}

struct twix_queue_entry build_tqe(enum twix_command cmd, int flags, size_t bufsz, ...)
{
	int nr_va = 0;
	switch(cmd) {
		case TWIX_CMD_GET_PROC_INFO:
			nr_va = 0;
			break;
	}
	long args[6] = {};
	va_list va;
	va_start(va, bufsz);
	for(int i = 0; i < nr_va; i++) {
		args[i] = va_arg(va, long);
	}
	va_end(va);

	struct twix_queue_entry tqe = {
		.cmd = cmd,
		.arg0 = args[0],
		.arg1 = args[1],
		.arg2 = args[2],
		.arg3 = args[3],
		.arg4 = args[4],
		.arg5 = args[5],
		.buflen = bufsz,
		.flags = flags,
	};

	return tqe;
}

void extract_bufdata(void *ptr, size_t len)
{
	void *base = twz_object_base(&userver.buffer);
	memcpy(ptr, base, len);
}

void write_bufdata(void *ptr, size_t len)
{
	void *base = twz_object_base(&userver.buffer);
	memcpy(base, ptr, len);
}

long get_proc_info(struct proc_info *info)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GET_PROC_INFO, 0, sizeof(struct proc_info));
	twix_sync_command(&tqe);
	extract_bufdata(info, sizeof(*info));

	return 0;
}

static bool setup_queue(void)
{
	if(userver.inited)
		return userver.ok;
	userver.ok = false;
	userver.inited = true;
	if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
		return false;
	}
	objid_t qid, bid;
	int r = twix_open_queue(&userver.api, 0, &qid, &bid);
	if(r) {
		return false;
	}

	if(twz_object_init_guid(&userver.cmdqueue, qid, FE_READ | FE_WRITE))
		return false;

	if(twz_object_init_guid(&userver.buffer, bid, FE_READ | FE_WRITE))
		return false;

	userver.ok = true;
	userver.inited = true;

	struct proc_info info = {};
	get_proc_info(&info);
	return true;
}

bool try_twix_version2(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5,
  long *ret)
{
	(void)frame;
	(void)num;
	(void)a0;
	(void)a1;
	(void)a2;
	(void)a3;
	(void)a4;
	(void)a5;
	(void)ret;
	setup_queue();
	return false;
}
