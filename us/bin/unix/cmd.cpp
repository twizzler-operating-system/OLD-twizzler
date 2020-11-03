#include <twix/twix.h>

#include "state.h"

#include <stdio.h>

static long __get_proc_info(queue_client *client, twix_queue_entry *tqe)
{
	if(tqe->buflen != sizeof(proc_info))
		return -EINVAL;
	struct proc_info pi = {
		.pid = client->proc->pid,
		.ppid = client->proc->ppid,
		.uid = client->proc->uid,
		.gid = client->proc->gid,
	};
	client->write_buffer(&pi);
	return 0;
}

long twix_cmd_open(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_pio(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_fcntl(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_mmap(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_stat(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_getdents(queue_client *client, twix_queue_entry *tqe);
long twix_cmd_readlink(queue_client *client, twix_queue_entry *tqe);

long twix_cmd_close(queue_client *client, twix_queue_entry *tqe)
{
	return 0;
}

long twix_cmd_exit(queue_client *client, twix_queue_entry *tqe)
{
	return 0;
}

static long __reopen_v1_fd(queue_client *client, twix_queue_entry *tqe)
{
	objid_t objid = MKID(tqe->arg2, tqe->arg1);
	int fd = tqe->arg0;
	int fcntl_flags = tqe->arg3;
	size_t pos = tqe->arg4;
	auto desc = std::make_shared<filedesc>(objid, pos, fcntl_flags);
	client->proc->steal_fd(fd, desc, 0);
	return 0;
}

static long (*call_table[NUM_TWIX_COMMANDS])(queue_client *, twix_queue_entry *tqe) = {
	[TWIX_CMD_GET_PROC_INFO] = __get_proc_info,
	[TWIX_CMD_REOPEN_V1_FD] = __reopen_v1_fd,
	[TWIX_CMD_OPENAT] = twix_cmd_open,
	[TWIX_CMD_PIO] = twix_cmd_pio,
	[TWIX_CMD_FCNTL] = twix_cmd_fcntl,
	[TWIX_CMD_STAT] = twix_cmd_stat,
	[TWIX_CMD_MMAP] = twix_cmd_mmap,
	[TWIX_CMD_EXIT] = twix_cmd_exit,
	[TWIX_CMD_CLOSE] = twix_cmd_close,
	[TWIX_CMD_GETDENTS] = twix_cmd_getdents,
	[TWIX_CMD_READLINK] = twix_cmd_readlink,
};

static const char *cmd_strs[] = {
	[TWIX_CMD_GET_PROC_INFO] = "get_proc_info",
	[TWIX_CMD_REOPEN_V1_FD] = "reopen_v1_fd",
	[TWIX_CMD_OPENAT] = "openat",
	[TWIX_CMD_PIO] = "pio",
	[TWIX_CMD_FCNTL] = "fcntl",
	[TWIX_CMD_STAT] = "fstatat",
	[TWIX_CMD_MMAP] = "mmap",
	[TWIX_CMD_EXIT] = "exit",
	[TWIX_CMD_CLOSE] = "close",
	[TWIX_CMD_GETDENTS] = "getdents",
	[TWIX_CMD_READLINK] = "readlink",
};

long queue_client::handle_command(twix_queue_entry *tqe)
{
	if(tqe->cmd >= 0 && tqe->cmd < NUM_TWIX_COMMANDS && call_table[tqe->cmd]) {
#if 0
		fprintf(stderr,
		  "[twix-server] got command from proc %d, thr %d: %d (%s)\n",
		  proc->pid,
		  thr->tid,
		  tqe->cmd,
		  cmd_strs[tqe->cmd]);
#endif
		long ret = call_table[tqe->cmd](this, tqe);
		// fprintf(stderr, "[twix-server]     return %ld\n", ret);
		return ret;
	}
	return -ENOSYS;
}
