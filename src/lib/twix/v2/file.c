#include "../syscall_defs.h"
#include "v2.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

long hook_open(struct syscall_args *args)
{
	const char *path = (const char *)args->a0;
	// debug_printf("::::: %s\n", path);
	struct twix_queue_entry tqe =
	  build_tqe(TWIX_CMD_OPENAT, 0, strlen(path), 3, -1, args->a1, args->a2);
	write_bufdata(path, strlen(path) + 1, 0);
	twix_sync_command(&tqe);
	// debug_printf("open returned %ld\n", tqe.ret);
	return tqe.ret;
}

long hook_ioctl(struct syscall_args *args)
{
	size_t len = 0;
	int cmd = args->a1;
	switch(cmd) {
		case TCGETS:
		case TCSETS:
		case TCSETSW:
			len = sizeof(struct termios);
			break;
		case TIOCSWINSZ:
		case TIOCGWINSZ:
			len = sizeof(struct winsize);
			break;
	}
	struct twix_queue_entry tqe = build_tqe(
	  TWIX_CMD_IOCTL, 0, len, 6, args->a0, args->a1, args->a2, args->a3, args->a4, args->a5);
	if(len) {
		write_bufdata((void *)args->a2, len, 0);
	}
	twix_sync_command(&tqe);
	if(len && tqe.ret >= 0) {
		extract_bufdata((void *)args->a2, len, 0);
	}
	return tqe.ret;
}

long hook_getdents(struct syscall_args *args)
{
	int fd = args->a0;
	void *dirp = (void *)args->a1;
	size_t count = args->a2;

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GETDENTS, 0, count, 1, fd);
	twix_sync_command(&tqe);
	if(tqe.ret > 0) {
		extract_bufdata(dirp, tqe.ret, 0);
	}
	return tqe.ret;
}

static long do_faccessat(int fd, const char *path, int amode, int flag)
{
	write_bufdata(path, strlen(path) + 1, 0);
	struct twix_queue_entry tqe =
	  build_tqe(TWIX_CMD_FACCESSAT, 0, strlen(path) + 1, 3, fd, amode, flag);
	twix_sync_command(&tqe);
	return tqe.ret;
}

long hook_faccessat(struct syscall_args *args)
{
	int fd = args->a0;
	char *path = (void *)args->a1;
	int amode = args->a2;
	int flag = args->a3;
	return do_faccessat(fd, path, amode, flag);
}

long hook_access(struct syscall_args *args)
{
	char *path = (void *)args->a0;
	int amode = args->a1;
	return do_faccessat(AT_FDCWD, path, amode, 0);
}

long hook_readlink(struct syscall_args *args)
{
	int fd = args->num == LINUX_SYS_readlinkat ? args->a0 : -1;
	const char *pathname = (const char *)(args->num == LINUX_SYS_readlinkat ? args->a1 : args->a0);
	char *buf = (char *)(args->num == LINUX_SYS_readlinkat ? args->a2 : args->a1);
	size_t bufsz = args->num == LINUX_SYS_readlinkat ? args->a3 : args->a2;

	size_t pathlen = strlen(pathname) + 1;
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_READLINK, 0, pathlen + bufsz, 2, fd, bufsz);
	write_bufdata(pathname, pathlen, 0);
	twix_sync_command(&tqe);
	if(tqe.ret > 0) {
		extract_bufdata(buf, tqe.ret, pathlen);
	}
	return tqe.ret;
}
static long do_dup(int old, int new, int flags, int version)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_DUP, 0, 0, 4, old, new, flags, version);
	twix_sync_command(&tqe);
	// debug_printf("___ DUP %d %d %d %d -> %ld\n", old, new, flags, version, tqe.ret);
	return tqe.ret;
}

long hook_dup(struct syscall_args *args)
{
	return do_dup(args->a0, -1, 0, 1);
}

long hook_dup2(struct syscall_args *args)
{
	return do_dup(args->a0, args->a1, 0, 2);
}

long hook_dup3(struct syscall_args *args)
{
	return do_dup(args->a0, args->a1, args->a2, 3);
}
