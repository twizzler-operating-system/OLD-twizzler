/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fcntl.h>
#include <stdbool.h>
#include <twix/twix.h>
#include <twz/debug.h>
#include <twz/sys/obj.h>
#include <twz/sys/view.h>

#include "../syscall_defs.h"
#include "../syscalls.h"
#include <twz/sys/thread.h>

#include "sys.h"
#include "v2.h"

void twix_sync_command(struct twix_queue_entry *tqe)
{
	struct twix_conn *conn = get_twix_conn();
	uint32_t _info = conn->info++;
	tqe->qe.info = _info;

	conn->block_count++;

	queue_submit(&conn->cmdqueue, (struct queue_entry *)tqe, 0);
	queue_get_finished(&conn->cmdqueue, (struct queue_entry *)tqe, 0);

	conn->block_count--;

	if(tqe->qe.info != _info) {
		twix_log("WARNING - out of sync " IDFMT " (%u %u)\n",
		  IDPR(twz_object_guid(&conn->cmdqueue)),
		  tqe->qe.info,
		  _info);
	}

	if(conn->block_count == 0) {
		check_signals(conn);
	}
}

struct twix_queue_entry build_tqe(enum twix_command cmd, int flags, size_t bufsz, int nr_va, ...)
{
	long args[6] = {};
	va_list va;
	va_start(va, nr_va);
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
	struct twix_conn *conn = get_twix_conn();
	void *base = twz_object_base(&conn->buffer);
	memcpy(ptr, (char *)base + off, len);
}

void write_bufdata(const void *ptr, size_t len, size_t off)
{
	struct twix_conn *conn = get_twix_conn();
	void *base = twz_object_base(&conn->buffer);
	memcpy((char *)base + off, ptr, len);
}

static long __dummy(struct syscall_args *args __attribute__((unused)))
{
	return 0;
}

long hook_simple_passthrough(struct syscall_args *args)
{
	long cmd = -1;
	switch(args->num) {
		case LINUX_SYS_close:
			cmd = TWIX_CMD_CLOSE;
			break;
	}
	struct twix_queue_entry tqe =
	  build_tqe(cmd, 0, 0, 6, args->a0, args->a1, args->a2, args->a3, args->a4, args->a5);
	twix_sync_command(&tqe);
	return tqe.ret;
}

static long (*syscall_v2_table[])(struct syscall_args *) = {
	[LINUX_SYS_getpid] = hook_proc_info_syscalls,
	[LINUX_SYS_getppid] = hook_proc_info_syscalls,
	[LINUX_SYS_getgid] = hook_proc_info_syscalls,
	[LINUX_SYS_getuid] = hook_proc_info_syscalls,
	[LINUX_SYS_getpgid] = hook_proc_info_syscalls,
	[LINUX_SYS_geteuid] = hook_proc_info_syscalls,
	[LINUX_SYS_getegid] = hook_proc_info_syscalls,
	[LINUX_SYS_set_tid_address] = __dummy,
	[LINUX_SYS_open] = hook_open,
	[LINUX_SYS_pwritev2] = hook_sys_pwritev2,
	[LINUX_SYS_pwritev] = hook_sys_pwritev,
	[LINUX_SYS_writev] = hook_sys_writev,
	[LINUX_SYS_preadv2] = hook_sys_preadv2,
	[LINUX_SYS_preadv] = hook_sys_preadv,
	[LINUX_SYS_gettid] = hook_sys_gettid,
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
	[LINUX_SYS_close] = hook_simple_passthrough,
	[LINUX_SYS_ioctl] = hook_ioctl,
	[LINUX_SYS_exit] = hook_exit,
	[LINUX_SYS_exit_group] = hook_exit,
	[LINUX_SYS_getdents64] = hook_getdents,
	[LINUX_SYS_access] = hook_access,
	[LINUX_SYS_faccessat] = hook_faccessat,
	[LINUX_SYS_readlink] = hook_readlink,
	[LINUX_SYS_readlinkat] = hook_readlink,
	[LINUX_SYS_futex] = hook_futex,
	[LINUX_SYS_clock_gettime] = hook_clock_gettime,
	[LINUX_SYS_fork] = hook_fork,
	[LINUX_SYS_kill] = hook_kill,
	[LINUX_SYS_wait4] = hook_wait4,
	[LINUX_SYS_dup] = hook_dup,
	[LINUX_SYS_dup2] = hook_dup2,
	[LINUX_SYS_dup3] = hook_dup3,
	[LINUX_SYS_prlimit] = hook_prlimit,
	[LINUX_SYS_getrlimit] = hook_getrlimit,
	[LINUX_SYS_setrlimit] = hook_setrlimit,
	[LINUX_SYS_sigaction] = hook_sigaction,
	[LINUX_SYS_clone] = hook_clone,
	[LINUX_SYS_execve] = hook_execve,
	[LINUX_SYS_ppoll] = hook_ppoll,
	[LINUX_SYS_poll] = hook_poll,
	[LINUX_SYS_select] = hook_select,
	[LINUX_SYS_pselect6] = hook_pselect,
	[LINUX_SYS_uname] = hook_uname,
	[LINUX_SYS_nanosleep] = hook_nanosleep,
	[LINUX_SYS_mremap] = hook_mremap,
};

#define array_len(x) ({ sizeof((x)) / sizeof((x)[0]); })

extern const char *syscall_names[];

extern int env_state;
int try_twix_version2(struct twix_register_frame *frame,
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
	if(!setup_queue()) {
		return -1;
	}
	if(env_state & ENV_SHOW_ALL) {
		twix_log("[twix] invoked syscall %d (%s)\n", num, syscall_names[num]);
	}
	if(num >= array_len(syscall_v2_table) || num < 0 || !syscall_v2_table[num]) {
		if(env_state & ENV_SHOW_UNIMP) {
			twix_log(":: syscall not implemented %ld (%s)\n", num, syscall_names[num]);
		}

		*ret = -ENOSYS;
		return -2;
	}

	*ret = syscall_v2_table[num](&args);
	if((*ret == -ENOSYS || *ret == -ENOTSUP) && (env_state & ENV_SHOW_UNIMP)) {
		twix_log(":: syscall %snot implemented %ld (%s)\n",
		  *ret == -ENOSYS ? "" : "feature ",
		  num,
		  syscall_names[num]);
	} else if(env_state & ENV_SHOW_ALL) {
		twix_log(
		  "[twix] syscall %d (%s) returned %ld (%lx)\n", num, syscall_names[num], *ret, *ret);
	}

	return *ret == -ENOSYS ? -3 : 0;
}
