/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Kernel Emergency Console -- abstracted I/O console that uses some machine-specific console for
 * I/O. printk messages go through here. */

ssize_t kec_read_buffer(void *_buffer, size_t len, int flags);
ssize_t kec_read(void *buffer, size_t len, int flags);
ssize_t kec_write_buffer(const void *_buffer, size_t len, int flags);
void kec_init(void);
void kec_write(const void *buffer, size_t len, int flags);
void kec_add_to_read_buffer(void *buffer, size_t len);

#include <assert.h>
#include <spinlock.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#define KEC_WRITE_DISCARD_ON_FULL 1
#define KEC_WRITE_EMERGENCY 2
#define KEC_WRITE_ALLOWED_USER_BITS KEC_WRITE_DISCARD_ON_FULL

#define KEC_READ_NONBLOCK 1

#define KEC_BUFFER_SZ 16384
#define KEC_INPUT_BUFFER_SZ 4096
#define MAX_SINGLE_WRITE 2048

struct kec_buffer {
	_Atomic uint64_t state;
	uint8_t buffer[KEC_BUFFER_SZ];
	struct spinlock read_lock, write_lock;
	/* input buffer is not a ring buffer. it's not intended to be especially fast. */
	size_t read_pos;
	uint8_t read_buffer[KEC_INPUT_BUFFER_SZ];

	void (*write)(const void *, size_t, int);
};

static struct kec_buffer kec_main_buffer;

#define IS_FULL(s) ({ (WRITE_HEAD(s) + 1) % KEC_BUFFER_SZ == READ_HEAD(s); })

#define IS_EMTPY(s) ({ READ_HEAD(s) == WRITE_HEAD(s); })

#define READ_HEAD(s) ({ (s) & 0xFFFF; })

#define WRITE_RESV(s) ({ ((s) >> 16) & 0xFFFF; })

#define WRITE_HEAD(s) ({ ((s) >> 32) & 0xFFFF; })

#define NEW_STATE(rh, wh, wr)                                                                      \
	({                                                                                             \
		((uint64_t)((rh) % KEC_BUFFER_SZ) & 0xFFFF)                                                \
		  | ((((uint64_t)((wh) % KEC_BUFFER_SZ)) & 0xFFFF) << 32)                                  \
		  | ((((uint64_t)((wr) % KEC_BUFFER_SZ)) & 0xFFFF) << 16);                                 \
	})

#define clamp(x, l)                                                                                \
	do {                                                                                           \
		if((x) > (l))                                                                              \
			(x) = (l);                                                                             \
	} while(0)

/*
static inline size_t avail_space(uint64_t state)
{
    size_t space;

    uint16_t head = WRITE_HEAD(state);
    uint16_t tail = READ_HEAD(state);

    if(head > tail) {
        space = KEC_BUFFER_SZ - (head - tail);
    } else {
        space = head - tail;
    }
}*/

static inline bool did_pass(uint16_t x, uint16_t y, uint16_t l, uint16_t N)
{
	/*
	 * does x + l "pass" y? Pass means we our end point goes past this pointer in logical space. But
	 * since we're only storing phyiscal indicies, it's a little tricky.
	 * Remember, all variables are mod N.
	 * option 1: x < y; x + l < y ==> MAYBE
	 *    YES if x + l wraps; so, YES if x + l < x
	 *    NO otherwise
	 * option 2: x < y; x + l >= y ==> YES
	 * option 3: x >= y; x + l >= y ==> MAYBE
	 *    YES if x + lwraps; so, YES if x + l < x
	 * option 4: x >= y; x + l < y ==> NO
	 */

	assert(l < N);
	uint16_t next_x = (x + l) % N;
	bool did_wrap = !!(next_x < x);
	if(x < y) {
		return did_wrap || (next_x < y ? did_wrap : true);
	}
	return next_x >= y && did_wrap;
}

static inline uint64_t RESERVE_WRITE(uint64_t state, size_t len)
{
	uint16_t wr = WRITE_RESV(state);
	uint16_t wh = WRITE_HEAD(state);
	uint16_t rh = READ_HEAD(state);

	bool passed_rh = did_pass(wr, rh, len, KEC_BUFFER_SZ);
	bool passed_wh = did_pass(wr, wh, len, KEC_BUFFER_SZ);
	wr = (wr + len) % KEC_BUFFER_SZ;

	if(passed_rh) {
		rh = wr;
	}

	if(passed_wh) {
		/* we passed the write head -- this could happen if many threads are trying to write */
		wh = (wr - len) % KEC_BUFFER_SZ;
	}

	return NEW_STATE(rh, wh, wr);
}

static inline uint64_t COMMIT_WRITE(uint64_t state, size_t len)
{
	uint16_t wh = WRITE_HEAD(state);
	uint16_t wr = WRITE_RESV(state);
	assert((wh + len) % KEC_BUFFER_SZ == wr);
	return NEW_STATE(READ_HEAD(state), wh + len, wr);
}

