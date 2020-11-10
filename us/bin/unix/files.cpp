#include <twix/twix.h>
#include <twz/io.h>
#include <twz/obj.h>
#include <twz/persist.h>

#define RWF_NOWAIT 0x8

#include "state.h"

#include <twz/name.h>

int filedesc::init_path(std::shared_ptr<filedesc> at,
  const char *path,
  int _fcntl_flags,
  int mode,
  int flags)
{
	if(at == nullptr) {
		/* TODO: get root from process */
		int r = twz_hier_resolve_name(twz_name_get_root(), path, flags, &dirent);
		if(r) {
			return r;
		}
		if(dirent.type != NAME_ENT_SYMLINK) {
			r = twz_object_init_guid(&obj, dirent.id, FE_READ | FE_WRITE);
			if(r) {
				return r;
			}
			has_obj = true;
		}
	} else {
		/* TODO: check if at is a directory */
		int r = twz_hier_resolve_name(&at->obj, path, flags, &dirent);
		if(r)
			return r;
		if(dirent.type != NAME_ENT_SYMLINK) {
			if((r = twz_object_init_guid(&obj, dirent.id, FE_READ | FE_WRITE))) {
				return r;
			}
			has_obj = true;
		}
	}
	if(has_obj) {
		objid = twz_object_guid(&obj);
	}
	fcntl_flags = _fcntl_flags;
	inited = true;
	return 0;
}

std::pair<int, std::shared_ptr<filedesc>> open_file(std::shared_ptr<filedesc> at,
  const char *path,
  int flags)
{
	if(path == NULL || path[0] == 0) {
		return std::make_pair(at != nullptr, at);
	}
	auto desc = std::make_shared<filedesc>();
	int r = desc->init_path(at, path, O_RDWR, 0, flags);
	return std::make_pair(r, desc);
}

/* buf: path, a0: dirfd, a1: flags, a2: mode (create) */
std::pair<long, bool> twix_cmd_open(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int mode = tqe->arg2;
	int flags = tqe->arg1 + 1;
	int dirfd = tqe->arg0;
	auto [ok, path] = client->buffer_to_string(tqe->buflen);

	auto at = client->proc->cwd;
	if(dirfd >= 0) {
		at = client->proc->get_file(dirfd);
		if(at == nullptr) {
			return R_S(-EBADF);
		}
	}
	auto desc = std::make_shared<filedesc>();
	int r;
	if((r = desc->init_path(at, path.c_str(), flags, mode))) {
		return R_S(r);
	}

	int fd = client->proc->assign_fd(desc, 0);
	// fprintf(stderr, "OPEN : %d %s -> %d\n", ok, path.c_str(), fd);
	return R_S(fd);
}

ssize_t filedesc::write(const void *buffer, size_t buflen, off_t offset, int flags, bool use_pos)
{
	/* TODO: atomicity */
	bool nonblock = (flags & RWF_NOWAIT) || (fcntl_flags & O_NONBLOCK);
	if(use_pos) {
		offset = pos;
	}
	ssize_t r = twzio_write(&obj, buffer, buflen, offset, nonblock ? TWZIO_NONBLOCK : 600);
	if(r == -ENOTSUP) {
		/* TODO: bounds check */
		memcpy((char *)twz_object_base(&obj) + offset, buffer, buflen);
		_clwb_len((char *)twz_object_base(&obj) + offset, buflen);
		_pfence();
		r = buflen;
		struct metainfo *mi = twz_object_meta(&obj);
		/* TODO: append */
		if(mi->flags & MIF_SZ) {
			if(offset + buflen > mi->sz) {
				mi->sz = offset + buflen;
			}
		}
		if(use_pos)
			pos += r;
	} else if(r > 0) {
		if(use_pos)
			pos += r;
	}
	return r;
}

