
#include <stdint.h>

/* TODO: arch-dep */

struct twix_register_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
};

#include <debug.h>
#include <errno.h>
#include <twzsys.h>
#include <twzobj.h>
#include <twzslots.h>
#include <twzview.h>
#include <twzio.h>

#define LINUX_SYS_readv            19
#define LINUX_SYS_writev           20

#define LINUX_SYS_arch_prctl      158

#define LINUX_SYS_set_tid_address 218

#define LINUX_SYS_preadv          295
#define LINUX_SYS_pwritev         296

#define LINUX_SYS_preadv2         327
#define LINUX_SYS_pwritev2        328

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

long linux_sys_arch_prctl(int code, unsigned long addr)
{
	switch(code) {
		case ARCH_SET_FS:
			sys_thrd_ctl(THRD_CTL_SET_FS, (long)addr);
			break;
		case ARCH_SET_GS:
			sys_thrd_ctl(THRD_CTL_SET_GS, (long)addr);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

struct iovec {
	void *base;
	size_t len;
};

struct file {
	int fd;
	uint32_t fl;
	bool valid;
	size_t pos;
	struct object obj;
};

#define MAX_FD 1024
static struct file fds[MAX_FD];

#define DW_NONBLOCK 1
static ssize_t __do_write(struct object *o, ssize_t off, void *base, size_t len, int flags)
{
	ssize_t r = twzio_write(o, base, len, (flags & DW_NONBLOCK) ? TWZIO_NONBLOCK : 0);
	if(r == -TE_NOTSUP) {
		/* TODO: bounds check */
		memcpy((char *)twz_ptr_base(o) + off + OBJ_NULLPAGE_SIZE, base, len);
		r = len;
	}
	return r;
}

static ssize_t __do_read(struct object *o, ssize_t off, void *base, size_t len, int flags)
{
	ssize_t r = twzio_read(o, base, len, (flags & DW_NONBLOCK) ? TWZIO_NONBLOCK : 0);
	if(r == -TE_NOTSUP) {
		/* TODO: bounds check */
		memcpy(base, (char *)twz_ptr_base(o) + off + OBJ_NULLPAGE_SIZE, len);
		r = len;
	}
	return r;
}

long linux_sys_preadv2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags)
{
	(void)flags; /* TODO (minor): implement */
	if(fd < 0 || fd >= MAX_FD) {
		return -EINVAL;
	}
	if(!fds[fd].valid) {
		twz_view_get(NULL, TWZSLOT_FILES_BASE + fd, NULL, &fds[fd].fl);
		if(!(fds[fd].fl & VE_VALID)) {
			return -EBADF;
		}
		twz_object_init(&fds[fd].obj, TWZSLOT_FILES_BASE + fd);
		fds[fd].valid = true;
	}

	if(!(fds[fd].fl & VE_READ)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i=0;i<iovcnt;i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_read(&fds[fd].obj, (off < 0) ? fds[fd].pos : (size_t)off,
				v->base, v->len, (count == 0) ? 0 : DW_NONBLOCK);
		if(r > 0) {
			if(off == -1) {
				fds[i].pos += r;
			} else {
				off += r;
			}
			count += r;
		} else {
			break;
		}
	}
	return count;

}

long linux_sys_preadv(int fd, const struct iovec *iov, int iovcnt, size_t off)
{
	return linux_sys_preadv2(fd, iov, iovcnt, off, 0);
}

long linux_sys_readv(int fd, const struct iovec *iov, int iovcnt)
{
	return linux_sys_preadv2(fd, iov, iovcnt, -1, 0);
}

long linux_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags)
{
	(void)flags; /* TODO (minor): implement */
	if(fd < 0 || fd >= MAX_FD) {
		return -EINVAL;
	}
	if(!fds[fd].valid) {
		twz_view_get(NULL, TWZSLOT_FILES_BASE + fd, NULL, &fds[fd].fl);
		if(!(fds[fd].fl & VE_VALID)) {
			return -EBADF;
		}
		twz_object_init(&fds[fd].obj, TWZSLOT_FILES_BASE + fd);
		fds[fd].valid = true;
	}

	if(!(fds[fd].fl & VE_WRITE)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i=0;i<iovcnt;i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_write(&fds[fd].obj, (off < 0) ? fds[fd].pos : (size_t)off,
				v->base, v->len, (count == 0) ? 0 : DW_NONBLOCK);
		if(r > 0) {
			if(off == -1) {
				fds[i].pos += r;
			} else {
				off += r;
			}
			count += r;
		} else {
			break;
		}
	}
	return count;
}

long linux_sys_pwritev(int fd, const struct iovec *iov, int iovcnt, size_t off)
{
	return linux_sys_pwritev2(fd, iov, iovcnt, off, 0);
}

long linux_sys_writev(int fd, const struct iovec *iov, int iovcnt)
{
	return linux_sys_pwritev2(fd, iov, iovcnt, -1, 0);
}

long linux_sys_set_tid_address()
{
	/* TODO: NI */
	return 0;
}

static long (*syscall_table[])() = {
	[LINUX_SYS_arch_prctl] = linux_sys_arch_prctl,
	[LINUX_SYS_set_tid_address] = linux_sys_set_tid_address,
	[LINUX_SYS_pwritev2] = linux_sys_pwritev2,
	[LINUX_SYS_pwritev] = linux_sys_pwritev,
	[LINUX_SYS_writev] = linux_sys_writev,
	[LINUX_SYS_preadv2] = linux_sys_preadv2,
	[LINUX_SYS_preadv] = linux_sys_preadv,
	[LINUX_SYS_readv] = linux_sys_readv,
};

static size_t stlen = sizeof(syscall_table) / sizeof(syscall_table[0]);

long twix_syscall(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
		return -ENOSYS;
	}
	return syscall_table[num](a0, a1, a2, a3, a4, a5);
}

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	debug_printf("TWIX entry: %ld, %p %lx\n", num, frame, frame->rsp);
	long ret = twix_syscall(num, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
	if(ret == -ENOSYS) {
		debug_printf("Unimplemented UNIX system call: %ld", num);
	} else {
		debug_printf("TWIX exit : %ld -> %ld\n", num, ret);
	}
	return ret;
}

asm (
	".global __twix_syscall_target;"
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
	"call __twix_syscall_target_c;"

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
	"ret;"
);


