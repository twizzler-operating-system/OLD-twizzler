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
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GET_PROC_INFO, 0, sizeof(struct proc_info), 0);
	twix_sync_command(&tqe);
	extract_bufdata(info, sizeof(*info), 0);

	return 0;
}

#include <signal.h>
#include <twz/fault.h>
#define SA_DFL                                                                                     \
	(struct sigaction)                                                                             \
	{                                                                                              \
		.sa_handler = SIG_DFL                                                                      \
	}
#define NUM_SIG 64
struct sigaction _signal_actions[NUM_SIG] = { [0 ...(NUM_SIG - 1)] = SA_DFL };

static void __twix_signal_handler(int fault, void *data, void *userdata)
{
	struct twix_queue_entry tqe;
	(void)userdata;
	struct fault_signal_info *info = data;
	debug_printf("!!!!! SIGNAL HANDLER: %ld\n", info->args[1]);

	if(info->args[1] < 0 || info->args[1] >= NUM_SIG || info->args[1] == 0) {
		goto inform_done;
	}
	struct sigaction *action = &_signal_actions[info->args[1]];

	if(action->sa_handler == SIG_IGN) {
		goto inform_done;
	} else if(action->sa_handler == SIG_DFL) {
		switch(info->args[1]) {
			case SIGCHLD:
			case SIGURG:
			case SIGWINCH:
				break;
			case SIGCONT:
				break;
			case SIGTTIN:
			case SIGTTOU:
			case SIGSTOP:
			case SIGTSTP:
				tqe = build_tqe(TWIX_CMD_SUSPEND, 0, 0, 1, info->args[1]);
				twix_sync_command(&tqe);
				break;
			default: {
				struct twix_queue_entry tqe = build_tqe(TWIX_CMD_EXIT,
				  0,
				  0,
				  2,
				  info->args[1],
				  TWIX_FLAGS_EXIT_THREAD | TWIX_FLAGS_EXIT_SIGNAL);
				twix_sync_command(&tqe);
				twz_thread_exit(info->args[1]);
			}
		}
	} else {
		action->sa_handler(info->args[1]);
	}
inform_done:
	tqe = build_tqe(TWIX_CMD_SIGDONE, 0, 0, 1, info->args[1]);
	twix_sync_command(&tqe);
}

void resetup_queue(void)
{
	debug_printf("child: here!\n");
	for(long i = 0; i < 100000; i++) {
		__syscall6(0, 0, 0, 0, 0, 0, 0);
	}
	debug_printf("child: done\n");
	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);
	userver.ok = false;
	userver.inited = true;
	objid_t qid, bid;
	int r = twix_open_queue(&userver.api, 0, &qid, &bid);
	if(r) {
		abort();
	}

	if(twz_object_init_guid(&userver.cmdqueue, qid, FE_READ | FE_WRITE))
		abort();

	if(twz_object_init_guid(&userver.buffer, bid, FE_READ | FE_WRITE))
		abort();

	userver.ok = true;
	userver.inited = true;
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
			objid_t id = twz_object_guid(&file->obj);
			struct twix_queue_entry tqe = build_tqe(TWIX_CMD_REOPEN_V1_FD,
			  0,
			  0,
			  5,
			  (long)fd,
			  ID_LO(id),
			  ID_HI(id),
			  (long)file->fcntl_fl | 3 /*TODO*/,
			  file->pos);
			twix_sync_command(&tqe);
		}
	}
	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);

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
	  build_tqe(TWIX_CMD_OPENAT, 0, strlen(path), 3, -1, args->a1, args->a2);
	write_bufdata(path, strlen(path) + 1, 0);
	twix_sync_command(&tqe);
	twix_log("open returned %ld\n", tqe.ret);
	return tqe.ret;
}

#include <twz/mutex.h>
#include <twz/view.h>
static struct mutex mmap_mutex;
static uint8_t mmap_bitmap[TWZSLOT_MMAP_NUM / 8];

static ssize_t __twix_mmap_get_slot(void)
{
	for(size_t i = 0; i < TWZSLOT_MMAP_NUM; i++) {
		if(!(mmap_bitmap[i / 8] & (1 << (i % 8)))) {
			mmap_bitmap[i / 8] |= (1 << (i % 8));
			return i + TWZSLOT_MMAP_BASE;
		}
	}
	return -1;
}

static ssize_t __twix_mmap_take_slot(size_t slot)
{
	slot -= TWZSLOT_MMAP_BASE;
	if(mmap_bitmap[slot / 8] & (1 << (slot % 8))) {
		return -1;
	}
	mmap_bitmap[slot / 8] |= (1 << (slot % 8));
	return slot + TWZSLOT_MMAP_BASE;
}

