/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <instrument.h>
#include <interrupt.h>
#include <processor.h>
#include <spinlock.h>
#include <stdatomic.h>

static __inline__ unsigned long long krdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

int __spinlock_try_acquire(struct spinlock *lock, const char *f __unused, int l __unused)
{
	register bool set = arch_interrupt_set(0);

	uint32_t our_ticket = atomic_fetch_add(&lock->next_ticket, 1);
	size_t tries = 0;
	while(1) {
		uint32_t cur = atomic_load(&lock->current_ticket);
		if(cur == our_ticket) {
			break;
		}
		//	if(f)
		//		printk("-----> %d %d %s %d\n", our_ticket, cur, f, l);
		if(our_ticket > cur) {
			uint32_t diff = our_ticket - cur;
			//	if(f)
			//		printk(":: %d %d %s %d\n", our_ticket, cur, f, l);
			while(diff--) {
				for(int i = 0; i < 100; i++) {
					arch_processor_relax();
				}
			}
		}
		if(tries++ > 5) {
			return 0;
		}
	}

#if CONFIG_DEBUG_LOCKS
	lock->holder_file = f;
	lock->holder_line = l;
	lock->holder_thread = current_thread;
#endif

	lock->holder = our_ticket;

	return set ? 3 : 1;
}

bool __spinlock_acquire(struct spinlock *lock, const char *f __unused, int l __unused)
{
	register bool set = arch_interrupt_set(0);
	long long a = krdtsc();

	uint32_t our_ticket = atomic_fetch_add(&lock->next_ticket, 1);
	size_t tries = 0;
	while(1) {
		uint32_t cur = atomic_load(&lock->current_ticket);
		if(cur == our_ticket) {
			break;
		}
		//	if(f)
		//		printk("-----> %d %d %s %d\n", our_ticket, cur, f, l);
		if(our_ticket > cur) {
			uint32_t diff = our_ticket - cur;
			//	if(f)
			//		printk(":: %d %d %s %d\n", our_ticket, cur, f, l);
			while(diff--) {
				for(int i = 0; i < 100; i++) {
					arch_processor_relax();
				}
			}
		}
		tries++;
#if CONFIG_DEBUG_LOCKS
		if(tries >= 10000000ul && f) {
			panic("POTENTIAL DEADLOCK in cpu %ld trying to acquire %s:%d (held from %s:%d by "
			      "cpu %ld) :: %d %d\n",
			  current_thread ? (long)current_thread->processor->id : -1,
			  f ? f : "??",
			  l,
			  lock->holder_file,
			  lock->holder_line,
			  lock->holder_thread ? (long)lock->holder_thread->processor->id : -1,
			  our_ticket,
			  cur);
		}
#endif
	}

#if 0
	while(atomic_fetch_or_explicit(&lock->data, 1, memory_order_acquire) & 1) {
		while(atomic_load_explicit(&lock->data, memory_order_acquire)) {
			arch_processor_relax();
#if CONFIG_DEBUG_LOCKS
			if(count++ == 100000000ul && f) {
				panic("POTENTIAL DEADLOCK in cpu %ld trying to acquire %s:%d (held from %s:%d by "
				      "cpu %ld)\n",
				  current_thread ? (long)current_thread->processor->id : -1,
				  f ? f : "??",
				  l,
				  lock->holder_file,
				  lock->holder_line,
				  lock->holder_thread ? (long)lock->holder_thread->processor->id : -1);
			}
#endif
		}
	}
#endif

#if CONFIG_DEBUG_LOCKS
	lock->holder_file = f;
	lock->holder_line = l;
	lock->holder_thread = current_thread;
#endif

	lock->holder = our_ticket;

	long long b = krdtsc();
	if(b - a > 1000000 && f) {
		// printk("LONG WAIT: %s %d :: %ld\n", f, l, b - a);
	}

	return set;
}

void __spinlock_release(struct spinlock *lock, bool flags, const char *f, int l)
{
	(void)f;
	(void)l;
	/* TODO: when not debugging locks, dont have these arguments */
#if CONFIG_DEBUG_LOCKS
	assert(lock->holder_thread == current_thread);
	lock->holder_file = NULL;
	lock->holder_line = 0;
	lock->holder_thread = NULL;
#endif
	// atomic_store_explicit(&lock->data, 0, memory_order_release);
	uint32_t hold = atomic_load(&lock->holder);
	uint32_t cur = atomic_fetch_add(&lock->current_ticket, 1);
	if(cur != hold) {
		panic("spinlock released while not held %s %d\n", f, l);
	}
	arch_interrupt_set(flags);
}
