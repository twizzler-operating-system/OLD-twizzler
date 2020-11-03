#include <twix/twix.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/persist.h>

#include "state.h"

#include <dirent.h>

struct linux_dirent64 {
	uint64_t d_ino;          /* 64-bit inode number */
	uint64_t d_off;          /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char d_type;    /* File type */
	char d_name[];           /* Filename (null-terminated) */
};

static struct linux_dirent64 *__copy_to_dirp(struct linux_dirent64 *dirp,
  struct twz_name_ent *ent,
  ssize_t avail,
  size_t pos_next,
  ssize_t *sz)
{
	*sz = (sizeof(*dirp) + strlen(ent->name) + 1 + 15) & ~15;
	if(*sz > avail) {
		return NULL;
	}
	dirp->d_off = pos_next;
	dirp->d_ino = 1;
	dirp->d_reclen = *sz;
	switch(ent->type) {
		case NAME_ENT_NAMESPACE:
			dirp->d_type = DT_DIR;
			break;
		case NAME_ENT_REGULAR:
			dirp->d_type = DT_REG;
			break;
		default:
			dirp->d_type = DT_UNKNOWN;
			break;
	}
	strcpy(dirp->d_name, ent->name);
	return (struct linux_dirent64 *)((char *)dirp + *sz);
}

long twix_cmd_getdents(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	size_t count = tqe->buflen;
	struct linux_dirent64 *dirp = (struct linux_dirent64 *)client->buffer_base();

	auto desc = client->proc->get_file(fd);
	if(desc == nullptr) {
		return -EBADF;
	}

	size_t bytes = 0;
	/* TODO: lock desc */
	while(true) {
		struct twz_name_ent *ent;
		ssize_t sz_h;
		sz_h = twz_hier_get_entry(&desc->obj, desc->pos, &ent);
		if(sz_h == 0) {
			return bytes;
		} else if(sz_h < 0) {
			return sz_h;
		}

		if(ent->flags & NAME_ENT_VALID) {
			ssize_t sz_l;
			dirp = __copy_to_dirp(dirp, ent, count, desc->pos + sz_h, &sz_l);
			if(!dirp) {
				return bytes;
			}
			bytes += sz_l;
			count -= sz_l;
		}
		desc->pos += sz_h;
	}
}

long twix_cmd_readlink(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	size_t bufsz = tqe->arg1;
	if(bufsz > tqe->buflen) {
		return -EINVAL;
	}
	auto at = fd >= 0 ? client->proc->get_file(fd) : client->proc->cwd;
	auto [ok, path] = client->buffer_to_string(tqe->buflen - bufsz);

	char *buf = (char *)client->buffer_base() + (tqe->buflen - bufsz);
	return twz_hier_readlink(&at->obj, path.c_str(), buf, bufsz);
}
