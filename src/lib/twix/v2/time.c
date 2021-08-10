#include "v2.h"
#include <time.h>

long hook_nanosleep(struct syscall_args *args)
{
	struct timespec *spec = (void *)args->a0;
	int x = 0;
	return twz_thread_sync32(THREAD_SYNC_SLEEP, (_Atomic unsigned int *)&x, 0, spec);
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

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
			return -ENOTSUP;
	}
	return 0;
}
