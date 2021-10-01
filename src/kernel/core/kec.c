/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Kernel Emergency Console -- abstracted I/O console that uses some machine-specific console for
 * I/O. printk messages go through here. */

ssize_t kec_read_buffer(void *_buffer, size_t len, int flags);
ssize_t kec_read(void *buffer, size_t len, int flags);
ssize_t kec_write_buffer(void *_buffer, size_t len, int flags);

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#define DISCARD_ON_FULL 1

#define KEC_BUFFER_SZ 16384

#define MAX_SINGLE_WRITE 2048

struct kec_buffer {
	_Atomic uint64_t state;
	uint8_t buffer[KEC_BUFFER_SZ];
};

static struct kec_buffer kec_main_buffer;

#define IS_FULL(s) ({ (WRITE_HEAD(s) + 1) % KEC_BUFFER_SZ == READ_HEAD(s); })

#define IS_EMTPY(s) ({ READ_HEAD(s) == WRITE_HEAD(s); })

#define READ_HEAD(s) ({ (s) & 0xFFFF; })

#define WRITE_RESV(s) ({ ((s) >> 16) & 0xFFFF; })

#define WRITE_HEAD(s) ({ ((s) >> 32) & 0xFFFF; })

#define NEW_STATE(rh, wh, wr)                                                                      \
	({                                                                                             \
		(((uint64_t)rh) & 0xFFFF) | ((((uint64_t)wh) & 0xFFFF) << 32)                              \
		  | ((((uint64_t)wr) & 0xFFFF) << 16);                                                     \
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

static inline uint64_t RESERVE_WRITE(uint64_t state, size_t len)
{
	uint16_t wr = WRITE_RESV(state);
	uint16_t wh = WRITE_HEAD(state);
	uint16_t rh = READ_HEAD(state);

	int rturn = wr >= rh;
	int wturn = wr >= wh;
	wr = (wr + len) % KEC_BUFFER_SZ;
	int n_rturn = wr >= rh;
	int n_wturn = wr >= wh;

	if(rturn != n_rturn) {
		/* we passed the read head */
		rh = wr;
	}

	if(wturn != n_wturn) {
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
	return NEW_STATE(rh + *thislen, wh, WRITE_RESV(state));
}

static inline bool try_commit(struct kec_buffer *kb, uint64_t *old, uint64_t new)
{
	return atomic_compare_exchange_weak(&kb->state, old, new);
}

ssize_t kec_read(void *buffer, size_t len, int flags)
{
	return 0;
	// return machine_kec_read(buffer, len, flags);
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

ssize_t kec_write_buffer(void *_buffer, size_t len, int flags)
{
	uint8_t *buffer = _buffer;
	uint64_t state = atomic_load(&kec_main_buffer.state);
	uint64_t new_state = state;
	clamp(len, MAX_SINGLE_WRITE);
	do {
		uint64_t copy_offset;
		if(!reserve_space(state, len, !!(flags & DISCARD_ON_FULL), &new_state, &copy_offset)) {
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
