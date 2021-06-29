#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/obj/io.h>
#include <twz/persist.h>
#include <twz/sys/obj.h>

#include "state.h"

#include <sys/mman.h>

std::pair<long, bool> twix_cmd_mmap(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int prot = tqe->arg1;
	int flags = tqe->arg2;
	off_t offset = tqe->arg3;
	off_t map_offset = tqe->arg4;
	size_t length = tqe->arg5;

	bool shared;
	// fprintf(stderr, "MMAP %d %d %d %ld %ld %lx\n", fd, prot, flags, offset, map_offset, length);
	if(flags & MAP_PRIVATE) {
		//	fprintf(stderr, "    private\n");
		shared = false;
	} else if(flags & MAP_SHARED) {
		//	fprintf(stderr, "    shared\n");
		shared = true;
	} else {
		return R_S(-EINVAL);
	}

	if(flags & MAP_FIXED) {
		//	fprintf(stderr, "    fixed\n");
		if(map_offset == 0 || (map_offset & (OBJ_NULLPAGE_SIZE - 1))
		   || (size_t)map_offset > OBJ_TOPDATA) {
			return R_S(-EINVAL);
		}
	} else {
		map_offset = OBJ_NULLPAGE_SIZE;
	}

	objid_t id;
	int r;
	if(fd >= 0) {
		auto desc = client->proc->get_file(fd);
		if(!desc) {
			return R_S(-EBADF);
		}
		struct metainfo *mi = twz_object_meta(&desc->obj);
		if(mi->flags & MIF_SZ) {
			if(length > mi->sz) {
				length = mi->sz;
			}
		}

		if(!shared) {
			int r = twz_object_create(
			  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_TIED_NONE, 0, 0, &id);
			if(r)
				return R_S(r);
			client->proc->objects_to_delete.push_back(id);
		} else {
			return R_S(-ENOTSUP);
		}

#if 0
		fprintf(stderr,
		  "    copying: %lx -> %lx for %lx\n",
		  map_offset,
		  offset + OBJ_NULLPAGE_SIZE,
		  length);
#endif
		/* TODO: verify offsets */
		if((r = sys_ocopy(id,
		      desc->objid,
		      map_offset,
		      offset + OBJ_NULLPAGE_SIZE,
		      (length + 0xfff) & ~0xfff,
		      0))) {
			return R_S(r);
		}

	} else {
		if(!(flags & MAP_ANON))
			return R_S(-EINVAL);
		// fprintf(stderr, "    anon\n");
		r = twz_object_create(
		  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_TIED_NONE, 0, 0, &id);
		if(r)
			return R_S(r);
		client->proc->objects_to_delete.push_back(id);
	}

	client->write_buffer(&id);

	return R_S(0);
}
