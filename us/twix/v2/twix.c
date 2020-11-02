#include <stdbool.h>
#include <twix/twix.h>
#include <twz/debug.h>

#include "../syscalls.h"

#include "sys.h"
#include "v2.h"

struct unix_server {
	twzobj cmdqueue, buffer;
	struct secure_api api;
	_Atomic bool inited;
	_Atomic bool ok;
};

static struct unix_server userver = {};

void twix_sync_command(struct twix_queue_entry *tqe)
{
	queue_submit(&userver.cmdqueue, (struct queue_entry *)tqe, 0);
	queue_get_finished(&userver.cmdqueue, (struct queue_entry *)tqe, 0);
}

struct twix_queue_entry build_tqe(enum twix_command cmd, int flags, size_t bufsz, ...)
{
	int nr_va = 0;
	switch(cmd) {
		case TWIX_CMD_GET_PROC_INFO:
			nr_va = 0;
			break;
		case TWIX_CMD_REOPEN_V1_FD:
		case TWIX_CMD_MMAP:
			nr_va = 5;
			break;
		case TWIX_CMD_OPENAT:
		case TWIX_CMD_FCNTL:
			nr_va = 3;
			break;
		case TWIX_CMD_PIO:
			nr_va = 4;
			break;
		case TWIX_CMD_STAT:
			nr_va = 2;
			break;
		default:
			nr_va = 0;
			break;
	}
	long args[6] = {};
	va_list va;
	va_start(va, bufsz);
	for(int i = 0; i < nr_va; i++) {
		args[i] = va_arg(va, long);
	}
	va_end(va);

	struct twix_queue_entry tqe = {
		.cmd = cmd,
		.arg0 = args[0],
		.arg1 = args[1],
		.arg2 = args[2],
		.arg3 = args[3],
		.arg4 = args[4],
		.arg5 = args[5],
		.buflen = bufsz,
		.flags = flags,
	};

	return tqe;
}

void extract_bufdata(void *ptr, size_t len, size_t off)
{
	void *base = twz_object_base(&userver.buffer);
	memcpy(ptr, (char *)base + off, len);
}

void write_bufdata(const void *ptr, size_t len, size_t off)
{
	void *base = twz_object_base(&userver.buffer);
	memcpy((char *)base + off, ptr, len);
}

long get_proc_info(struct proc_info *info)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GET_PROC_INFO, 0, sizeof(struct proc_info));
	twix_sync_command(&tqe);
	extract_bufdata(info, sizeof(*info), 0);

	return 0;
}

static bool setup_queue(void)
{
	if(userver.inited)
		return userver.ok;
	userver.ok = false;
	userver.inited = true;
	if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
		return false;
	}
	objid_t qid, bid;
	int r = twix_open_queue(&userver.api, 0, &qid, &bid);
	if(r) {
		return false;
	}

	if(twz_object_init_guid(&userver.cmdqueue, qid, FE_READ | FE_WRITE))
		return false;

	if(twz_object_init_guid(&userver.buffer, bid, FE_READ | FE_WRITE))
		return false;

	userver.ok = true;
	userver.inited = true;

	for(int fd = 0; fd < MAX_FD; fd++) {
		struct file *file = twix_get_fd(fd);
		if(file) {
			struct twix_queue_entry tqe = build_tqe(TWIX_CMD_REOPEN_V1_FD,
			  0,
			  0,
			  fd,
			  ID_LO(twz_object_guid(&file->obj)),
			  ID_HI(twz_object_guid(&file->obj)),
			  file->fcntl_fl,
			  file->pos);
			twix_sync_command(&tqe);
		}
	}

	return true;
}

#include "../syscall_defs.h"

long hook_proc_info_syscalls(struct syscall_args *args)
{
	struct proc_info info = {};
	get_proc_info(&info);
	switch(args->num) {
		case LINUX_SYS_getpid:
			debug_printf("GETPID: %d\n", info.pid);
			return info.pid;
			break;
		default:
			return -ENOSYS;
	}
}

static long __dummy(struct syscall_args *args __attribute__((unused)))
{
	return 0;
}

long hook_open(struct syscall_args *args)
{
	const char *path = (const char *)args->a0;
	struct twix_queue_entry tqe =
	  build_tqe(TWIX_CMD_OPENAT, 0, strlen(path), -1, args->a1, args->a2);
	write_bufdata(path, strlen(path) + 1, 0);
	twix_sync_command(&tqe);
	return tqe.ret;
}

long hook_mmap(struct syscall_args *args)
{
	void *addr = (void *)args->a0;
	size_t len = args->a1;
	int prot = args->a2;
	int flags = args->a3;
	int fd = args->a4;
	off_t offset = args->a5;

	struct twix_queue_entry tqe = build_tqe(
	  TWIX_CMD_MMAP, 0, sizeof(objid_t), fd, prot, flags, offset, (uintptr_t)addr % OBJ_MAXSIZE);
	twix_sync_command(&tqe);
	return -ENOSYS;
}

static long (*syscall_v2_table[1024])(struct syscall_args *) = {
	[LINUX_SYS_getpid] = hook_proc_info_syscalls,
	[LINUX_SYS_set_tid_address] = __dummy,
	[LINUX_SYS_open] = hook_open,
	[LINUX_SYS_pwritev2] = hook_sys_pwritev2,
	[LINUX_SYS_pwritev] = hook_sys_pwritev,
	[LINUX_SYS_writev] = hook_sys_writev,
	[LINUX_SYS_preadv2] = hook_sys_preadv2,
	[LINUX_SYS_preadv] = hook_sys_preadv,
	[LINUX_SYS_readv] = hook_sys_readv,
	[LINUX_SYS_pread] = hook_sys_pread,
	[LINUX_SYS_pwrite] = hook_sys_pwrite,
	[LINUX_SYS_read] = hook_sys_read,
	[LINUX_SYS_write] = hook_sys_write,
	[LINUX_SYS_fcntl] = hook_sys_fcntl,
	[LINUX_SYS_stat] = hook_sys_stat,
	[LINUX_SYS_fstat] = hook_sys_fstat,
	[LINUX_SYS_lstat] = hook_sys_lstat,
	[LINUX_SYS_fstatat] = hook_sys_fstatat,
	[LINUX_SYS_mmap] = hook_mmap,
};

extern const char *syscall_names[];

bool try_twix_version2(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5,
  long *ret)
{
	struct syscall_args args = {
		.a0 = a0,
		.a1 = a1,
		.a2 = a2,
		.a3 = a3,
		.a4 = a4,
		.a5 = a5,
		.num = num,
		.frame = frame,
	};
	if(!setup_queue())
		return false;
	if(num >= 1024 || num < 0 || !syscall_v2_table[num]) {
		twix_log("twix_v2 syscall: %ld (%s)\n", num, syscall_names[num]);
		return false;
	}
	*ret = syscall_v2_table[num](&args);
	return *ret != -ENOSYS;
}
