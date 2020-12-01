#pragma once
#include <guard.h>
#include <stdatomic.h>

struct thread;
struct spinlock {
	_Atomic uint32_t current_ticket;
	bool fl;
#if CONFIG_DEBUG_LOCKS
	const char *holder_file;
	int holder_line;
	struct thread *holder_thread;
#endif
#if 0
	_Alignas(64 /* TODO: constant */)
#endif
	_Atomic uint32_t next_ticket;
	_Atomic uint32_t holder;
};

#define DECLARE_SPINLOCK(name) struct spinlock name = { .current_ticket = 0, .next_ticket = 0 }

#define SPINLOCK_INIT                                                                              \
	(struct spinlock)                                                                              \
	{                                                                                              \
		.current_ticket = 0, .next_ticket = 0                                                      \
	}

bool __spinlock_acquire(struct spinlock *lock, const char *, int);
void __spinlock_release(struct spinlock *lock, bool, const char *, int);

#define spinlock_acquire(l) __spinlock_acquire(l, __FILE__, __LINE__)
#define spinlock_release(l, n) __spinlock_release(l, n, __FILE__, __LINE__)

#define spinlock_acquire_save(l) (l)->fl = __spinlock_acquire(l, __FILE__, __LINE__)
#define spinlock_release_restore(l) __spinlock_release(l, (l)->fl, __FILE__, __LINE__)
