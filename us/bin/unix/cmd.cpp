#include "state.h"
#include <twix/twix.h>

#include <stdio.h>

static std::pair<long, bool> __get_proc_info(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe)
{
	if(tqe->buflen != sizeof(proc_info))
		return R_S(-EINVAL);
	struct proc_info pi = {
		.pid = client->proc->pid,
		.ppid = client->proc->parent == nullptr ? 0 : client->proc->parent->pid,
		.uid = client->proc->uid,
		.gid = client->proc->gid,
	};
	client->write_buffer(&pi);
	return R_S(0);
}

std::pair<long, bool> twix_cmd_send_signal(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe)
{
	int pid = tqe->arg0;
	int sig = tqe->arg1;

	if(pid <= 0) {
		return R_S(-ENOTSUP);
	}

	auto proc = process_lookup(pid);
	if(!proc) {
		return R_S(-ESRCH);
	}
	/* TODO: check permission */

	proc->send_signal(sig);

	return R_S(0);
}

#include <future>

static void __wait_ready(std::shared_ptr<unixprocess> wp,
  twix_queue_entry *tqe,
  std::shared_ptr<queue_client> client)
{
	fprintf(stderr, "_wait_ready\n");
	wp->wait_ready();
	queue_complete(&client->queue, (struct queue_entry *)tqe, 0);
	delete tqe;
	fprintf(stderr, "_wait_ready: done\n");
}

std::pair<long, bool> twix_cmd_wait_ready(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe)
{
	int pid = tqe->arg0;
	auto proc = process_lookup(pid);
	if(!proc) {
		std::lock_guard<std::mutex> _lg(client->proc->lock);
		for(auto p : client->proc->children) {
			if(p->pid == pid) {
				proc = p;
				break;
			}
		}
	}

	if(!proc) {
		return R_S(-ESRCH);
	}
	std::lock_guard<std::mutex> _lg(proc->lock);
	if(proc->state != PROC_FORKED) {
		return R_S(0);
	}

	return R_S(0);
	fprintf(stderr, "WaitReady\n");
	twix_queue_entry *nt = new twix_queue_entry();
	memcpy(nt, tqe, sizeof(*nt));

	std::async(std::launch::async, __wait_ready, proc, nt, client);
	return R_A(-EAGAIN);
}

std::pair<long, bool> twix_cmd_close(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	return R_S(0);
}

std::pair<long, bool> twix_cmd_exit(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	client->proc->exit_status = (tqe->arg0 & 0xff) << 8;
	return R_S(0);
}

static std::pair<long, bool> __reopen_v1_fd(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe)
{
	objid_t objid = MKID(tqe->arg2, tqe->arg1);
	int fd = tqe->arg0;
	int fcntl_flags = tqe->arg3;
	size_t pos = tqe->arg4;
	auto desc = std::make_shared<filedesc>(objid, pos, fcntl_flags);
	client->proc->steal_fd(fd, desc, 0);
	return R_S(0);
}

static std::pair<long, bool> (
  *call_table[NUM_TWIX_COMMANDS])(std::shared_ptr<queue_client>, twix_queue_entry *tqe) = {
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
	[TWIX_CMD_CLONE] = twix_cmd_clone,
	[TWIX_CMD_SEND_SIGNAL] = twix_cmd_send_signal,
	[TWIX_CMD_WAIT_READY] = twix_cmd_wait_ready,
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
	[TWIX_CMD_CLONE] = "clone",
	[TWIX_CMD_SEND_SIGNAL] = "send_signal",
};

std::pair<long, bool> handle_command(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
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
		auto ret = call_table[tqe->cmd](client, tqe);
		// if(tqe->cmd == TWIX_CMD_WAIT_READY && ret == -EAGAIN) {
		//	return std::make_pair(ret, false);
		//}
		// fprintf(stderr, "[twix-server]     return %ld\n", ret);
		// return std::make_pair(ret, true);
		return ret;
	}
	return std::make_pair(-ENOSYS, true);
}
