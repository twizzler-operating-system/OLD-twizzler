/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <twz/debug.h>
#include <twz/obj/hier.h>

/* TODO: arch-dep */
#include "syscall_defs.h"
#include "syscalls.h"

long linux_sys_set_thread_area()
{
	return 0;
}

long linux_sys_fsync()
{
	return 0;
}

#include <twz/sys/obj.h>
#include <twz/sys/sync.h>
#include <twz/sys/sys.h>
#include <twz/sys/thread.h>
long linux_sys_nanosleep(struct timespec *spec)
{
	int x = 0;
	return twz_thread_sync32(THREAD_SYNC_SLEEP, (_Atomic unsigned int *)&x, 0, spec);
}

long linux_sys_unlink(char *path)
{
	char dup1[PATH_MAX + 1];
	char dup2[PATH_MAX + 1];
	strcpy(dup1, path);
	strcpy(dup2, path);

	char *dir = dirname(dup1);
	char *base = basename(dup2);

	twzobj parent;
	int r = twz_object_init_name(&parent, dir, FE_READ | FE_WRITE);
	if(r)
		return r;

	twzobj obj;
	r = twz_object_init_name(&obj, path, FE_READ);
	if(r) {
		twz_object_release(&parent);
		return r;
	}

	if((r = twz_object_tie(NULL, &obj, 0))) {
		twz_object_release(&obj);
		twz_object_release(&parent);
		return r;
	}
	if((r = twz_object_delete(&obj, 0))) {
		twz_object_release(&obj);
		twz_object_release(&parent);
		return r;
	}

	r = twz_hier_remove_name(&parent, base);
	twz_object_release(&obj);
	twz_object_release(&parent);
	return r;
}

