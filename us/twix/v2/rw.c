#include <stdio.h>

#include "v2.h"
#include <sys/uio.h>

#include <twix/twix.h>

#define HOOK5(name, t0, v0, t1, v1, t2, v2, t3, v3, t4, v4)                                        \
	long twix_##name(t0, t1, t2, t3, t4);                                                          \
	long hook_##name(struct syscall_args *args)                                                    \
	{                                                                                              \
		return twix_##name((t0)args->a0, (t1)args->a1, (t2)args->a2, (t3)args->a3, (t4)args->a4);  \
	}                                                                                              \
	long twix_##name(t0 v0, t1 v1, t2 v2, t3 v3, t4 v4)

#define HOOK4(name, t0, v0, t1, v1, t2, v2, t3, v3)                                                \
	long twix_##name(t0, t1, t2, t3);                                                              \
	long hook_##name(struct syscall_args *args)                                                    \
	{                                                                                              \
		return twix_##name((t0)args->a0, (t1)args->a1, (t2)args->a2, (t3)args->a3);                \
	}                                                                                              \
	long twix_##name(t0 v0, t1 v1, t2 v2, t3 v3)

#define HOOK3(name, t0, v0, t1, v1, t2, v2)                                                        \
	long twix_##name(t0, t1, t2);                                                                  \
	long hook_##name(struct syscall_args *args)                                                    \
	{                                                                                              \
		return twix_##name((t0)args->a0, (t1)args->a1, (t2)args->a2);                              \
	}                                                                                              \
	long twix_##name(t0 v0, t1 v1, t2 v2)

HOOK5(sys_preadv2, int, fd, const struct iovec *, iov, int, iovcnt, off_t, off, int, flags)
{
	size_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		count += iov[i].iov_len;
	}

	if(count >= OBJ_TOPDATA)
		count = OBJ_TOPDATA;

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_PIO,
	  0,
	  count,
	  fd,
	  off,
	  flags,
	  TWIX_FLAGS_PIO_WRITE | ((off < 0) ? TWIX_FLAGS_PIO_POS : 0));
	twix_sync_command(&tqe);
	if(tqe.ret <= 0) {
		return tqe.ret;
	}
	count = 0;
	for(int i = 0; i < iovcnt; i++) {
		size_t thisiov_len = iov[i].iov_len;
		if(count + thisiov_len > (size_t)tqe.ret) {
			thisiov_len = tqe.ret - count;
			if(thisiov_len == 0) {
				return count;
			}
		}
		extract_bufdata(iov[i].iov_base, thisiov_len, count);
	}

	return tqe.ret;
}

HOOK4(sys_preadv, int, fd, const struct iovec *, iov, int, iovcnt, size_t, off)
{
	return twix_sys_preadv2(fd, iov, iovcnt, off, 0);
}

HOOK3(sys_readv, int, fd, const struct iovec *, iov, int, iovcnt)
{
	return twix_sys_preadv2(fd, iov, iovcnt, -1, 0);
}

HOOK4(sys_pread, int, fd, void *, buf, size_t, count, size_t, off)
{
	struct iovec v = { .iov_base = buf, .iov_len = count };
	return twix_sys_preadv(fd, &v, 1, off);
}

HOOK3(sys_read, int, fd, void *, buf, size_t, count)
{
	struct iovec v = { .iov_base = buf, .iov_len = count };
	return twix_sys_preadv(fd, &v, 1, -1);
}

HOOK5(sys_pwritev2, int, fd, const struct iovec *, iov, int, iovcnt, off_t, off, int, flags)
{
	// long twix_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt, off_t off, int flags)
	//{
	size_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		size_t thisiov_len = iov[i].iov_len;
		if(count + thisiov_len > OBJ_TOPDATA) {
			thisiov_len = OBJ_TOPDATA - count;
			if(thisiov_len == 0) {
				return count;
			}
		}
		write_bufdata(iov[i].iov_base, thisiov_len, count);
	}

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_PIO,
	  0,
	  count,
	  fd,
	  off,
	  flags,
	  TWIX_FLAGS_PIO_WRITE | ((off < 0) ? TWIX_FLAGS_PIO_POS : 0));
	twix_sync_command(&tqe);
	return tqe.ret;
}

HOOK4(sys_pwritev, int, fd, const struct iovec *, iov, int, iovcnt, off_t, off)
{
	return twix_sys_pwritev2(fd, iov, iovcnt, off, 0);
}

HOOK3(sys_writev, int, fd, const struct iovec *, iov, int, iovcnt)
{
	return twix_sys_pwritev2(fd, iov, iovcnt, -1, 0);
}

HOOK4(sys_pwrite, int, fd, const void *, buf, size_t, count, size_t, off)
{
	struct iovec v = { .iov_base = (void *)buf, .iov_len = count };
	return twix_sys_pwritev(fd, &v, 1, off);
}

HOOK3(sys_write, int, fd, const void *, buf, size_t, count)
{
	struct iovec v = { .iov_base = (void *)buf, .iov_len = count };
	return twix_sys_pwritev(fd, &v, 1, -1);
}

long hook_sys_fcntl(struct syscall_args *args)
{
	int fd = args->a0;
	int cmd = args->a1;
	int arg = args->a2;

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_FCNTL, 0, 0, fd, cmd, arg);
	twix_sync_command(&tqe);
	return tqe.ret;
}

#include <fcntl.h>
#include <sys/stat.h>
static long twix_sys_fstatat(int fd, const char *path, struct stat *st, int flag)
{
	size_t buflen = path ? strlen(path) + 1 : 0;
	if(path) {
		write_bufdata(path, buflen, 0);
	}
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_STAT, buflen, 0, fd, flag);
	twix_sync_command(&tqe);
	extract_bufdata(st, sizeof(*st), buflen);
	return tqe.ret;
}

long hook_sys_stat(struct syscall_args *args)
{
	const char *path = (const char *)args->a0;
	struct stat *st = (struct stat *)args->a1;
	return twix_sys_fstatat(AT_FDCWD, path, st, 0);
}

long hook_sys_lstat(struct syscall_args *args)
{
	const char *path = (const char *)args->a0;
	struct stat *st = (struct stat *)args->a1;
	return twix_sys_fstatat(AT_FDCWD, path, st, AT_SYMLINK_NOFOLLOW);
}

long hook_sys_fstatat(struct syscall_args *args)
{
	int fd = args->a0;
	const char *path = (const char *)args->a1;
	struct stat *st = (struct stat *)args->a2;
	int flags = args->a3;
	return twix_sys_fstatat(fd, path, st, flags);
}

long hook_sys_fstat(struct syscall_args *args)
{
	int fd = args->a0;
	struct stat *st = (struct stat *)args->a1;
	return twix_sys_fstatat(fd, NULL, st, 0);
}