ssize_t filedesc::read(void *buffer, size_t buflen, off_t offset, int flags, bool use_pos)
{
	/* TODO: atomicity */
	bool nonblock = (flags & RWF_NOWAIT) || (fcntl_flags & O_NONBLOCK);
	if(use_pos) {
		offset = pos;
	}
	ssize_t r = twzio_read(&obj, buffer, buflen, offset, nonblock ? TWZIO_NONBLOCK : 0);
	if(r == -ENOTSUP) {
		struct metainfo *mi = twz_object_meta(&obj);
		if(mi->flags & MIF_SZ) {
			if((size_t)offset >= mi->sz) {
				return 0;
			}
			if(offset + buflen > mi->sz) {
				buflen = mi->sz - offset;
			}
		} else {
			return -EIO;
		}
		memcpy(buffer, (char *)twz_object_base(&obj) + offset, buflen);
		r = buflen;
		if(use_pos)
			pos += r;
	} else if(r > 0) {
		if(use_pos)
			pos += r;
	}
	return r;
}

/* buf: data buffer, buflen: data buffer length *
 * a0: fd, a1: offset, a2: flags, a3: flags2 */
std::pair<long, bool> twix_cmd_pio(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	off_t off = tqe->arg1;
	int preadv_flags = tqe->arg2;
	int flags = tqe->arg3;

	// fprintf(stderr, "PIO %d %ld %d %d\n", fd, off, preadv_flags, flags);

	bool writing = flags & TWIX_FLAGS_PIO_WRITE;

	auto filedesc = client->proc->get_file(fd);
	if(filedesc == nullptr) {
		return R_S(-EBADF);
	}
	if(!filedesc->access(writing ? W_OK : R_OK)) {
		return R_S(-EACCES);
	}

	ssize_t ret;
	if(writing) {
		ret = filedesc->write(
		  client->buffer_base(), tqe->buflen, off, preadv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	} else {
		ret = filedesc->read(
		  client->buffer_base(), tqe->buflen, off, preadv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	}

	return R_S(ret);
}

std::pair<long, bool> twix_cmd_fcntl(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int cmd = tqe->arg1;
	int arg = tqe->arg2;

	long ret = 0;
	switch(cmd) {
		case F_GETFD:
			ret = client->proc->get_file_flags(fd);
			break;
		case F_SETFD:
			ret = client->proc->set_file_flags(fd, arg);
			break;
		case F_GETFL: {
			auto desc = client->proc->get_file(fd);
			if(desc == nullptr)
				ret = -EBADF;
			else
				ret = desc->fcntl_flags - 1;
		} break;
		case F_SETFL: {
			auto desc = client->proc->get_file(fd);
			if(desc == nullptr)
				ret = -EBADF;
			else
				desc->fcntl_flags = arg & ~(O_ACCMODE);
		} break;
		default:
			ret = -EINVAL;
	}
	return R_S(ret);
}

#include <sys/stat.h>
std::pair<long, bool> twix_cmd_stat(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int flags = tqe->arg1;
	auto at = fd >= 0 ? client->proc->get_file(fd) : client->proc->cwd;
	if(!at) {
		return R_S(-EBADF);
	}

	auto [ok, path] = client->buffer_to_string(tqe->buflen);
	// fprintf(stderr, "STAT %d %d: '%s'\n", fd, flags, path.c_str());

	auto [r, desc] =
	  open_file(at, ok ? path.c_str() : NULL, (flags & AT_SYMLINK_NOFOLLOW) ? TWZ_HIER_SYM : 0);
	if(r) {
		return R_S(r);
	}
	size_t sz = 255;
	if(desc->has_obj) {
		struct metainfo *mi = twz_object_meta(&desc->obj);
		sz = mi->sz;
	}
	struct stat st = (struct stat){
		.st_dev = ID_HI(twz_object_guid(&desc->obj)),
		.st_ino = ID_LO(twz_object_guid(&desc->obj)),
		.st_nlink = 1,

		.st_mode = 0777,
		.st_uid = 0,
		.st_gid = 0,
		.__pad0 = 0,
		.st_rdev = 0,
		.st_size = sz,
		.st_blksize = 0,
		.st_blocks = 0,

		.st_atim = {},
		.st_mtim = {},
		.st_ctim = {},
		.__unused = {},
	};

	if(desc->dirent.type == NAME_ENT_NAMESPACE) {
		st.st_mode |= S_IFDIR;
	} else if(desc->dirent.type == NAME_ENT_SYMLINK) {
		st.st_mode |= S_IFLNK;
	} else {
		st.st_mode |= S_IFREG;
	}

	client->write_buffer(&st, tqe->buflen);

	return R_S(0);
}
