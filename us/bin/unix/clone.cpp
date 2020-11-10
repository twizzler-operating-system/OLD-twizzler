#include "state.h"
#include <twix/twix.h>

std::pair<long, bool> twix_cmd_clone(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	objid_t thrid = MKID(tqe->arg1, tqe->arg0);
	long flags = tqe->arg2;
	if(!(flags & TWIX_FLAGS_CLONE_PROCESS)) {
		return R_S(-ENOTSUP);
	}
	std::shared_ptr<unixprocess> proc = std::make_shared<unixprocess>(client->proc);
	client->proc->children.push_back(proc);
	procs_insert_forked(thrid, proc);
	return R_S(proc->pid);
}
