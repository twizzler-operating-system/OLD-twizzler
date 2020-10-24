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

static long (*call_table[NUM_TWIX_COMMANDS])(queue_client *, twix_queue_entry *tqe) = {
	[TWIX_CMD_GET_PROC_INFO] = __get_proc_info,
};

long queue_client::handle_command(twix_queue_entry *tqe)
{
	fprintf(stderr, "[twix-server] got command from proc %d, thr %d\n", proc->pid, thr->tid);
	if(tqe->cmd >= 0 && tqe->cmd < NUM_TWIX_COMMANDS) {
		return call_table[tqe->cmd](this, tqe);
	}
	return -ENOSYS;
}
