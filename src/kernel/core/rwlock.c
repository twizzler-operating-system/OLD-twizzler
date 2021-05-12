#include <interrupt.h>
#include <rwlock.h>
#include <stdatomic.h>

#define RWLOCK_MAX_TRIES 100

#define DEBUG_RWLOCK 0

void rwlock_init(struct rwlock *rw)
{
	rw->readers = rw->writers = 0;
}

struct rwlock_result __rwlock_rlock(struct rwlock *rw, int flags, const char *file, int line)
{
#if DEBUG_RWLOCK
	printk("trying to get read lock %p: %s:%d\n", rw, file, line);
#endif
	register int set = arch_interrupt_set(0);
	uint32_t tries = 0;
	while(1) {
		atomic_fetch_add(&rw->readers, 1);
		if(atomic_load(&rw->writers) == 0) {
			break;
		}
		atomic_fetch_sub(&rw->readers, 1);
		while(atomic_load(&rw->writers)) {
			for(size_t p = 0; p < 100; p++)
				asm("pause");
			tries++;
			if(tries > RWLOCK_MAX_TRIES && (flags & RWLOCK_TRY)) {
				goto timeout;
			}
			if(tries > 1000000)
				panic("deadlock: %d %d", rw->readers, rw->writers);
		}
	}
	struct rwlock_result res = {
		.lock = rw,
		.int_flag = set,
		.res = RWLOCK_GOT,
		.write = 0,
	};
	assert(rw->readers > 0 && rw->writers >= 0);
	return res;

timeout:
	res = (struct rwlock_result){
		.lock = NULL,
		.int_flag = 0,
		.res = RWLOCK_TIMEOUT,
		.write = 0,
	};
	return res;
}

struct rwlock_result __rwlock_upgrade(struct rwlock_result *rr,
  int flags,
  const char *file,
  int line)
{
#if DEBUG_RWLOCK
	printk("trying to upgrade lock %p: %s:%d\n", rr->lock, file, line);
#endif
	assert(!rr->write);
	uint32_t tries = 0;
	while(1) {
		if(atomic_fetch_add(&rr->lock->writers, 1)) {
			atomic_fetch_sub(&rr->lock->writers, 1);
			while(atomic_load(&rr->lock->writers)) {
				for(size_t p = 0; p < 100; p++)
					asm("pause");
				tries++;
				if(tries > RWLOCK_MAX_TRIES && (flags & RWLOCK_TRY)) {
					goto timeout;
				}
			}
		} else {
			/* we've blocked writers, now. Wait for all readers to drain after we release our read
			 * lock */
			atomic_fetch_sub(&rr->lock->readers, 1);
			while(atomic_load(&rr->lock->readers)) {
				for(size_t p = 0; p < 100; p++)
					asm("pause");
				tries++;
				if(tries > RWLOCK_MAX_TRIES && (flags & RWLOCK_TRY)) {
					goto timeout;
				}
			}
			break;
		}
	}
	struct rwlock_result res = {
		.lock = rr->lock,
		.int_flag = rr->int_flag,
		.res = RWLOCK_GOT,
		.write = 1,
	};
	assert(rr->lock->readers >= 0 && rr->lock->writers > 0);
	return res;

timeout:
	res = (struct rwlock_result){
		.lock = NULL,
		.int_flag = 0,
		.res = RWLOCK_TIMEOUT,
		.write = 0,
	};
	return res;
}

struct rwlock_result __rwlock_downgrade(struct rwlock_result *rr, const char *file, int line)
{
#if DEBUG_RWLOCK
	printk("downgrade lock %p: %s:%d\n", rr->lock, file, line);
#endif
	assert(rr->write);
	/* always succeeds */
	atomic_fetch_add(&rr->lock->readers, 1);
	atomic_fetch_sub(&rr->lock->writers, 1);
	struct rwlock_result res = {
		.lock = rr->lock,
		.int_flag = rr->int_flag,
		.res = RWLOCK_GOT,
		.write = 0,
	};
	assert(rr->lock->readers > 0 && rr->lock->writers >= 0);
	return res;
}

struct rwlock_result __rwlock_wlock(struct rwlock *rw, int flags, const char *file, int line)
{
#if DEBUG_RWLOCK
	printk("trying to get write lock %p: %s:%d\n", rw, file, line);
#endif
	register int set = arch_interrupt_set(0);
	uint32_t tries = 0;
	while(1) {
		if(atomic_fetch_add(&rw->writers, 1)) {
			atomic_fetch_sub(&rw->writers, 1);
			while(atomic_load(&rw->writers)) {
				for(size_t p = 0; p < 100; p++)
					asm("pause");
				tries++;
				if(tries > RWLOCK_MAX_TRIES && (flags & RWLOCK_TRY)) {
					goto timeout;
				}
			}
		} else {
			while(atomic_load(&rw->readers)) {
				for(size_t p = 0; p < 100; p++)
					asm("pause");
				tries++;
				if(tries > RWLOCK_MAX_TRIES && (flags & RWLOCK_TRY)) {
					goto timeout;
				}
			}
			break;
		}
	}
	struct rwlock_result res = {
		.lock = rw,
		.int_flag = set,
		.res = RWLOCK_GOT,
		.write = 1,
	};
	assert(rw->readers >= 0 && rw->writers > 0);
	return res;

timeout:
	res = (struct rwlock_result){
		.lock = NULL,
		.int_flag = 0,
		.res = RWLOCK_TIMEOUT,
		.write = 0,
	};
	arch_interrupt_set(set);
	return res;
}

void __rwlock_wunlock(struct rwlock_result *rr, const char *file, int line)
{
#if DEBUG_RWLOCK
	printk("wunlock lock %p: %s:%d\n", rr->lock, file, line);
#endif
	assert(rr->write);
	assert(rr->lock->writers > 0);
	atomic_fetch_sub(&rr->lock->writers, 1);
	arch_interrupt_set(rr->int_flag);
}

void __rwlock_runlock(struct rwlock_result *rr, const char *file, int line)
{
#if DEBUG_RWLOCK
	printk("runlock lock %p: %s:%d\n", rr->lock, file, line);
#endif
	assert(!rr->write);
	assert(rr->lock->readers > 0);
	atomic_fetch_sub(&rr->lock->readers, 1);
	arch_interrupt_set(rr->int_flag);
}
