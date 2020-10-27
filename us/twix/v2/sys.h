#pragma once
#include <stddef.h>
#include <sys/uio.h>

struct syscall_args;
long hook_sys_pwritev2(struct syscall_args *);
long hook_sys_pwritev(struct syscall_args *);
long hook_sys_writev(struct syscall_args *);
long hook_sys_pwrite(struct syscall_args *);
long hook_sys_write(struct syscall_args *);
long hook_sys_preadv2(struct syscall_args *);
long hook_sys_preadv(struct syscall_args *);
long hook_sys_readv(struct syscall_args *);
long hook_sys_pread(struct syscall_args *);
long hook_sys_read(struct syscall_args *);
long hook_sys_fcntl(struct syscall_args *args);
