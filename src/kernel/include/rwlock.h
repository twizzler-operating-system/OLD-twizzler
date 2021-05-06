#pragma once

struct rwlock {
	_Atomic uint32_t readers, writers;
};

#define RWLOCK_INIT                                                                                \
	(struct rwlock)                                                                                \
	{                                                                                              \
		.readers = 0, .writers = 0                                                                 \
	}

#define RWLOCK_GOT 0
#define RWLOCK_TIMEOUT 1

struct rwlock_result {
	struct rwlock *lock;
	int int_flag;
	short res;
	short write;
};
#define RWLOCK_TRY 1

struct rwlock_result rwlock_rlock(struct rwlock *rw, int flags);
struct rwlock_result rwlock_upgrade(struct rwlock_result *rr, int flags);
struct rwlock_result rwlock_downgrade(struct rwlock_result *rr);
struct rwlock_result rwlock_wlock(struct rwlock *rw, int flags);
void rwlock_wunlock(struct rwlock_result *rr);
void rwlock_runlock(struct rwlock_result *rw);
void rwlock_init(struct rwlock *);