#include <sys/mman.h>
long linux_sys_mmap(void *addr, size_t len, int prot, int flags, int fd, size_t off);
long linux_sys_mremap(void *old, size_t old_sz, size_t new_sz, int flags, void *new)
{
	if(old_sz > new_sz)
		return (long)old;

	if(flags & 2)
		return -EINVAL;

	if(!(flags & 1)) {
		return -EINVAL;
	}

	long p = linux_sys_mmap(NULL, new_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if(p < 0)
		return p;

	memcpy((void *)p, old, old_sz);

	// twix_log("mremap: %p %lx %lx %x %p\n", old, old_sz, new_sz, flags, new);
	return p;
	return -ENOSYS;
}

static long (*syscall_table[])() = {
	[LINUX_SYS_poll] = linux_sys_poll,
	[LINUX_SYS_mremap] = linux_sys_mremap,
	[LINUX_SYS_arch_prctl] = linux_sys_arch_prctl,
	[LINUX_SYS_set_tid_address] = linux_sys_set_tid_address,
	[LINUX_SYS_set_thread_area] = linux_sys_set_thread_area,
	[LINUX_SYS_pwritev2] = linux_sys_pwritev2,
	[LINUX_SYS_pwritev] = linux_sys_pwritev,
	[LINUX_SYS_writev] = linux_sys_writev,
	[LINUX_SYS_preadv2] = linux_sys_preadv2,
	[LINUX_SYS_preadv] = linux_sys_preadv,
	[LINUX_SYS_readv] = linux_sys_readv,
	[LINUX_SYS_pread] = linux_sys_pread,
	[LINUX_SYS_fcntl] = linux_sys_fcntl,
	[LINUX_SYS_pwrite] = linux_sys_pwrite,
	[LINUX_SYS_read] = linux_sys_read,
	[LINUX_SYS_write] = linux_sys_write,
	[LINUX_SYS_ioctl] = linux_sys_ioctl,
	[LINUX_SYS_exit] = linux_sys_exit,
	[LINUX_SYS_exit_group] = linux_sys_exit,
	[LINUX_SYS_open] = linux_sys_open,
	[LINUX_SYS_close] = linux_sys_close,
	[LINUX_SYS_mmap] = linux_sys_mmap,
	[LINUX_SYS_execve] = linux_sys_execve,
	[LINUX_SYS_clone] = linux_sys_clone,
	[LINUX_SYS_pselect6] = linux_sys_pselect6,
	[LINUX_SYS_select] = linux_sys_select,
	[LINUX_SYS_fork] = linux_sys_fork,
	[LINUX_SYS_wait4] = linux_sys_wait4,
	[LINUX_SYS_stat] = linux_sys_stat,
	[LINUX_SYS_fstat] = linux_sys_fstat,
	[LINUX_SYS_access] = linux_sys_access,
	[LINUX_SYS_futex] = linux_sys_futex,
	[LINUX_SYS_clock_gettime] = linux_sys_clock_gettime,
	[LINUX_SYS_faccessat] = linux_sys_faccessat,
	[LINUX_SYS_getuid] = linux_sys_getuid,
	[LINUX_SYS_getgid] = linux_sys_getgid,
	[LINUX_SYS_geteuid] = linux_sys_geteuid,
	[LINUX_SYS_getegid] = linux_sys_getegid,
	[LINUX_SYS_uname] = linux_sys_uname,
	[LINUX_SYS_lseek] = linux_sys_lseek,
	[LINUX_SYS_getdents64] = linux_sys_getdents64,
	[LINUX_SYS_lstat] = linux_sys_lstat,
	[LINUX_SYS_readlink] = linux_sys_readlink,
	[LINUX_SYS_dup] = linux_sys_dup,
	[LINUX_SYS_dup2] = linux_sys_dup2,
	[LINUX_SYS_dup3] = linux_sys_dup3,
	[LINUX_SYS_getrandom] = linux_sys_getrandom,
	[LINUX_SYS_mkdir] = linux_sys_mkdir,
	[LINUX_SYS_munmap] = linux_sys_munmap,
	[LINUX_SYS_mprotect] = linux_sys_mprotect,
	[LINUX_SYS_madvise] = linux_sys_madvise,
	[LINUX_SYS_gettid] = linux_sys_gettid,
	[LINUX_SYS_getpid] = linux_sys_getpid,
	[LINUX_SYS_getppid] = linux_sys_getppid,
	[LINUX_SYS_prlimit] = linux_sys_prlimit,
	[LINUX_SYS_getpgid] = linux_sys_getpgid,
	[LINUX_SYS_getrlimit] = linux_sys_getrlimit,
	[LINUX_SYS_chroot] = linux_sys_chroot,
	[LINUX_SYS_fsync] = linux_sys_fsync,
	[LINUX_SYS_ftruncate] = linux_sys_ftruncate,
	[LINUX_SYS_vfork] = linux_sys_fork, /* this is not a typo: we implement vfork as fork. */
	[LINUX_SYS_nanosleep] = linux_sys_nanosleep,
	[LINUX_SYS_unlink] = linux_sys_unlink,
};

const char *syscall_names[] = {
	[0] = "read",
	[1] = "write",
	[2] = "open",
	[3] = "close",
	[4] = "stat",
	[5] = "fstat",
	[6] = "lstat",
	[7] = "poll",
	[8] = "lseek",
	[9] = "mmap",
	[10] = "mprotect",
	[11] = "munmap",
	[12] = "brk",
	[13] = "rt_sigaction",
	[14] = "rt_sigprocmask",
	[15] = "rt_sigreturn",
	[16] = "ioctl",
	[17] = "pread64",
	[18] = "pwrite64",
	[19] = "readv",
	[20] = "writev",
	[21] = "access",
	[22] = "pipe",
	[23] = "select",
	[24] = "sched_yield",
	[25] = "mremap",
	[26] = "msync",
	[27] = "mincore",
	[28] = "madvise",
	[29] = "shmget",
	[30] = "shmat",
	[31] = "shmctl",
	[32] = "dup",
	[33] = "dup2",
	[34] = "pause",
	[35] = "nanosleep",
	[36] = "getitimer",
	[37] = "alarm",
	[38] = "setitimer",
	[39] = "getpid",
	[40] = "sendfile",
	[41] = "socket",
	[42] = "connect",
	[43] = "accept",
	[44] = "sendto",
	[45] = "recvfrom",
	[46] = "sendmsg",
	[47] = "recvmsg",
	[48] = "shutdown",
	[49] = "bind",
	[50] = "listen",
	[51] = "getsockname",
	[52] = "getpeername",
	[53] = "socketpair",
	[54] = "setsockopt",
	[55] = "getsockopt",
	[56] = "clone",
	[57] = "fork",
	[58] = "vfork",
	[59] = "execve",
	[60] = "exit",
	[61] = "wait4",
	[62] = "kill",
	[63] = "uname",
	[64] = "semget",
	[65] = "semop",
	[66] = "semctl",
	[67] = "shmdt",
	[68] = "msgget",
	[69] = "msgsnd",
	[70] = "msgrcv",
	[71] = "msgctl",
	[72] = "fcntl",
	[73] = "flock",
	[74] = "fsync",
	[75] = "fdatasync",
	[76] = "truncate",
	[77] = "ftruncate",
	[78] = "getdents",
	[79] = "getcwd",
	[80] = "chdir",
	[81] = "fchdir",
	[82] = "rename",
	[83] = "mkdir",
	[84] = "rmdir",
	[85] = "creat",
	[86] = "link",
	[87] = "unlink",
	[88] = "symlink",
	[89] = "readlink",
	[90] = "chmod",
	[91] = "fchmod",
	[92] = "chown",
	[93] = "fchown",
	[94] = "lchown",
	[95] = "umask",
	[96] = "gettimeofday",
	[97] = "getrlimit",
	[98] = "getrusage",
	[99] = "sysinfo",
	[100] = "times",
	[101] = "ptrace",
	[102] = "getuid",
	[103] = "syslog",
	[104] = "getgid",
	[105] = "setuid",
	[106] = "setgid",
	[107] = "geteuid",
	[108] = "getegid",
	[109] = "setpgid",
	[110] = "getppid",
	[111] = "getpgrp",
	[112] = "setsid",
	[113] = "setreuid",
	[114] = "setregid",
	[115] = "getgroups",
	[116] = "setgroups",
	[117] = "setresuid",
	[118] = "getresuid",
	[119] = "setresgid",
	[120] = "getresgid",
	[121] = "getpgid",
	[122] = "setfsuid",
	[123] = "setfsgid",
	[124] = "getsid",
	[125] = "capget",
	[126] = "capset",
	[127] = "rt_sigpending",
	[128] = "rt_sigtimedwait",
	[129] = "rt_sigqueueinfo",
	[130] = "rt_sigsuspend",
	[131] = "sigaltstack",
	[132] = "utime",
	[133] = "mknod",
	[134] = "uselib",
	[135] = "personality",
	[136] = "ustat",
	[137] = "statfs",
	[138] = "fstatfs",
	[139] = "sysfs",
	[140] = "getpriority",
	[141] = "setpriority",
	[142] = "sched_setparam",
	[143] = "sched_getparam",
	[144] = "sched_setscheduler",
	[145] = "sched_getscheduler",
	[146] = "sched_get_priority_max",
	[147] = "sched_get_priority_min",
	[148] = "sched_rr_get_interval",
	[149] = "mlock",
	[150] = "munlock",
	[151] = "mlockall",
	[152] = "munlockall",
	[153] = "vhangup",
	[154] = "modify_ldt",
	[155] = "pivot_root",
	[156] = "_sysctl",
	[157] = "prctl",
	[158] = "arch_prctl",
	[159] = "adjtimex",
	[160] = "setrlimit",
	[161] = "chroot",
	[162] = "sync",
	[163] = "acct",
	[164] = "settimeofday",
	[165] = "mount",
	[166] = "umount2",
	[167] = "swapon",
	[168] = "swapoff",
	[169] = "reboot",
	[170] = "sethostname",
	[171] = "setdomainname",
	[172] = "iopl",
	[173] = "ioperm",
	[174] = "create_module",
	[175] = "init_module",
	[176] = "delete_module",
	[177] = "get_kernel_syms",
	[178] = "query_module",
	[179] = "quotactl",
	[180] = "nfsservctl",
	[181] = "getpmsg",
	[182] = "putpmsg",
	[183] = "afs_syscall",
	[184] = "tuxcall",
	[185] = "security",
	[186] = "gettid",
	[187] = "readahead",
	[188] = "setxattr",
	[189] = "lsetxattr",
	[190] = "fsetxattr",
	[191] = "getxattr",
	[192] = "lgetxattr",
	[193] = "fgetxattr",
	[194] = "listxattr",
	[195] = "llistxattr",
	[196] = "flistxattr",
	[197] = "removexattr",
	[198] = "lremovexattr",
	[199] = "fremovexattr",
	[200] = "tkill",
	[201] = "time",
	[202] = "futex",
	[203] = "sched_setaffinity",
	[204] = "sched_getaffinity",
	[205] = "set_thread_area",
	[206] = "io_setup",
	[207] = "io_destroy",
	[208] = "io_getevents",
	[209] = "io_submit",
	[210] = "io_cancel",
	[211] = "get_thread_area",
	[212] = "lookup_dcookie",
	[213] = "epoll_create",
	[214] = "epoll_ctl_old",
	[215] = "epoll_wait_old",
	[216] = "remap_file_pages",
	[217] = "getdents64",
	[218] = "set_tid_address",
	[219] = "restart_syscall",
	[220] = "semtimedop",
	[221] = "fadvise64",
	[222] = "timer_create",
	[223] = "timer_settime",
	[224] = "timer_gettime",
	[225] = "timer_getoverrun",
	[226] = "timer_delete",
	[227] = "clock_settime",
	[228] = "clock_gettime",
	[229] = "clock_getres",
	[230] = "clock_nanosleep",
	[231] = "exit_group",
	[232] = "epoll_wait",
	[233] = "epoll_ctl",
	[234] = "tgkill",
	[235] = "utimes",
	[236] = "vserver",
	[237] = "mbind",
	[238] = "set_mempolicy",
	[239] = "get_mempolicy",
	[240] = "mq_open",
	[241] = "mq_unlink",
	[242] = "mq_timedsend",
	[243] = "mq_timedreceive",
	[244] = "mq_notify",
	[245] = "mq_getsetattr",
	[246] = "kexec_load",
	[247] = "waitid",
	[248] = "add_key",
	[249] = "request_key",
	[250] = "keyctl",
	[251] = "ioprio_set",
	[252] = "ioprio_get",
	[253] = "inotify_init",
	[254] = "inotify_add_watch",
	[255] = "inotify_rm_watch",
	[256] = "migrate_pages",
	[257] = "openat",
	[258] = "mkdirat",
	[259] = "mknodat",
	[260] = "fchownat",
	[261] = "futimesat",
	[262] = "newfstatat",
	[263] = "unlinkat",
	[264] = "renameat",
	[265] = "linkat",
	[266] = "symlinkat",
	[267] = "readlinkat",
	[268] = "fchmodat",
	[269] = "faccessat",
	[270] = "pselect6",
	[271] = "ppoll",
	[272] = "unshare",
	[273] = "set_robust_list",
	[274] = "get_robust_list",
	[275] = "splice",
	[276] = "tee",
	[277] = "sync_file_range",
	[278] = "vmsplice",
	[279] = "move_pages",
	[280] = "utimensat",
	[281] = "epoll_pwait",
	[282] = "signalfd",
	[283] = "timerfd_create",
	[284] = "eventfd",
	[285] = "fallocate",
	[286] = "timerfd_settime",
	[287] = "timerfd_gettime",
	[288] = "accept4",
	[289] = "signalfd4",
	[290] = "eventfd2",
	[291] = "epoll_create1",
	[292] = "dup3",
	[293] = "pipe2",
	[294] = "inotify_init1",
	[295] = "preadv",
	[296] = "pwritev",
	[297] = "rt_tgsigqueueinfo",
	[298] = "perf_event_open",
	[299] = "recvmmsg",
	[300] = "fanotify_init",
	[301] = "fanotify_mark",
	[302] = "prlimit64",
	[303] = "name_to_handle_at",
	[304] = "open_by_handle_at",
	[305] = "clock_adjtime",
	[306] = "syncfs",
	[307] = "sendmmsg",
	[308] = "setns",
	[309] = "getcpu",
	[310] = "process_vm_readv",
	[311] = "process_vm_writev",
	[312] = "kcmp",
	[313] = "finit_module",
	[314] = "sched_setattr",
	[315] = "sched_getattr",
	[316] = "renameat2",
	[317] = "seccomp",
	[318] = "getrandom",
	[319] = "memfd_create",
	[320] = "kexec_file_load",
	[321] = "bpf",
	[322] = "execveat",
	[323] = "userfaultfd",
	[324] = "membarrier",
	[325] = "mlock2",
	[326] = "copy_file_range",
	[327] = "preadv2",
	[328] = "pwritev2",
	[329] = "pkey_mprotect",
	[330] = "pkey_alloc",
	[331] = "pkey_free",
};
static size_t stlen = sizeof(syscall_table) / sizeof(syscall_table[0]);
int try_twix_version2(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5,
  long *ret);

int env_state = 0;
static void check_env_state()
{
	if(env_state & ENV_CHECKED)
		return;
	env_state |= ENV_CHECKED;
	char *s = getenv("TWIX_LOG");
	if(!s)
		return;
	if(!strcasecmp(s, "missing") || !strcmp(s, "1") || !strcasecmp(s, "err")) {
		env_state |= ENV_SHOW_UNIMP;
	}
	if(!strcasecmp(s, "all") || !strcmp(s, "2")) {
		env_state |= ENV_SHOW_ALL;
		env_state |= ENV_SHOW_UNIMP;
	}
}

long twix_syscall(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
	check_env_state();
	__linux_init();
	long t2ret;
	int t2res = try_twix_version2(NULL, num, a0, a1, a2, a3, a4, a5, &t2ret);
	if(t2res != -1) {
		return t2ret;
	}
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
		if(env_state & ENV_SHOW_UNIMP) {
			if((env_state & ENV_SHOW_ALL) || (num != 12 && num != 13 && num != 14))
				twix_log("[twix] unimplemented Linux system call: %ld (%s)\n",
				  num,
				  num >= 0 && num < stlen ? syscall_names[num] : "");
		}
		return -ENOSYS;
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return -ENOSYS;
	}
	if(num == LINUX_SYS_fork) {
		/* needs frame */
		return -ENOSYS;
	}
	if(env_state & ENV_SHOW_ALL) {
		twix_log("[twix] invoked syscall %d (%s)\n", num, syscall_names[num]);
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	if((r == -ENOSYS || r == -ENOTSUP) && (env_state & ENV_SHOW_UNIMP)) {
		twix_log("[twix] syscall %snot implemented %ld (%s)\n",
		  r == -ENOSYS ? "" : "feature ",
		  num,
		  syscall_names[num]);
	}
	return r;
}

#include <stdarg.h>
#include <stdio.h>
void twix_log(char *str, ...)
{
	// static int twix_log_fd = 0;
	// if(twix_log_fd <= 0) {
	// twix_log_fd = linux_sys_dup(2);
	//}
	// int fd = twix_log_fd > 0 ? twix_log_fd : 2;
	char buf[2048];
	va_list ap;
	va_start(ap, str);
	vsnprintf(buf, sizeof(buf), str, ap);
	va_end(ap);
	// debug_printf("::: %d: %s\n", fd, buf);
	__sys_debug_print(buf, strlen(buf));
	//	debug_printf(buf);
	// twix_syscall(LINUX_SYS_write, fd, (long)buf, strlen(buf), 0, 0, 0);
	// twix_syscall(LINUX_SYS_write, twix_log_fd, (long)buf, strlen(buf), 0, 0, 0);
}

static long twix_syscall_frame(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5)
{
	check_env_state();
	__linux_init();
	long t2ret;
	int t2res = try_twix_version2(frame, num, a0, a1, a2, a3, a4, a5, &t2ret);
	if(t2res != -1) {
		return t2ret;
	}
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
		if(env_state & ENV_SHOW_UNIMP) {
			if((env_state & ENV_SHOW_ALL) || (num != 12 && num != 13 && num != 14))
				twix_log("Unimplemented Linux system call: %ld (%s)\n",
				  num,
				  num >= 0 && num < stlen ? syscall_names[num] : "");
		}
		return -ENOSYS;
	}
	if(env_state & ENV_SHOW_ALL) {
		twix_log("[twix] invoked syscall %d (%s)\n", num, syscall_names[num]);
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	} else if(num == LINUX_SYS_fork) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	if((r == -ENOSYS || r == -ENOTSUP) && (env_state & ENV_SHOW_UNIMP)) {
		twix_log(":: syscall %snot implemented %ld (%s)\n",
		  r == -ENOSYS ? "" : "feature ",
		  num,
		  syscall_names[num]);
	}
	return r;
}

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	long ret = twix_syscall_frame(
	  frame, num, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
	return ret;
}

