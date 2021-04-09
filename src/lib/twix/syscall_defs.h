#pragma once

#define LINUX_SYS_read 0
#define LINUX_SYS_write 1
#define LINUX_SYS_open 2
#define LINUX_SYS_close 3
#define LINUX_SYS_stat 4
#define LINUX_SYS_fstat 5
#define LINUX_SYS_lstat 6
#define LINUX_SYS_poll 7
#define LINUX_SYS_lseek 8
#define LINUX_SYS_mmap 9

#define LINUX_SYS_munmap 11
#define LINUX_SYS_mprotect 10

#define LINUX_SYS_sigaction 13
#define LINUX_SYS_ioctl 16
#define LINUX_SYS_pread 17
#define LINUX_SYS_pwrite 18
#define LINUX_SYS_readv 19
#define LINUX_SYS_writev 20
#define LINUX_SYS_access 21

#define LINUX_SYS_select 23

#define LINUX_SYS_madvise 28

#define LINUX_SYS_dup 32
#define LINUX_SYS_dup2 33

#define LINUX_SYS_nanosleep 35

#define LINUX_SYS_getpid 39
#define LINUX_SYS_clone 56
#define LINUX_SYS_fork 57
#define LINUX_SYS_vfork 58
#define LINUX_SYS_execve 59
#define LINUX_SYS_exit 60
#define LINUX_SYS_wait4 61
#define LINUX_SYS_kill 62

#define LINUX_SYS_uname 63

#define LINUX_SYS_fsync 74

#define LINUX_SYS_ftruncate 77

#define LINUX_SYS_mkdir 83

#define LINUX_SYS_unlink 87

#define LINUX_SYS_readlink 89

#define LINUX_SYS_getrlimit 97

#define LINUX_SYS_getuid 102
#define LINUX_SYS_getgid 104
#define LINUX_SYS_geteuid 107
#define LINUX_SYS_getegid 108

#define LINUX_SYS_getppid 110
#define LINUX_SYS_getpgid 121
#define LINUX_SYS_arch_prctl 158
#define LINUX_SYS_setrlimit 160
#define LINUX_SYS_chroot 161
#define LINUX_SYS_gettid 186

#define LINUX_SYS_futex 202

#define LINUX_SYS_set_thread_area 205

#define LINUX_SYS_getdents64 217
#define LINUX_SYS_set_tid_address 218

#define LINUX_SYS_clock_gettime 228

#define LINUX_SYS_exit_group 231

#define LINUX_SYS_fstatat 262
#define LINUX_SYS_readlinkat 267
#define LINUX_SYS_faccessat 269
#define LINUX_SYS_pselect6 270

#define LINUX_SYS_dup3 292
#define LINUX_SYS_preadv 295
#define LINUX_SYS_pwritev 296

#define LINUX_SYS_prlimit 302
#define LINUX_SYS_getrandom 318

#define LINUX_SYS_preadv2 327
#define LINUX_SYS_pwritev2 328

#define LINUX_SYS_fcntl 72
