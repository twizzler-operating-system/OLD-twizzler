/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <interrupt.h>
#include <kec.h>
#include <memory.h>
#include <processor.h>
#include <syscall.h>
#include <thread.h>

static long syscall_null(long a)
{
	// printk("-- called null syscall\n");
	if(a == 0x1234) {
		thread_print_all_threads();
	}
	return 0;
}

bool verify_user_pointer(const void *p, size_t run)
{
	run = (run + 7) & ~7;
	if(run < 8)
		run = 8;
	for(size_t i = 0; i < run; i += 8) {
		uintptr_t addr = (uintptr_t)p + i;
		if(!VADDR_IS_USER(addr))
			return false;
		if(addr % OBJ_MAXSIZE < OBJ_NULLPAGE_SIZE)
			return false;
	}
	return true;
}

static long syscall_debug_print(const char *data, size_t len)
{
	if(len > 1024)
		len = 1024;
	char buf[len + 1];
	strncpy(buf, data, len);
	printk("%s", data);

	return len;
}

long (*syscall_table[NUM_SYSCALLS])() = {
	[SYS_NULL] = syscall_null,
	[SYS_THRD_SPAWN] = syscall_thread_spawn,
	[SYS_DEBUG_PRINT] = syscall_debug_print, // TODO: deprecate
	[SYS_INVL_KSO] = syscall_invalidate_kso,
	[SYS_ATTACH] = syscall_attach,
	[SYS_DETACH] = syscall_detach,
	[SYS_BECOME] = syscall_become,
	[SYS_THRD_SYNC] = syscall_thread_sync,
	[SYS_OCREATE] = syscall_ocreate, // TODO: replace with ocreate2
	[SYS_ODELETE] = syscall_odelete,
	[SYS_THRD_CTL] = syscall_thrd_ctl,
	[SYS_KACTION] = syscall_kaction,
	[SYS_OPIN] = syscall_opin,
	[SYS_OCTL] = syscall_octl,
	[SYS_KCONF] = syscall_kconf,
	[SYS_OTIE] = syscall_otie,
	[SYS_OCOPY] = syscall_ocopy,
	[SYS_KQUEUE] = syscall_kqueue,
	[SYS_OSTAT] = syscall_ostat,
	[SYS_SIGNAL] = syscall_signal,
	[SYS_OCREATE2] = syscall_ocreate2,
	[SYS_KEC_READ] = syscall_kec_read,
	[SYS_KEC_WRITE] = syscall_kec_write,
};

long syscall_prelude(int num)
{
	(void)num;
	// kso_detach_event(current_thread, true, num);
	return 0;
}

long syscall_epilogue(int num)
{
	(void)num;
	// kso_detach_event(current_thread, false, num);
	return 0;
}
