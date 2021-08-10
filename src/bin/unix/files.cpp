#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/obj/io.h>
#include <twz/persist.h>
#include <twz/sys/name.h>

#define RWF_NOWAIT 0x8

#include "async.h"
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
		struct twz_name_ent dirent;
		int r = twz_hier_resolve_name(twz_name_get_root(), path, flags, &dirent);
		if(r) {
			return r;
		}
		d_type = dirent.type;
		d_name = std::string(dirent.name);
		if(dirent.type != NAME_ENT_SYMLINK) {
			r = twz_object_init_guid(&obj, dirent.id, FE_READ | FE_WRITE);
			if(r) {
				return r;
			}
			has_obj = true;
		}
	} else {
		/* TODO: check if at is a directory */
		struct twz_name_ent dirent;
		int r = twz_hier_resolve_name(&at->obj, path, flags, &dirent);
		if(r)
			return r;
		d_type = dirent.type;
		d_name = std::string(dirent.name);
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
	std::shared_ptr<filedesc> desc = std::make_shared<filedesc>();
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
	if(use_pos) {
		offset = pos;
	}
	ssize_t r = twzio_write(&obj, buffer, buflen, offset, TWZIO_NONBLOCK);
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

bool filedesc::is_nonblock(int flags)
{
	return !!(fcntl_flags & O_NONBLOCK) || !!(flags & RWF_NOWAIT);
}

ssize_t filedesc::read(void *buffer, size_t buflen, off_t offset, int flags, bool use_pos)
{
	/* TODO: atomicity */
	if(use_pos) {
		offset = pos;
	}
	ssize_t r = twzio_read(&obj, buffer, buflen, offset, TWZIO_NONBLOCK);
	// debug_printf("::::::: %ld :: " IDFMT "\n", r, IDPR(twz_object_guid(&obj)));
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

static int async_pio_poll(async_job &job, struct event *ev)
{
	int fd = job.tqe.arg0;

	auto filedesc = job.client->proc->get_file(fd);
	if(filedesc == nullptr) {
		return -EBADF;
	}

	int r = filedesc->poll(job.type, ev);
	return r;
}

static void async_pio_callback(async_job &job, int _ret)
{
	if(!_ret) {
		auto [ret, respond] = twix_cmd_pio(job.client, &job.tqe);
		if(respond) {
			job.tqe.ret = ret;
			job.client->complete(&job.tqe);
		}
	} else {
		job.tqe.ret = _ret;
		job.client->complete(&job.tqe);
	}
}

#include <twz/sys/view.h>
int filedesc::poll(uint64_t type, struct event *ev)
{
	return twzio_poll(&obj, type, ev);
}

/* buf: data buffer, buflen: data buffer length *
 * a0: fd, a1: offset, a2: flags, a3: flags2 */
std::pair<long, bool> twix_cmd_pio(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	off_t off = tqe->arg1;
	int pv_flags = tqe->arg2;
	int flags = tqe->arg3;

	// debug_printf("PIO %d %ld %d %d\n", fd, off, pv_flags, flags);

	bool writing = flags & TWIX_FLAGS_PIO_WRITE;

	auto filedesc = client->proc->get_file(fd);
	if(filedesc == nullptr) {
		return R_S(-EBADF);
	}
	if(!filedesc->access(writing ? W_OK : R_OK)) {
		return R_S(-EACCES);
	}

	bool non_block = filedesc->is_nonblock(pv_flags);

	ssize_t ret;
	if(writing) {
		ret = filedesc->write(
		  client->buffer_base(), tqe->buflen, off, pv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	} else {
		ret = filedesc->read(
		  client->buffer_base(), tqe->buflen, off, pv_flags, !!(flags & TWIX_FLAGS_PIO_POS));
	}

	if(ret == -EAGAIN) {
		if(non_block) {
			return R_S(ret);
		}
		auto job = async_job(client,
		  *tqe,
		  async_pio_poll,
		  async_pio_callback,
		  NULL,
		  writing ? TWZIO_EVENT_WRITE : TWZIO_EVENT_READ);
		async_add_job(job);
		return R_A(0);
	} else {
		return R_S(ret);
	}
}

std::pair<long, bool> twix_cmd_poll(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	struct twix_poll_info *info = (struct twix_poll_info *)client->buffer_base();
	long flags = tqe->arg0;

	long ready = 0;
	struct event *events = (struct event *)calloc(info->nr_polls * 2, sizeof(struct event));
	long event_count = 0;
	for(size_t i = 0; i < info->nr_polls; i++) {
		struct pollfd *p = &info->polls[i];
		auto filedesc = client->proc->get_file(p->fd);
		if(filedesc == nullptr) {
			p->revents |= POLLNVAL;
			ready++;
			continue;
		}
		int r = !!(p->events & POLLIN);
		int w = !!(p->events & POLLOUT);
		if(r) {
			int ret = filedesc->poll(TWZIO_EVENT_READ, &events[event_count++]);
			if(ret < 0) {
				p->revents |= POLLERR;
				ready++;
			} else if(ret > 0) {
				p->revents |= POLLIN;
				ready++;
			}
		}
		if(w) {
			int ret = filedesc->poll(TWZIO_EVENT_WRITE, &events[event_count++]);
			if(ret < 0) {
				p->revents |= POLLERR;
				ready++;
			} else if(ret > 0) {
				p->revents |= POLLOUT;
				ready++;
			}
		}
	}

	if(ready == 0) {
		/* TODO (major) : convert this to async jobs */
		std::thread t1([tqe, client, events, info, flags, event_count] {
			twix_queue_entry _tqe = *tqe;
			int r =
			  event_wait(event_count, events, (flags & TWIX_POLL_TIMEOUT) ? &info->timeout : NULL);
			if(r == 0 && (flags & TWIX_POLL_TIMEOUT)) {
				_tqe.ret = 0;
				client->complete(&_tqe);
			} else {
				auto [ret, respond] = twix_cmd_poll(client, &_tqe);
				if(respond) {
					_tqe.ret = ret;
					client->complete(&_tqe);
				}
			}
			free(events);
		});
		t1.detach();
		return R_A(0);
	} else {
		return R_S(0);
	}
}

std::pair<long, bool> twix_cmd_faccessat(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe)
{
	/* TODO */
	return R_S(0);
}

std::pair<long, bool> twix_cmd_dup(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int oldfd = tqe->arg0;
	int newfd = tqe->arg1;
	int flags = tqe->arg2;
	int version = tqe->arg3;

	if(version != 1 && version != 2 && version != 3) {
		return R_S(-EINVAL);
	}
	if(version == 3 && oldfd == newfd) {
		return R_S(-EINVAL);
	}

	auto desc = client->proc->get_file(oldfd);
	int oldfdfl = client->proc->get_file_flags(oldfd);
	if(desc == nullptr || oldfdfl == -EBADF) {
		return R_S(-EBADF);
	}
	if(version == 2 && newfd == oldfd) {
		return R_S(newfd);
	}

	if(version == 1) {
		newfd = client->proc->assign_fd(desc, 0);
	} else {
		client->proc->steal_fd(newfd, desc, flags);
	}

	return R_S(newfd);
}

long filedesc::ioctl(int cmd, void *buf)
{
	return twzio_ioctl(&obj, cmd, buf);
}

std::pair<long, bool> twix_cmd_ioctl(std::shared_ptr<queue_client> client, twix_queue_entry *tqe)
{
	int fd = tqe->arg0;
	int cmd = tqe->arg1;
	auto filedesc = client->proc->get_file(fd);
	if(filedesc == nullptr)
		return R_S(-EBADF);

	void *buf = NULL;
	if(tqe->buflen) {
		buf = client->buffer_base();
	} else {
		buf = (void *)tqe->arg2;
	}
	long ret = filedesc->ioctl(cmd, buf);
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
		.st_size = (off_t)sz,
		.st_blksize = 0,
		.st_blocks = 0,

		.st_atim = {},
		.st_mtim = {},
		.st_ctim = {},
		.__unused = {},
	};

	if(desc->d_type == NAME_ENT_NAMESPACE) {
		st.st_mode |= S_IFDIR;
	} else if(desc->d_type == NAME_ENT_SYMLINK) {
		st.st_mode |= S_IFLNK;
	} else {
		st.st_mode |= S_IFREG;
	}

	client->write_buffer(&st, tqe->buflen);

	return R_S(0);
}
