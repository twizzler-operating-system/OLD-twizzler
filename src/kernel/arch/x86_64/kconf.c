/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/x86_64.h>
#include <syscall.h>

static long _tsc_ps = 0;

long arch_syscall_kconf(int cmd, long arg)
{
	(void)arg;
	switch(cmd) {
		case KCONF_ARCH_TSC_PSPERIOD:
			return _tsc_ps;
			break;
		default:
			return -EINVAL;
	}
}

void arch_syscall_kconf_set_tsc_period(long ps)
{
	_tsc_ps = ps;
}
