#pragma once

/** @file
 * @brief reader-writer lock functionality
 *
 * Supplies a reader-writer lock. The algorithm could use some work, but the semantics are there. */

struct rwlock {
	_Atomic int32_t readers, writers;
	void *_Atomic wowner;
};

/** An initialized for a reader-writer lock */
#define RWLOCK_INIT                                                                                \
	(struct rwlock)                                                                                \
	{                                                                                              \
		.readers = 0, .writers = 0                                                                 \
	}

#define RWLOCK_GOT 0
#define RWLOCK_TIMEOUT 1

/** The result of an rwlock lock call. Used to unlock or up/downgrade later. */
struct rwlock_result {
	struct rwlock *lock;
	int int_flag;
	/** The result of the call. If the lock call was allowed to timeout (RWLOCK_TRY), this can be
	 * RWLOCK_TIMEOUT, otherwise it must be RWLOCK_GOT. */
	short res;
	uint8_t write;
	uint8_t recursed;
};

#define RWLOCK_TRY 1
#define RWLOCK_RECURSE 2

/** Get a read-lock (shared with other readers).
 * @param rw the lock to lock
 * @param flags Can set RWLOCK_TRY to allowed failure to lock, or RWLOCK_RECURSE to allow recursing
 * for this CPU.
 * @return the result of the lock operation, as a rwlock_result.
 *
 * NOTE: you probably want to call the macro rwlock_rlock.
 */
struct rwlock_result __rwlock_rlock(struct rwlock *rw, int flags, const char *, int);

/** Upgrade a locked lock from read lock to write lock. For flags, see __rwlock_rlock.
 *
 * NOTE: you probably want to call the macro rwlock_upgrade.
 */
struct rwlock_result __rwlock_upgrade(struct rwlock_result *rr, int flags, const char *, int);

/** Downgrade a locked lock from write lock to read lock. For flags, see __rwlock_rlock.
 *
 * NOTE: you probably want to call the macro rwlock_downgrade.
 */
struct rwlock_result __rwlock_downgrade(struct rwlock_result *rr, const char *, int);

/** Get a write-lock (unique).
 * @param rw the lock to lock
 * @param flags Can set RWLOCK_TRY to allowed failure to lock, or RWLOCK_RECURSE to allow recursing
 * for this CPU.
 * @return the result of the lock operation, as a rwlock_result.
 *
 * NOTE: you probably want to call the macro rwlock_wlock.
 */
struct rwlock_result __rwlock_wlock(struct rwlock *rw, int flags, const char *, int);

#define rwlock_wlock(a, b) __rwlock_wlock(a, b, __FILE__, __LINE__)
#define rwlock_rlock(a, b) __rwlock_rlock(a, b, __FILE__, __LINE__)
#define rwlock_upgrade(a, b) __rwlock_upgrade(a, b, __FILE__, __LINE__)
#define rwlock_downgrade(a) __rwlock_downgrade(a, __FILE__, __LINE__)
#define rwlock_wunlock(a) __rwlock_wunlock(a, __FILE__, __LINE__)
#define rwlock_runlock(a) __rwlock_runlock(a, __FILE__, __LINE__)

/** Unlock a read-locked rwlock. */
void __rwlock_wunlock(struct rwlock_result *rr, const char *, int);

/** Unlock a write-locked rwlock. */
void __rwlock_runlock(struct rwlock_result *rw, const char *, int);

/** Initialize a rwlock struct */
void rwlock_init(struct rwlock *);
