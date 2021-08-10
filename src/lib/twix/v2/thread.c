#include "../syscall_defs.h"
#include "v2.h"
#include <twz/sys/thread.h>
long get_proc_info(struct proc_info *info)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GET_PROC_INFO, 0, sizeof(struct proc_info), 0);
	twix_sync_command(&tqe);
	extract_bufdata(info, sizeof(*info), 0);

	return 0;
}

long hook_proc_info_syscalls(struct syscall_args *args)
{
	struct proc_info info = {};
	get_proc_info(&info);
	switch(args->num) {
		case LINUX_SYS_getpid:
			return info.pid;
			break;
		case LINUX_SYS_getppid:
			return info.ppid;
			break;
		case LINUX_SYS_getpgid:
			return info.pgid;
			break;
		case LINUX_SYS_getgid:
			return info.gid;
			break;
		case LINUX_SYS_getuid:
			return info.uid;
			break;
		case LINUX_SYS_getegid:
			return info.egid;
			break;
		case LINUX_SYS_geteuid:
			return info.euid;
			break;
		default:
			return -ENOSYS;
	}
}

long hook_sys_gettid(struct syscall_args *args)
{
	args->num = LINUX_SYS_getpid; // TODO
	return hook_proc_info_syscalls(args);
}

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_WAIT_REQUEUE_PI 11
#define FUTEX_CMP_REQUEUE_PI 12

#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

long hook_futex(struct syscall_args *args)
{
	int *uaddr = (int *)args->a0;
	int op = args->a1;
	int val = args->a2;
	const struct timespec *timeout = (struct timespec *)args->a3;

	switch((op & FUTEX_CMD_MASK)) {
		case FUTEX_WAIT:
			twz_thread_sync32(
			  THREAD_SYNC_SLEEP, (_Atomic unsigned int *)uaddr, val, (struct timespec *)timeout);
			return 0; // TODO
			break;
		case FUTEX_WAKE:
			twz_thread_sync32(THREAD_SYNC_WAKE, (_Atomic unsigned int *)uaddr, val, NULL);
			return 0; // TODO
			break;
		default:
			debug_printf("futex %d: %p (%x) %x\n", op, uaddr, uaddr ? *uaddr : 0, val);
			return -ENOTSUP;
	}
	return 0;
}

long hook_kill(struct syscall_args *args)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_SEND_SIGNAL, 0, 0, 2, args->a0, args->a1);
	twix_sync_command(&tqe);
	return tqe.ret;
}

#include <sys/resource.h>
struct rusage;
static long linux_wait4(int pid, int *status, int options, struct rusage *rusage)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_WAIT, 0, sizeof(*rusage), 2, pid, options);
	twix_sync_command(&tqe);
	if(rusage) {
		extract_bufdata(rusage, sizeof(*rusage), 0);
	}
	if(status) {
		*status = tqe.arg0;
	}
	return tqe.ret;
}

long hook_wait4(struct syscall_args *args)
{
	int pid = args->a0;
	int *status = (int *)args->a1;
	int options = args->a2;
	struct rusage *ru = (void *)args->a3;
	return linux_wait4(pid, status, options, ru);
}

long hook_wait3(struct syscall_args *args)
{
	int *status = (int *)args->a0;
	int options = args->a1;
	struct rusage *ru = (void *)args->a2;
	return linux_wait4(-1, status, options, ru);
}

long hook_waitpid(struct syscall_args *args)
{
	int pid = args->a0;
	int *status = (int *)args->a1;
	int options = args->a2;
	return linux_wait4(pid, status, options, NULL);
}

long hook_wait(struct syscall_args *args)
{
	int *status = (int *)args->a0;
	return linux_wait4(-1, status, 0, NULL);
}
long hook_exit(struct syscall_args *args)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_EXIT,
	  0,
	  0,
	  2,
	  args->a0,
	  args->num == LINUX_SYS_exit_group ? TWIX_FLAGS_EXIT_GROUP : 0);
	twix_sync_command(&tqe);
	twz_thread_exit(args->a0);
	return 0;
}
