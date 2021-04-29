#include <fcntl.h>
#include <stdbool.h>
#include <twix/twix.h>
#include <twz/debug.h>
#include <twz/sys/obj.h>
#include <twz/sys/view.h>

#include "../syscalls.h"

#include "sys.h"
#include "v2.h"

twzobj state_object;

struct twix_conn {
	twzobj cmdqueue, buffer;
	objid_t qid, bid;
	_Atomic size_t info;

	_Atomic int block_count;
	_Atomic size_t pending_count;
	struct {
		_Atomic long flags;
		long args[3];
	} pending_sigs[];
};

struct unix_server {
	struct secure_api api;
	_Atomic bool inited;
	_Atomic bool ok;
};

#include <signal.h>
#include <twz/sys/fault.h>
#include <twz/sys/thread.h>
#define SA_DFL                                                                                     \
	(struct k_sigaction)                                                                           \
	{                                                                                              \
		.handler = SIG_DFL                                                                         \
	}
#define NUM_SIG 64

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned mask[2];
};

struct k_sigaction _signal_actions[NUM_SIG] = { [0 ...(NUM_SIG - 1)] = SA_DFL };

static void __twix_do_handler(long *args)
{
	struct twix_queue_entry tqe;
	if(args[0] < 0 || args[0] >= NUM_SIG || args[0] == 0) {
		goto inform_done;
	}
	struct k_sigaction *action = &_signal_actions[args[0]];

	if(action->handler == SIG_IGN) {
		goto inform_done;
	} else if(action->handler == SIG_DFL) {
		switch(args[0]) {
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
				tqe = build_tqe(TWIX_CMD_SUSPEND, 0, 0, 1, args[0]);
				twix_sync_command(&tqe);
				break;
			default: {
				struct twix_queue_entry tqe = build_tqe(
				  TWIX_CMD_EXIT, 0, 0, 2, args[0], TWIX_FLAGS_EXIT_THREAD | TWIX_FLAGS_EXIT_SIGNAL);
				twix_sync_command(&tqe);
				twz_thread_exit(args[0]);
			}
		}
	} else {
		action->handler(args[0]);
	}
inform_done:
	tqe = build_tqe(TWIX_CMD_SIGDONE, 0, 0, 1, args[0]);
	twix_sync_command(&tqe);
}

static void check_signals(struct twix_conn *conn)
{
	for(size_t i = 0; i < conn->pending_count; i++) {
		long ex = 2;
		long args[3];
		args[0] = conn->pending_sigs[i].args[0];
		args[1] = conn->pending_sigs[i].args[1];
		args[2] = conn->pending_sigs[i].args[2];
		if(atomic_compare_exchange_strong(&conn->pending_sigs[i].flags, &ex, 0)) {
			__twix_do_handler(args);
		}
	}
}

static void append_signal(struct twix_conn *conn, long *args)
{
	for(size_t i = 0; i < conn->pending_count; i++) {
		long ex = 0;
		if(atomic_compare_exchange_strong(&conn->pending_sigs[i].flags, &ex, 1)) {
			conn->pending_sigs[i].args[0] = args[1];
			conn->pending_sigs[i].args[1] = args[2];
			conn->pending_sigs[i].args[2] = args[3];
			conn->pending_sigs[i].flags = 2;
			return;
		}
	}

	size_t new = conn->pending_count++;
	conn->pending_sigs[new].flags = 1;
	conn->pending_sigs[new].args[0] = args[1];
	conn->pending_sigs[new].args[1] = args[2];
	conn->pending_sigs[new].args[2] = args[3];
	conn->pending_sigs[new].flags = 2;
}

static struct unix_server userver = {};

void twix_sync_command(struct twix_queue_entry *tqe)
{
	struct twix_conn *conn = twz_object_base(&state_object);
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
	struct twix_conn *conn = twz_object_base(&state_object);
	void *base = twz_object_base(&conn->buffer);
	memcpy(ptr, (char *)base + off, len);
}

void write_bufdata(const void *ptr, size_t len, size_t off)
{
	struct twix_conn *conn = twz_object_base(&state_object);
	void *base = twz_object_base(&conn->buffer);
	memcpy((char *)base + off, ptr, len);
}

