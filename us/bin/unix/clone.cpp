#include "state.h"
#include <twix/twix.h>

long twix_cmd_clone(queue_client *client, twix_queue_entry *tqe)
{
	objid_t thrid = MKID(tqe->arg1, tqe->arg0);
	long flags = tqe->arg2;
	if(!(flags & TWIX_FLAGS_CLONE_PROCESS)) {
		return -ENOTSUP;
	}
	std::shared_ptr<unixprocess> proc = std::make_shared<unixprocess>(client->proc);
	client->proc->children.push_back(proc);
	procs_insert_forked(thrid, proc);
	fprintf(stderr, "forked! pid = %d\n", proc->pid);
	return proc->pid;
}
