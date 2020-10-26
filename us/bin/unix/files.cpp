#include <twix/twix.h>
#include <twz/obj.h>

#include "state.h"

int filedesc::init_path(const char *path, int _fcntl_flags)
{
	int r = twz_object_init_name(&obj, path, FE_READ | FE_WRITE);
	if(r) {
		return r;
	}
	objid = twz_object_guid(&obj);
	fcntl_flags = _fcntl_flags;
	return 0;
}

/* buf: path, a0: flags, a1: mode (create) */
long cmd_open(queue_client *client, twix_queue_entry *tqe)
{
	return 0;
}
