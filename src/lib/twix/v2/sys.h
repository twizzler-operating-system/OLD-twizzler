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
long hook_sys_fstat(struct syscall_args *args);
long hook_sys_fstatat(struct syscall_args *args);
long hook_sys_lstat(struct syscall_args *args);
long hook_sys_stat(struct syscall_args *args);
long hook_clone(struct syscall_args *);
long hook_execve(struct syscall_args *);
long hook_poll(struct syscall_args *args);
long hook_ppoll(struct syscall_args *args);
long hook_pselect(struct syscall_args *args);
long hook_select(struct syscall_args *args);
