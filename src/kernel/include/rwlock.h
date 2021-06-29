#pragma once

struct rwlock {
	_Atomic int32_t readers, writers;
	void *_Atomic wowner;
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
	uint8_t write;
	uint8_t recursed;
};
#define RWLOCK_TRY 1
#define RWLOCK_RECURSE 2

struct rwlock_result __rwlock_rlock(struct rwlock *rw, int flags, const char *, int);
struct rwlock_result __rwlock_upgrade(struct rwlock_result *rr, int flags, const char *, int);
struct rwlock_result __rwlock_downgrade(struct rwlock_result *rr, const char *, int);
struct rwlock_result __rwlock_wlock(struct rwlock *rw, int flags, const char *, int);

#define rwlock_wlock(a, b) __rwlock_wlock(a, b, __FILE__, __LINE__)
#define rwlock_rlock(a, b) __rwlock_rlock(a, b, __FILE__, __LINE__)
#define rwlock_upgrade(a, b) __rwlock_upgrade(a, b, __FILE__, __LINE__)
#define rwlock_downgrade(a) __rwlock_downgrade(a, __FILE__, __LINE__)
#define rwlock_wunlock(a) __rwlock_wunlock(a, __FILE__, __LINE__)
#define rwlock_runlock(a) __rwlock_runlock(a, __FILE__, __LINE__)

void __rwlock_wunlock(struct rwlock_result *rr, const char *, int);
void __rwlock_runlock(struct rwlock_result *rw, const char *, int);
void rwlock_init(struct rwlock *);