__attribute__((used)) static long __twix_syscall_target_c_t(long num,
  struct twix_register_frame *frame)
{
	return __twix_syscall_target_c(num, frame);
}

asm(".global __twix_syscall_target;"
    "__twix_syscall_target:;"
    "movq %rsp, %rcx;"
    "andq $-16, %rsp;"
    "pushq %rcx;"

    "pushq %rbx;"
    "pushq %rdx;"
    "pushq %rdi;"
    "pushq %rsi;"
    "pushq %rbp;"
    "pushq %r8;"
    "pushq %r9;"
    "pushq %r10;"
    "pushq %r11;"
    "pushq %r12;"
    "pushq %r13;"
    "pushq %r14;"
    "pushq %r15;"

    "movq %rsp, %rsi;"
    "movq %rax, %rdi;"

    "lea __twix_syscall_target_c_t(%rip), %rcx;"
    "call *%rcx;"

    "popq %r15;"
    "popq %r14;"
    "popq %r13;"
    "popq %r12;"
    "popq %r11;"
    "popq %r10;"
    "popq %r9;"
    "popq %r8;"
    "popq %rbp;"
    "popq %rsi;"
    "popq %rdi;"
    "popq %rdx;"
    "popq %rbx;"
    "popq %rsp;"
    "ret;");
