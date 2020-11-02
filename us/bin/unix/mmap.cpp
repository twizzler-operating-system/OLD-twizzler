#include <twix/twix.h>
#include <twz/io.h>
#include <twz/obj.h>
#include <twz/persist.h>

#include "state.h"

long twix_cmd_mmap(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int prot = tqe->arg1;
	int flags = tqe->arg2;
	off_t offset = tqe->arg3;
	off_t map_offset = tqe->arg4;

	fprintf(stderr, "MMAP %d %d %d %ld %ld\n", fd, prot, flags, offset, map_offset);
	return -ENOSYS;
}