#include <sys/mman.h>
long hook_mmap(struct syscall_args *args)
{
	void *addr = (void *)args->a0;
	size_t len = (args->a1 + 0xfff) & ~0xfff;
	int prot = args->a2;
	int flags = args->a3;
	int fd = args->a4;
	off_t offset = args->a5;
	objid_t id;
	int r;

	ssize_t slot = -1;
	size_t adj = OBJ_NULLPAGE_SIZE;
	if(addr && (flags & MAP_FIXED)) {
		adj = (uintptr_t)addr % OBJ_MAXSIZE;
		slot = VADDR_TO_SLOT(addr);
		if(__twix_mmap_take_slot(slot) == -1) {
			/* if we're trying to map to an address with an existing object in an mmap slot... */
			if(!(flags & MAP_ANON)) {
				return -ENOTSUP;
			}
			/* TODO: verify that this is a "private" object */
			twz_view_get(NULL, VADDR_TO_SLOT(addr), &id, NULL);
			if(id) {
				if((r = sys_ocopy(id, 0, (uintptr_t)addr % OBJ_MAXSIZE, 0, len, 0))) {
					return r;
				}
				return (long)addr;
			}
		}
	} else {
		addr = (void *)OBJ_NULLPAGE_SIZE;
	}

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_MMAP,
	  0,
	  sizeof(objid_t),
	  6,
	  fd,
	  prot,
	  flags,
	  offset,
	  (uintptr_t)addr % OBJ_MAXSIZE,
	  len);
	twix_sync_command(&tqe);
	if(tqe.ret)
		return tqe.ret;
	extract_bufdata(&id, sizeof(id), 0);
	/* TODO: tie object */
	// twix_log("twix_v2_mmap: " IDFMT "\n", IDPR(id));

	if(slot == -1) {
		slot = __twix_mmap_get_slot();
	}
	// twix_log("   mmap slot %ld\n", slot);
	if(slot == -1) {
		return -ENOMEM;
	}

	/* TODO: perms */
	twz_view_set(NULL, slot, id, VE_READ | VE_WRITE | VE_EXEC);

	return (long)SLOT_TO_VADDR(slot) + adj;
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

long hook_ioctl(struct syscall_args *args)
{
	return 0;
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

long hook_access(struct syscall_args *args)
{
	return 0;
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

#include <twz/thread.h>
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
			twix_log("futex %d: %p (%x) %x\n", op, uaddr, uaddr ? *uaddr : 0, val);
			return -ENOTSUP;
	}
	return 0;
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#include <time.h>
long hook_clock_gettime(struct syscall_args *args)
{
	clockid_t clock = args->a0;
	struct timespec *tp = (struct timespec *)args->a1;
	static long tsc_ps = 0;
	if(!tsc_ps) {
		tsc_ps = sys_kconf(KCONF_ARCH_TSC_PSPERIOD, 0);
	}
	switch(clock) {
		uint64_t ts;
		case CLOCK_REALTIME:
		case CLOCK_REALTIME_COARSE:
		case CLOCK_PROCESS_CPUTIME_ID:
		/* TODO: these should probably be different */
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
		case CLOCK_MONOTONIC_COARSE:
			ts = rdtsc();
			/* TODO: overflow? */
			tp->tv_sec = ((long)((double)ts / (1000.0 / (double)tsc_ps))) / 1000000000ul;
			tp->tv_nsec = ((long)((double)ts / (1000.0 / (double)tsc_ps))) % 1000000000ul;
			break;
		default:
			twix_log(":: CGT: %ld\n", clock);
			return -ENOTSUP;
	}
	return 0;
}

long hook_sys_gettid(struct syscall_args *args)
{
	args->num = LINUX_SYS_getpid; // TODO
	return hook_proc_info_syscalls(args);
}

long hook_kill(struct syscall_args *args)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_SEND_SIGNAL, 0, 0, 2, args->a0, args->a1);
	twix_sync_command(&tqe);
	return tqe.ret;
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
	[LINUX_SYS_readlink] = hook_readlink,
	[LINUX_SYS_readlinkat] = hook_readlink,
	[LINUX_SYS_futex] = hook_futex,
	[LINUX_SYS_clock_gettime] = hook_clock_gettime,
	[LINUX_SYS_fork] = hook_fork,
	[LINUX_SYS_kill] = hook_kill,
};

extern const char *syscall_names[];

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
	if(num >= 1024 || num < 0 || !syscall_v2_table[num]) {
		if(num != 14) // TODO signals
			twix_log("twix_v2 syscall: UNHANDLED %3ld (%s)\n", num, syscall_names[num]);
		*ret = -ENOSYS;
		return -2;
	}
	// twix_log("twix_v2 syscall:           %3ld (%s)\n", num, syscall_names[num]);
	*ret = syscall_v2_table[num](&args);
	return *ret == -ENOSYS ? -3 : 0;
}
