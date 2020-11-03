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
	struct twix_queue_entry tqe =
	  build_tqe(TWIX_CMD_EXIT, 0, 0, 2, args->a0, args->num == LINUX_SYS_exit_group);
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
	[LINUX_SYS_close] = hook_simple_passthrough,
	[LINUX_SYS_ioctl] = hook_ioctl,
	[LINUX_SYS_exit] = hook_exit,
	[LINUX_SYS_exit_group] = hook_exit,
	[LINUX_SYS_getdents64] = hook_getdents,
	[LINUX_SYS_access] = hook_access,
	[LINUX_SYS_readlink] = hook_readlink,
	[LINUX_SYS_readlinkat] = hook_readlink,
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
		twix_log("twix_v2 syscall: UNHANDLED %3ld (%s)\n", num, syscall_names[num]);
		*ret = -ENOSYS;
		return -2;
	}
	// twix_log("twix_v2 syscall:           %3ld (%s)\n", num, syscall_names[num]);
	*ret = syscall_v2_table[num](&args);
	return *ret == -ENOSYS ? -3 : 0;
}
