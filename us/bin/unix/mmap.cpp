#include <twix/twix.h>
#include <twz/io.h>
#include <twz/obj.h>
#include <twz/persist.h>

#include "state.h"

#include <sys/mman.h>

long twix_cmd_mmap(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int prot = tqe->arg1;
	int flags = tqe->arg2;
	off_t offset = tqe->arg3;
	off_t map_offset = tqe->arg4;

	bool shared;
	fprintf(stderr, "MMAP %d %d %d %ld %ld\n", fd, prot, flags, offset, map_offset);
	if(flags & MAP_PRIVATE) {
		fprintf(stderr, "    private\n");
		shared = false;
	} else if(flags & MAP_SHARED) {
		fprintf(stderr, "    shared\n");
		shared = true;
	} else {
		return -EINVAL;
	}

	if(flags & MAP_FIXED) {
		fprintf(stderr, "    fixed\n");
		if(map_offset == 0 || (map_offset & (OBJ_NULLPAGE_SIZE - 1))
		   || (size_t)map_offset > OBJ_TOPDATA) {
			return -EINVAL;
		}
	} else {
		map_offset = 0;
	}

	objid_t id;
	if(fd >= 0) {
		auto desc = client->proc->get_file(fd);
		if(!desc) {
			return -EBADF;
		}
		id = desc->objid;
		if(!shared) {
			/* TODO: create a new object */
			//	int r = twz_object_create(
			//	  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_TIED_NONE, 0, id,
			//&id); 	if(r) 		return r;
		}
	} else {
		int r = twz_object_create(
		  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE,
		  0,
		  0,
		  &id);
		if(r)
			return r;
	}

	client->write_buffer(&id);

	return 0;
}
