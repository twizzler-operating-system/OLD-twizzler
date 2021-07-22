/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <twz/_err.h>
#include <twz/_types.h>
#include <twz/debug.h>
#include <twz/meta.h>
#include <twz/obj.h>
#include <twz/ptr.h>
#include <twz/sys/obj.h>
#include <twz/sys/slots.h>
#include <twz/sys/sys.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

void twz_thread_yield(void)
{
	sys_thrd_ctl(THRD_CTL_YIELD, 0);
}

void twz_thread_set_name(const char *name)
{
	kso_set_name(NULL, name);
}

struct twzthread_ctrl_repr *twz_thread_ctrl_repr_base(void)
{
	return (
	  struct twzthread_ctrl_repr *)((char *)(TWZSLOT_TCTRL * OBJ_MAXSIZE) + OBJ_NULLPAGE_SIZE);
}

struct twzthread_repr *twz_thread_repr_base(void)
{
	uint64_t a;
	asm volatile("rdgsbase %%rax" : "=a"(a));
	if(!a) {
		libtwz_panic("could not find twz_thread_repr_base %p", __builtin_return_address(0));
	}
	a &= ~(OBJ_MAXSIZE - 1);
	return (struct twzthread_repr *)(a + OBJ_NULLPAGE_SIZE);
}

void twz_thread_exit(uint64_t ecode)
{
	struct twzthread_repr *repr = twz_thread_repr_base();
	repr->syncinfos[THRD_SYNC_EXIT] = ecode;
	repr->syncs[THRD_SYNC_EXIT] = 1;
	int r = sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->syncs[THRD_SYNC_EXIT]);
	(void)r;
	__builtin_unreachable();
}

void twz_thread_sync_init(struct sys_thread_sync_args *args,
  int op,
  _Atomic uint64_t *addr,
  uint64_t val)
{
	*args = (struct sys_thread_sync_args){
		.op = op,
		.addr = (uint64_t *)addr,
		.arg = val,
	};
}

int twz_thread_sync(int op, _Atomic uint64_t *addr, uint64_t val, struct timespec *timeout)
{
	struct sys_thread_sync_args args = {
		.op = op,
		.addr = (uint64_t *)addr,
		.arg = val,
	};
	return sys_thread_sync(1, &args, timeout);
}

int twz_thread_sync32(int op, _Atomic uint32_t *addr, uint32_t val, struct timespec *timeout)
{
	struct sys_thread_sync_args args = {
		.op = op,
		.addr = (uint64_t *)addr,
		.arg = val,
	};
	args.flags |= THREAD_SYNC_32BIT;
	return sys_thread_sync(1, &args, timeout);
}

int twz_thread_sync_multiple(size_t c, struct sys_thread_sync_args *args, struct timespec *timeout)
{
	return sys_thread_sync(c, args, timeout);
}

#if 0
uint64_t twz_thread_cword_consume(_Atomic uint64_t *w, uint64_t reset)
{
	while(true) {
		uint64_t tmp = atomic_exchange(w, reset);
		if(tmp != reset) {
			return tmp;
		}
		if(twz_thread_sync(THREAD_SYNC_SLEEP, w, reset, NULL) < 0) {
			libtwz_panic("thread_sync failed");
		}
	}
}

void twz_thread_cword_wake(_Atomic uint64_t *w, uint64_t val)
{
	*w = val;
	if(twz_thread_sync(THREAD_SYNC_WAKE, w, INT_MAX, NULL) < 0) {
		libtwz_panic("thread_sync failed");
	}
}
#endif