static inline bool reserve_space(uint64_t state,
  size_t len,
  bool toss,
  uint64_t *new_state,
  uint64_t *copy_offset)
{
	*copy_offset = WRITE_HEAD(state);
	*new_state = RESERVE_WRITE(state, len);
	return (READ_HEAD(state) == READ_HEAD(*new_state)) || !toss;
}

static inline uint64_t read_advance(uint64_t state,
  size_t len,
  size_t *thislen,
  size_t *copy_offset)
{
	uint16_t wh = WRITE_HEAD(state);
	uint16_t rh = READ_HEAD(state);
	*copy_offset = rh;
	if(rh < wh) {
		*thislen = wh - rh;
	} else {
		*thislen = KEC_BUFFER_SZ - (rh - wh);
	}
	clamp(*thislen, len);
	return NEW_STATE(rh + *thislen, wh, WRITE_RESV(state));
}

static inline bool try_commit(struct kec_buffer *kb, uint64_t *old, uint64_t new)
{
	return atomic_compare_exchange_weak(&kb->state, old, new);
}

ssize_t kec_read(void *buffer, size_t len, int flags)
{
	spinlock_acquire_save(&kec_main_buffer.read_lock);
	if(kec_main_buffer.read_pos > 0) {
		size_t thislen = len;
		clamp(thislen, kec_main_buffer.read_pos);
		memcpy(buffer, kec_main_buffer.read_buffer, thislen);
		memmove(kec_main_buffer.read_buffer,
		  &kec_main_buffer.read_buffer[thislen],
		  KEC_INPUT_BUFFER_SZ - thislen);
		kec_main_buffer.read_pos -= thislen;
		spinlock_release_restore(&kec_main_buffer.read_lock);
		return thislen;
	} else {
		if(flags & KEC_READ_NONBLOCK) {
			return -EAGAIN;
		}
		/* TODO: block */
		panic("");
		spinlock_release_restore(&kec_main_buffer.read_lock);
	}
}

void kec_add_to_read_buffer(void *buffer, size_t len)
{
	spinlock_acquire_save(&kec_main_buffer.read_lock);
	clamp(len, KEC_INPUT_BUFFER_SZ - kec_main_buffer.read_pos);
	memcpy(&kec_main_buffer.read_buffer[kec_main_buffer.read_pos], buffer, len);
	kec_main_buffer.read_pos += len;
	spinlock_release_restore(&kec_main_buffer.read_lock);
}

void kec_write(const void *buffer, size_t len, int flags)
{
	if(!(flags & KEC_WRITE_EMERGENCY)) {
		/* not emergency -- we'll serialize */
		spinlock_acquire_save_recur(&kec_main_buffer.write_lock);
	}

	if(kec_main_buffer.write) {
		kec_main_buffer.write(buffer, len, flags);
	}
	kec_write_buffer(buffer, len, flags);

	if(!(flags & KEC_WRITE_EMERGENCY)) {
		spinlock_release_restore(&kec_main_buffer.write_lock);
	}
}

ssize_t kec_read_buffer(void *_buffer, size_t len, int flags)
{
	uint8_t *buffer = _buffer;
	uint64_t state = atomic_load(&kec_main_buffer.state);
	uint64_t new_state;
	size_t thislen, copy_offset;
	do {
		uint64_t rh = READ_HEAD(state);
		new_state = read_advance(state, len, &thislen, &copy_offset);

		size_t first_copy_len = thislen;
		size_t second_copy_len = 0;
		if(copy_offset + thislen > KEC_BUFFER_SZ) {
			first_copy_len = KEC_BUFFER_SZ - copy_offset;
			second_copy_len = thislen - first_copy_len;
		}
		memcpy(buffer, &kec_main_buffer.buffer[copy_offset], first_copy_len);
		memcpy(buffer + first_copy_len, kec_main_buffer.buffer, second_copy_len);
	} while(!try_commit(&kec_main_buffer, &state, new_state));
	return thislen;
}

ssize_t kec_write_buffer(const void *_buffer, size_t len, int flags)
{
	const uint8_t *buffer = _buffer;
	uint64_t state = atomic_load(&kec_main_buffer.state);
	uint64_t new_state = state;
	clamp(len, MAX_SINGLE_WRITE);
	do {
		uint64_t copy_offset;
		if(!reserve_space(
		     state, len, !!(flags & KEC_WRITE_DISCARD_ON_FULL), &new_state, &copy_offset)) {
			return -ENOSPC;
		}

		size_t first_copy_len = len;
		size_t second_copy_len = 0;
		if(copy_offset + len > KEC_BUFFER_SZ) {
			first_copy_len = KEC_BUFFER_SZ - copy_offset;
			second_copy_len = len - first_copy_len;
		}
		memcpy(&kec_main_buffer.buffer[copy_offset], buffer, first_copy_len);
		memcpy(kec_main_buffer.buffer, buffer + first_copy_len, second_copy_len);
		new_state = COMMIT_WRITE(new_state, len);
	} while(!try_commit(&kec_main_buffer, &state, new_state));

	return len;
}

void kec_init(void)
{
	// machine_kec_init(&kec_main_buffer);
}