long get_proc_info(struct proc_info *info)
{
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_GET_PROC_INFO, 0, sizeof(struct proc_info), 0);
	twix_sync_command(&tqe);
	extract_bufdata(info, sizeof(*info), 0);

	return 0;
}

void __twix_signal_handler(int fault, void *data, void *userdata)
{
	(void)userdata;
	struct fault_signal_info *info = data;
	debug_printf("!!!!! SIGNAL HANDLER: %ld\n", info->args[1]);

	struct twix_conn *conn = twz_object_base(&state_object);
	append_signal(conn, info->args);

	if(conn->block_count == 0) {
		check_signals(conn);
	}
}

void resetup_queue(long is_thread)
{
	/* TODO do we need to serialize this? */
	// debug_printf("child: here! %ld\n", is_thread);
	// for(long i = 0; i < 100000; i++) {
	//	__syscall6(0, 0, 0, 0, 0, 0, 0);
	//}
	// debug_printf("child: done\n");

	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);
	// debug_printf("::: %d %p %p\n", is_thread, &userver, userver.api.hdr);
	if(!is_thread) {
		userver.ok = false;
		userver.inited = true;
	}
	objid_t qid, bid;

	if(!is_thread) {
		if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
			abort();
		}
	}
	int r = twix_open_queue(&userver.api, 0, &qid, &bid);
	if(r) {
		abort();
	}

	// twix_log("reopen! " IDFMT "\n", IDPR(qid));

	objid_t stateid;
	if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &stateid)) {
		abort();
	}

	twz_view_fixedset(NULL, TWZSLOT_UNIX, stateid, VE_READ | VE_WRITE | VE_FIXED | VE_VALID);
	twz_object_init_ptr(&state_object, SLOT_TO_VADDR(TWZSLOT_UNIX));

	struct twix_conn *conn = twz_object_base(&state_object);
	conn->bid = bid;
	conn->qid = qid;

	if(twz_object_init_guid(&conn->cmdqueue, qid, FE_READ | FE_WRITE))
		abort();

	if(twz_object_init_guid(&conn->buffer, bid, FE_READ | FE_WRITE))
		abort();

	userver.ok = true;
	userver.inited = true;
	// debug_printf("AFTER SETUP %p\n", userver.api.hdr);
}

static bool setup_queue(void)
{
	if(userver.inited)
		return userver.ok;
	userver.ok = false;
	userver.inited = true;
	uint32_t flags;
	twz_view_get(NULL, TWZSLOT_UNIX, NULL, &flags);
	bool already = (flags & VE_VALID) && (flags & VE_FIXED);
	objid_t qid, bid;
	if(already) {
		// debug_printf("reopening existing connection\n");
		if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
			return false;
		}
	} else {
		if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
			return false;
		}
		int r = twix_open_queue(&userver.api, 0, &qid, &bid);
		if(r) {
			return false;
		}

		objid_t stateid;
		if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &stateid)) {
			return false;
		}
		twz_view_fixedset(NULL, TWZSLOT_UNIX, stateid, VE_READ | VE_WRITE | VE_FIXED | VE_VALID);
	}

	twz_object_init_ptr(&state_object, SLOT_TO_VADDR(TWZSLOT_UNIX));

	struct twix_conn *conn = twz_object_base(&state_object);
	if(!already) {
		conn->qid = qid;
		conn->bid = bid;
	}
	// debug_printf("OPEN WITH " IDFMT "  " IDFMT "\n", IDPR(conn->qid), IDPR(conn->bid));

	if(twz_object_init_guid(&conn->cmdqueue, conn->qid, FE_READ | FE_WRITE))
		return false;
	if(twz_object_init_guid(&conn->buffer, conn->bid, FE_READ | FE_WRITE))
		return false;

	userver.ok = true;
	userver.inited = true;

	if(!already) {
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
	}
	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);

	return true;
}

bool twix_force_v2_retry(void)
{
	userver.inited = false;
	return setup_queue();
}

#include "../syscall_defs.h"

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

static long __dummy(struct syscall_args *args __attribute__((unused)))
{
	return 0;
}

