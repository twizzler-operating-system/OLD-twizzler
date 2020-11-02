#include <twix/twix.h>
#include <twz/io.h>
#include <twz/obj.h>
#include <twz/persist.h>

#define RWF_NOWAIT 0x8

#include "state.h"

int filedesc::init_path(const char *path, int _fcntl_flags, int mode)
{
	int r = twz_object_init_name(&obj, path, FE_READ | FE_WRITE);
	if(r) {
		return r;
	}
	objid = twz_object_guid(&obj);
	fcntl_flags = _fcntl_flags;
	return 0;
}

/* buf: path, a0: dirfd, a1: flags, a2: mode (create) */
long twix_cmd_open(queue_client *client, twix_queue_entry *tqe)
{
	int mode = tqe->arg2;
	int flags = tqe->arg1 + 1;
	int dirfd = tqe->arg0;
	auto [ok, path] = client->buffer_to_string(tqe->buflen);

	if(dirfd != -1) {
		return -ENOSYS;
	}
	auto desc = std::make_shared<filedesc>();
	int r;
	if((r = desc->init_path(path.c_str(), flags, mode))) {
		return r;
	}

	int fd = client->proc->assign_fd(desc, 0);
	fprintf(stderr, "OPEN : %d %s -> %d\n", ok, path.c_str(), fd);
	return fd;
}

ssize_t filedesc::write(const void *buffer, size_t buflen, off_t offset, int flags, bool use_pos)
{
	/* TODO: atomicity */
	bool nonblock = (flags & RWF_NOWAIT) || (fcntl_flags & O_NONBLOCK);
	if(use_pos) {
		offset = pos;
	}
	ssize_t r = twzio_write(&obj, buffer, buflen, offset, nonblock ? TWZIO_NONBLOCK : 0);
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
long twix_cmd_pio(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	off_t off = tqe->arg1;
	int preadv_flags = tqe->arg2;
	int flags = tqe->arg3;

	// fprintf(stderr, "PIO %d %ld %d %d\n", fd, off, preadv_flags, flags);

	bool writing = flags & TWIX_FLAGS_PIO_WRITE;

	auto filedesc = client->proc->get_file(fd);
	if(filedesc == nullptr) {
		return -EBADF;
	}
	if(!filedesc->access(writing ? W_OK : R_OK)) {
		return -EACCES;
	}

	ssize_t ret;
	if(writing) {
		ret = filedesc->write(
		  client->buffer_base(), tqe->buflen, off, preadv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	} else {
		ret = filedesc->read(
		  client->buffer_base(), tqe->buflen, off, preadv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	}

	return ret;
}

long twix_cmd_fcntl(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int cmd = tqe->arg1;
	int arg = tqe->arg2;

	switch(cmd) {
		case F_GETFD:
			return client->proc->get_file_flags(fd);
			break;
		case F_SETFD:
			return client->proc->set_file_flags(fd, arg);
			break;
		case F_GETFL: {
			auto desc = client->proc->get_file(fd);
			if(desc == nullptr)
				return -EBADF;
			return desc->fcntl_flags - 1;
		} break;
		case F_SETFL: {
			auto desc = client->proc->get_file(fd);
			if(desc == nullptr)
				return -EBADF;
			desc->fcntl_flags = arg & ~(O_ACCMODE);
			return 0;
		} break;
		default:
			return -EINVAL;
	}
}

#include <sys/stat.h>
long twix_cmd_stat(queue_client *client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int flags = tqe->arg1;
	auto desc = client->proc->get_file(fd);
	if(!desc) {
		return -EBADF;
	}
	auto [ok, path] = client->buffer_to_string(tqe->buflen);
	fprintf(stderr, "STAT %d %d: '%s'\n", fd, flags, path.c_str());

	if(path.size() > 0) {
		return -ENOTSUP; // TODO
	}

	struct metainfo *mi = twz_object_meta(&desc->obj);

	struct stat st = (struct stat){
		.st_dev = 0,
		.st_ino = 11,
		.st_nlink = 1,

		.st_mode = 0777,
		.st_uid = 0,
		.st_gid = 0,
		.__pad0 = 0,
		.st_rdev = 0,
		.st_size = mi->sz,
		.st_blksize = 0,
		.st_blocks = 0,

		.st_atim = {},
		.st_mtim = {},
		.st_ctim = {},
		.__unused = {},
	};

	client->write_buffer(&st, tqe->buflen);

	return 0;
}