long hook_sigaction(struct syscall_args *args)
{
	int signum = args->a0;
	struct k_sigaction *act = (void *)args->a1;
	struct k_sigaction *oldact = (void *)args->a2;

	if(oldact) {
		*oldact = _signal_actions[signum];
	} else if(act) {
		/* TODO: need to make this atomic */
		_signal_actions[signum] = *act;
	}

	return 0;
}

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

#include <twz/mutex.h>
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

long hook_mremap(struct syscall_args *args)
{
	void *old = (void *)args->a0;
	size_t old_sz = args->a1;
	size_t new_sz = args->a2;
	int flags = args->a3;
	void *new = (void *)args->a4;
	if(old_sz > new_sz)
		return (long)old;

	if(flags & 2)
		return -EINVAL;

	if(!(flags & 1)) {
		return -EINVAL;
	}

	struct syscall_args _args = {
		.a0 = 0,
		.a1 = new_sz,
		.a2 = PROT_READ | PROT_WRITE,
		.a3 = MAP_PRIVATE | MAP_ANON,
		.a4 = -1,
		.a5 = 0,
	};
	long p = hook_mmap(&_args);
	if(p < 0)
		return p;

	memcpy((void *)p, old, old_sz);

	return p;
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

#include <sys/ioctl.h>
#include <termios.h>
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

long hook_nanosleep(struct syscall_args *args)
{
	struct timespec *spec = (void *)args->a0;
	int x = 0;
	return twz_thread_sync32(THREAD_SYNC_SLEEP, (_Atomic unsigned int *)&x, 0, spec);
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

#include <twz/sys/thread.h>
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

#include <sys/utsname.h>
long hook_uname(struct syscall_args *args)
{
	struct utsname *buf = (void *)args->a0;
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_UNAME, 0, 0, 0);
	twix_sync_command(&tqe);
	if(tqe.ret == 0) {
		struct twix_uname_info info;
		extract_bufdata(&info, sizeof(info), 0);
		strcpy(buf->sysname, info.sysname);
		strcpy(buf->nodename, info.sysname);
		strcpy(buf->nodename, "twizzler"); // TODO
		strcpy(buf->release, info.release);
		strcpy(buf->version, info.version);
	}
	return tqe.ret;
}

long do_prlimit(int pid, int r, struct rlimit *new, struct rlimit *old)
{
	int flags = 0;
	if(new)
		flags |= TWIX_PRLIMIT_SET;
	if(old)
		flags |= TWIX_PRLIMIT_GET;
	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_PRLIMIT, 0, sizeof(*new), 3, pid, r, flags);
	if(new)
		write_bufdata(new, sizeof(*new), 0);
	twix_sync_command(&tqe);
	if(old && tqe.ret == 0)
		extract_bufdata(old, sizeof(*old), 0);
	return tqe.ret;
}

long hook_prlimit(struct syscall_args *args)
{
	int pid = args->a0;
	int r = args->a1;
	struct rlimit *new = (struct rlimit *)args->a2;
	struct rlimit *old = (struct rlimit *)args->a3;
	return do_prlimit(pid, r, new, old);
}

long hook_getrlimit(struct syscall_args *args)
{
	int r = args->a0;
	struct rlimit *lim = (struct rlimit *)args->a1;
	return do_prlimit(0, r, NULL, lim);
}

long hook_setrlimit(struct syscall_args *args)
{
	int r = args->a0;
	struct rlimit *lim = (struct rlimit *)args->a1;
	return do_prlimit(0, r, NULL, lim);
}

static long (*syscall_v2_table[1024])(struct syscall_args *) = {
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
	// twix_log("twix_v2 syscall: %3ld (%s) %p\n", num, syscall_names[num], userver.api.hdr);
	if(num >= 1024 || num < 0 || !syscall_v2_table[num]) {
		if(env_state & ENV_SHOW_UNIMP) {
			twix_log(":: syscall not implemented %ld (%s)\n", num, syscall_names[num]);
		}

		*ret = -ENOSYS;
		return -2;
	}
	// twix_log("twix_v2 syscall:           %3ld (%s)\n", num, syscall_names[num]);

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
