#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <twz/alloc.h>
#include <twz/obj.h>
#include <twz/persist.h>

#define CANARY 0x55aa66bb
#define MAGIC 0x5a8ab49b

#define NR_SMALL 31
#define NR_LARGE 22

#define small_scaling(x) ((x)*16 + 32)

#define MAX_CHUNK_SIZE 16 * 1024

#define MAX_SMALL 512

#define MAX_BUCKET 8

#define MAX_ALIGN 4096

const uint16_t large_sizes[] = {
	36 * 16,
	42 * 16,
	50 * 16,
	63 * 16,
	72 * 16,
	84 * 16,
	102 * 16,
	127 * 16,
	146 * 16,
	170 * 16,
	204 * 16,
	255 * 16,
	292 * 16,
	340 * 16,
	409 * 16,
	511 * 16,
	584 * 16,
	682 * 16,
	750 * 16,
	818 * 16,
	950 * 16,
	1024 * 16,
};

_Static_assert(sizeof(large_sizes) / sizeof(large_sizes[0]) == NR_LARGE, "");

#define ALIGNED 1
#define RELEASED 2
#define ALLOCED 4

struct chunk {
	uint32_t canary;
	uint16_t flags;
	/* specifies offset when a phantom chunk (for recovering from alignment) */
	uint16_t off;
	uint32_t len;
	/* gets size of the chunk header to aligned on 16 */
	uint32_t pad;

	/* only for free chunks */
	uint32_t nxt;
	uint32_t prv;
};

#define ALLOC_CHUNK_HDR_SZ 16
_Static_assert(offsetof(struct chunk, pad) + sizeof(uint32_t) == ALLOC_CHUNK_HDR_SZ, "");
_Static_assert(small_scaling(NR_SMALL - 1) == MAX_SMALL, "");

#define VERIFY 1

#if 0
#define log(x, ...) fprintf(stderr, "\e[35m" x "\e[0m\n", ##__VA_ARGS__)
#define log2(x, ...) fprintf(stderr, "\e[41m" x "\e[0m\n", ##__VA_ARGS__)
#else
#define log(...)
#define log2(...)
#endif

struct mutex {
	int lock;
};

/* TODO */
#define mutex_acquire(x)
#define mutex_release(x)
#define mutex_init(x)

#define HDR_F_VOLATILE 1

struct alloc_hdr {
	struct mutex lock;
	size_t len;

	/* fast bins -- contain a limited number of recently freed chunks, organized by size class.
	 * Chunks that are split can result in a second chunk that can end up in these bins too. */
	uint32_t bins[NR_SMALL + NR_LARGE][MAX_BUCKET];
	/* unsorted list of chunks. Acts as a "catch all" for chunks when we cant fit them into a fast
	 * bin, or if we just need to put them somewhere. */
	uint32_t unsorted;
	/* sorted list of chunks, keyed by address. The sorted list is used to coalesce chunks so we can
	 * recover space. If we can't fit a chunk into a fast bin, we try to sort it (but give up if it
	 * takes too long, falling back to unsorted). */
	uint32_t sorted;
	/* list of huge chunks. These are so big they don't have a size class, so they cannot go into a
	 * fast bin. While huge chunks try to get coalesced in sorted too, they do need a place to end
	 * up if we can't fit them somewhere. */
	uint32_t huge;
	/* in realloc, we might get interrupted partway through, so keep track of some temporary work */
	void *tofree;
	/* These two describe the top of the heap, where top is where can allocate a fresh chunk from
	 * next, and high watermark indicates up till where we still have allocated pages. */
	uint32_t top;
	uint32_t high_watermark;
	/* these are used in the transaction system, which operates internally. tmpend is used when
	 * we're adding to the log without committing the log, and end is where the committed part of
	 * the log ends. */
	uint32_t tmpend;
	uint32_t end;
	uint32_t logsz;
	uint32_t flags;
	uint32_t magic;
	uint64_t pad2;
	/* the transaction log is placed directly after the header */
	char log[];
};

struct alloc_req {
	size_t len;
	void *data;
	void **owner;
	uint64_t flags;
	void (*ctor)(void *, void *);
	twzobj *obj;
};

#define ALIGN(x, a) __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

static inline int is_aligned(uint64_t x, uint32_t a)
{
	return a == 0 || ALIGN(x, a) == x;
}

#define alignment_from_flags(f) ({ (f) >> 32; })

struct log_entry {
	/* location of the thing we're logging, relative to the alloc_hdr */
	int64_t offset_from_hdr;
	uint16_t len;
	uint16_t next;
	uint32_t pad;
	char value[];
};

#define LOG_SIZE 2048

_Static_assert(LOG_SIZE > (sizeof(struct log_entry) + 8) * 8 * MAX_BUCKET, "");

static inline int __tx_add_commit(struct alloc_hdr *hdr)
{
	/* first commit the log, then commit the committed end ptr */
	_clwb_len(hdr->log, hdr->tmpend);
	_pfence();
	hdr->end = hdr->tmpend;
	_clwb(&hdr->end);
	_pfence();
	return 0;
}

#define ADD_COMMIT 1
#define EXT_PTR 2

static inline int64_t tx_ptr_make(struct alloc_hdr *hdr, void *ptr)
{
	intptr_t h = (intptr_t)hdr;
	intptr_t p = (intptr_t)ptr;
	return p - h;
}

static inline void *tx_ptr_break(struct alloc_hdr *hdr, int64_t p)
{
	intptr_t h = (intptr_t)hdr;
	return (void *)(p + h);
}

static inline int __tx_is_dup(struct alloc_hdr *hdr, void *p)
{
	if(hdr->flags & HDR_F_VOLATILE)
		return 0;
	uint32_t e = 0;
	/* flush everything that we've recorded (or, if we're recovering, first restore the old value),
	 * and then reset the end ptr to 0 and flush that. */
	while(e < hdr->tmpend) {
		struct log_entry *entry = (void *)&hdr->log[e];
		if(entry->offset_from_hdr == tx_ptr_make(hdr, p)) {
			return 1;
		}
		e += entry->next;
	}
	return 0;
}

static inline int __tx_add(struct alloc_hdr *hdr, void *p, uint16_t len, int flags)
{
	if(hdr->flags & HDR_F_VOLATILE)
		return 0;
	if(__tx_is_dup(hdr, p))
		return 0;
	if(hdr->tmpend + sizeof(struct log_entry) + len > hdr->logsz) {
		abort();
	}
	struct log_entry *entry = (void *)&hdr->log[hdr->tmpend];
	entry->offset_from_hdr = tx_ptr_make(hdr, p);
	entry->len = len;
	switch(len) {
		case 2:
			*(uint16_t *)entry->value = *(uint16_t *)p;
			break;
		case 4:
			*(uint32_t *)entry->value = *(uint32_t *)p;
			break;
		case 8:
			*(uint64_t *)entry->value = *(uint64_t *)p;
			break;
		default:
			memcpy(entry->value, p, len);
			break;
	}
	uint16_t next = sizeof(*entry) + len;
	next = (next + 7) & ~7;
	entry->next = next;
	hdr->tmpend += next;
	if(flags & ADD_COMMIT) {
		__tx_add_commit(hdr);
	}
	return 0;
}

static inline int __same_line(void *a, void *b)
{
	return ((uintptr_t)a & ~(__CL_SIZE - 1)) == ((uintptr_t)b & ~(__CL_SIZE - 1));
}

static inline void __tx_cleanup(struct alloc_hdr *hdr, int abort)
{
	if(hdr->flags & HDR_F_VOLATILE)
		return;
	uint32_t e = 0;
	void *last_vp = NULL;
	long long last_len = 0;
	/* flush everything that we've recorded (or, if we're recovering, first restore the old value),
	 * and then reset the end ptr to 0 and flush that. */
	while(e < hdr->end) {
		struct log_entry *entry = (void *)&hdr->log[e];
		void *vp = tx_ptr_break(hdr, entry->offset_from_hdr);
		if(abort) {
			log("RECOVER! %d %p %ld %d\n", hdr->top, vp, entry->offset_from_hdr, entry->len);
			memcpy(vp, entry->value, entry->len);
		}

		char *l = vp;
		char *last_l = last_vp;
		long long rem = entry->len;
		while(rem > 0) {
			if(last_len == 0 || !__same_line(last_l, l)) {
				_clwb(l);
			}
			uint32_t off = (uintptr_t)l & (__CL_SIZE - 1);
			uint32_t last_off = (uintptr_t)last_l & (__CL_SIZE - 1);
			l += (__CL_SIZE - off);
			last_l += (__CL_SIZE - off);
			rem -= (__CL_SIZE - off);
			last_len -= (__CL_SIZE - last_off);
		}

		last_len = entry->len;
		last_vp = vp;
		e += entry->next;
	}

	if(hdr->end) {
		_pfence();
		hdr->end = 0;
		_clwb(&hdr->end);
		_pfence();
	}
	hdr->tmpend = 0;
}

static inline void __tx_commit(struct alloc_hdr *hdr)
{
	__tx_cleanup(hdr, 0);
}

static inline void __tx_abort(struct alloc_hdr *hdr)
{
	__tx_cleanup(hdr, 1);
}

#define TX_ALLOC_BEGIN(hdr)                                                                        \
	assert(hdr->end == 0 && hdr->tmpend == 0);                                                     \
	__tx_abort(hdr);

#define TX_ALLOC_END(hdr) __tx_commit(hdr)

#define TX_ALLOC_RECORD(hdr, p, f) __tx_add(hdr, p, sizeof(*p), f)

#define TX_ALLOC_RECOVER(hdr) __tx_abort(hdr)

static inline int get_size_class(size_t len)
{
	/* get size class that will certainly be able to service a request of length len */
	if(len <= MAX_SMALL) {
		for(unsigned i = 0; i < NR_SMALL; i++) {
			if(small_scaling(i) >= len)
				return i;
		}
	}
	for(int i = 0; i < NR_LARGE; i++) {
		if(large_sizes[i] >= len)
			return i + NR_SMALL;
	}
	return -1;
}

static inline size_t get_size_from_sc(int size_class)
{
	if(size_class == -1) {
		abort();
	}
	if(size_class < NR_SMALL)
		return small_scaling(size_class);
	else {
		return large_sizes[size_class - NR_SMALL];
	}
}

static inline int get_size_class_for_chunk(struct chunk *chunk)
{
	/* get the right size class to put a chunk into for a fast bin */
	int sc = get_size_class(chunk->len);
	if(sc == -1)
		return -1;
	size_t size_sc = get_size_from_sc(sc);
	if(size_sc > chunk->len && sc > 0) {
		sc--;
	}
	assert(get_size_from_sc(sc) <= chunk->len);
	return sc;
}

static void try_release_pages(struct alloc_hdr *hdr, uint32_t start, uint32_t end)
{
	// TODO system
	// TODO: adjust for hdr location
	start = ALIGN(start, 4096);
	end = ALIGN(end, 4096);
	end -= 4096;
	if(end <= start)
		return;
	for(uint32_t p = start; p < end; p += 4096) {
		// TODO free page
	}
}

static inline struct chunk *follow_chunk_ptr(struct alloc_hdr *hdr, uint32_t ptr)
{
	char *mem = (char *)hdr;
	return (struct chunk *)(mem + ptr);
}

static inline uint32_t offset_from_chunk_ptr(struct alloc_hdr *hdr, void *p)
{
	char *mem = (char *)hdr;
	return (uint32_t)(long)((char *)p - (uintptr_t)mem);
}

static void add_chunk_to_list(struct alloc_hdr *hdr, struct chunk *chunk, uint32_t *list)
{
	/* TX (external) */
	/* EXT_PTR TRANSACTION */
	if(*list) {
		struct chunk *root = follow_chunk_ptr(hdr, *list);
		struct chunk *prev = follow_chunk_ptr(hdr, root->prv);

		TX_ALLOC_RECORD(hdr, &chunk->prv, 0);
		TX_ALLOC_RECORD(hdr, &chunk->nxt, 0);
		TX_ALLOC_RECORD(hdr, &prev->nxt, 0);
		TX_ALLOC_RECORD(hdr, &root->prv, 0);
		TX_ALLOC_RECORD(hdr, list, ADD_COMMIT);

		chunk->prv = root->prv;
		chunk->nxt = prev->nxt;
		prev->nxt = offset_from_chunk_ptr(hdr, chunk);
		root->prv = offset_from_chunk_ptr(hdr, chunk);
		*list = offset_from_chunk_ptr(hdr, chunk);
	} else {
		TX_ALLOC_RECORD(hdr, &chunk->prv, 0);
		TX_ALLOC_RECORD(hdr, &chunk->nxt, 0);
		TX_ALLOC_RECORD(hdr, list, ADD_COMMIT);
		*list = chunk->prv = chunk->nxt = offset_from_chunk_ptr(hdr, chunk);
	}
}

/* similar to above, except we ensure the list remains sorted. O(n). We also support "giveup", where
 * if we have to iterate past too many chunks before we can insert, we just give up and return
 * failure. If give up is -1, we do not give up. */
static int add_chunk_to_sorted_list(struct alloc_hdr *hdr,
  struct chunk *chunk,
  uint32_t *list,
  int give_up)
{
	/* TX (external) */
	/* EXT_PTR TRANSACTION */
	if(*list) {
		/* this first part of the loop is read-only */
		struct chunk *root = follow_chunk_ptr(hdr, *list);
		uint32_t c = offset_from_chunk_ptr(hdr, chunk);
		uint32_t r = offset_from_chunk_ptr(hdr, root);
		int wrap = 0;
		int count = 0;
		while(r < c) {
			if(give_up >= 0 && count > give_up) {
				return 0;
			}
			root = follow_chunk_ptr(hdr, root->nxt);
			r = offset_from_chunk_ptr(hdr, root);
			if(r == *list) {
				wrap = 1;
				break;
			}
			count++;
		}

		struct chunk *prev = follow_chunk_ptr(hdr, root->prv);

		TX_ALLOC_RECORD(hdr, &chunk->prv, 0);
		TX_ALLOC_RECORD(hdr, &chunk->nxt, 0);
		TX_ALLOC_RECORD(hdr, &prev->nxt, 0);
		TX_ALLOC_RECORD(hdr, &root->prv, ADD_COMMIT);

		chunk->prv = root->prv;
		chunk->nxt = prev->nxt;
		prev->nxt = offset_from_chunk_ptr(hdr, chunk);
		root->prv = offset_from_chunk_ptr(hdr, chunk);
		if(!wrap && r == *list) {
			TX_ALLOC_RECORD(hdr, list, ADD_COMMIT);
			*list = offset_from_chunk_ptr(hdr, chunk);
		}
	} else {
		/* this internally contributes to a transaction */
		add_chunk_to_list(hdr, chunk, list);
	}
	return 1;
}

static void remove_chunk_from_list(struct alloc_hdr *hdr, struct chunk *chunk, uint32_t *list)
{
	/* EXT_PTR TRANSACTION */
	struct chunk *prev = follow_chunk_ptr(hdr, chunk->prv);
	struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
	TX_ALLOC_RECORD(hdr, &next->prv, 0);
	TX_ALLOC_RECORD(hdr, &prev->nxt, 0);
	TX_ALLOC_RECORD(hdr, &chunk->nxt, 0);
	TX_ALLOC_RECORD(hdr, &chunk->prv, ADD_COMMIT);
	next->prv = chunk->prv;
	prev->nxt = chunk->nxt;
	if(*list == offset_from_chunk_ptr(hdr, chunk)) {
		TX_ALLOC_RECORD(hdr, list, ADD_COMMIT);
		*list = chunk->nxt == *list ? 0 : chunk->nxt;
	}
	chunk->prv = chunk->nxt = 0;
}

static inline int is_huge(struct chunk *chunk)
{
	return get_size_class_for_chunk(chunk) == -1;
}

static inline uint32_t *get_unsorted_list_head(struct alloc_hdr *hdr)
{
	return &hdr->unsorted;
}

static inline uint32_t *get_sorted_list_head(struct alloc_hdr *hdr)
{
	return &hdr->sorted;
}

static inline uint32_t *get_huge_list_head(struct alloc_hdr *hdr)
{
	return &hdr->huge;
}

static inline size_t chunk_size(struct chunk *chunk)
{
	return chunk->len;
}

static inline int add_to_bucket(struct alloc_hdr *hdr, struct chunk *chunk)
{
	int size_class = get_size_class_for_chunk(chunk);
	if(size_class == -1) {
		uint32_t *list = get_huge_list_head(hdr);
		add_chunk_to_list(hdr, chunk, list);
		return 1;
	} else {
		for(int i = 0; i < MAX_BUCKET; i++) {
			log("  try %d %d: %d\n", size_class, i, hdr->bins[size_class][i]);
			if(hdr->bins[size_class][i] == 0) {
				TX_ALLOC_RECORD(hdr, &hdr->bins[size_class][i], ADD_COMMIT);
				hdr->bins[size_class][i] = offset_from_chunk_ptr(hdr, chunk);
				return 1;
			}
		}
	}
	return 0;
}

static void verify_allocator(struct alloc_hdr *hdr)
{
	(void)hdr;
	/* TODO */
}

#define FAIL_TO_LISTS 1
static void add_chunk_to_main_list(struct alloc_hdr *hdr, struct chunk *chunk, int flags)
{
	/* EXT_PTR TRANSACTION */
	if(is_huge(chunk)) {
		uint32_t *list = get_huge_list_head(hdr);
		add_chunk_to_list(hdr, chunk, list);
	} else {
		if(!add_to_bucket(hdr, chunk)) {
			if(flags & FAIL_TO_LISTS) {
				uint32_t *list = get_sorted_list_head(hdr);
				if(!add_chunk_to_sorted_list(hdr, chunk, list, 16)) {
					list = get_unsorted_list_head(hdr);
					add_chunk_to_list(hdr, chunk, list);
				}
			} else {
				abort();
			}
		}
	}
}

static inline void do_ctor(struct alloc_hdr *hdr, struct chunk *chunk, struct alloc_req *req)
{
	(void)hdr;
	void *p = (void *)((uintptr_t)chunk + ALLOC_CHUNK_HDR_SZ);
	if(req->ctor == TWZ_ALLOC_CTOR_ZERO) {
		memset(p, 0, req->len);
		_clwb_len(p, req->len);
	} else if(req->ctor != NULL) {
		req->ctor(p, req->data);
		_clwb_len(p, req->len);
	}
	/* we can avoid the persist barrier because we know it'll get called to commit the transaction
	 * for writing the owned pointer. */
}

static void *make_external_ptr(struct alloc_hdr *hdr, struct chunk *chunk, struct alloc_req *req)
{
	/* alignment: must be minimum of 16, and multiples of 16 */
	uint32_t align = alignment_from_flags(req->flags);
	align = ALIGN(align, 16);
	if(align > MAX_ALIGN)
		align = MAX_ALIGN;
	uintptr_t p = ((uintptr_t)chunk + ALLOC_CHUNK_HDR_SZ) - (uintptr_t)hdr;
	if(!is_aligned(p, align)) {
		/* make phantom chunk */
		uintptr_t orig = p;
		p = ALIGN(p, align);
		struct chunk *phantom = (struct chunk *)((p + (uintptr_t)hdr) - ALLOC_CHUNK_HDR_SZ);
		TX_ALLOC_RECORD(hdr, &phantom->flags, 0);
		TX_ALLOC_RECORD(hdr, &phantom->off, 0);
		TX_ALLOC_RECORD(hdr, &phantom->len, 0);
		TX_ALLOC_RECORD(hdr, &phantom->canary, 0);
		phantom->flags = ALIGNED | ALLOCED;
		phantom->canary = CANARY;
		phantom->off = p - orig;
		phantom->len = chunk->len - phantom->off;
		log("    ALIGNED orig: %lx, p: %lx (off: %lx) :: %d %d",
		  orig,
		  p,
		  phantom->off,
		  offset_from_chunk_ptr(hdr, chunk),
		  offset_from_chunk_ptr(hdr, phantom));
	}
	if(!(chunk->flags & ALLOCED) || (chunk->flags & RELEASED)) {
		TX_ALLOC_RECORD(hdr, &chunk->flags, 0);
		chunk->flags = ALLOCED;
	} else {
		log("elided header write");
	}
	do_ctor(hdr, chunk, req);
	void *rp = (void *)(p + (uintptr_t)hdr);
	rp = twz_ptr_local(rp);
	return rp;
}

static int create_chunk(struct alloc_hdr *hdr, struct alloc_req *req)
{
	/* transactionally allocates new chunk at top and adds it to the correct bin.
	 * If we fail after we commit but before we return, we do no leak because the
	 * memory is now in the bin, so it can be found. */
	/* TX */
	/* INTERNAL TRANSACTION */
	// chunk init, list add
	if(hdr->top + req->len > hdr->len) {
		return 0;
	}
	struct chunk *chunk = follow_chunk_ptr(hdr, hdr->top);

	TX_ALLOC_BEGIN(hdr)
	{
		TX_ALLOC_RECORD(hdr, &hdr->top, 0);
		TX_ALLOC_RECORD(hdr, &hdr->high_watermark, 0);
		TX_ALLOC_RECORD(hdr, &chunk->canary, 0);
		void *p = make_external_ptr(hdr, chunk, req);
		TX_ALLOC_RECORD(hdr, &chunk->len, 0);
		TX_ALLOC_RECORD(hdr, req->owner, EXT_PTR | ADD_COMMIT);
		hdr->top += req->len;
		if(hdr->top > hdr->high_watermark)
			hdr->high_watermark = hdr->top;
		chunk->canary = CANARY;
		chunk->flags = ALLOCED;
		chunk->len = req->len;
		*req->owner = p;
	}
	TX_ALLOC_END(hdr);

	return 1;
}

static void put_chunk_somewhere(struct alloc_hdr *hdr, struct chunk *chunk)
{
	/* TX external */
	if(is_huge(chunk)) {
		log("  trying to sort");
		uint32_t *list = get_sorted_list_head(hdr);
		if(!add_chunk_to_sorted_list(hdr, chunk, list, -1)) {
			log("  failed, adding to unsorted");
			list = get_unsorted_list_head(hdr);
			add_chunk_to_list(hdr, chunk, list);
		}
	} else {
		add_chunk_to_main_list(hdr, chunk, FAIL_TO_LISTS);
		log("  adding to bin");
	}
}

static inline int can_split(struct chunk *chunk, size_t len)
{
	/* We cannot split a chunk of size class 0 or 1, because this would create a chunk with size
	 * class < 0. We also cannot split a chunk of SC x into a chunk of SC x-1, as this would also
	 * create a chunk of SC < 0. */
	return chunk->len > len && chunk->len - len >= get_size_from_sc(0);
}

static int chunk_split(struct alloc_hdr *hdr,
  struct chunk *chunk,
  uint32_t *list,
  struct alloc_req *req)
{
	/* split a chunk into two chunks, where the resulting "first" one of them will be returned by
	 * the allocator. The second one will have the remaining length, and will end up "somewhere". */
	/* TX */
	// 1 list remove, 2 list adds
	/* INTERNAL TRANSACTION */
	if(!can_split(chunk, req->len))
		return 0;
	size_t orig_len = chunk_size(chunk);
	struct chunk *newchunk = (struct chunk *)((char *)chunk + req->len);

	TX_ALLOC_BEGIN(hdr)
	{
		remove_chunk_from_list(hdr, chunk, list);

		TX_ALLOC_RECORD(hdr, &chunk->len, 0);
		/* OPT: we could avoid recording these, and just make sure to flush them */
		TX_ALLOC_RECORD(hdr, &newchunk->len, 0);
		TX_ALLOC_RECORD(hdr, &newchunk->canary, ADD_COMMIT);

		chunk->len = req->len;
		newchunk->len = orig_len - req->len;
		newchunk->canary = CANARY;
		newchunk->off = 0;
		newchunk->flags = 0;
		assert(orig_len == chunk_size(chunk) + chunk_size(newchunk));
		/* we don't really care where the new chunk ends up */
		put_chunk_somewhere(hdr, newchunk);

		void *p = make_external_ptr(hdr, chunk, req);
		TX_ALLOC_RECORD(hdr, req->owner, EXT_PTR | ADD_COMMIT);
		*req->owner = p;
	}
	TX_ALLOC_END(hdr);
	return 1;
}

static inline int can_coalesce(struct alloc_hdr *hdr, struct chunk *chunk1, struct chunk *chunk2)
{
	return offset_from_chunk_ptr(hdr, chunk1) + chunk_size(chunk1)
	       == offset_from_chunk_ptr(hdr, chunk2);
}

static void chunk_coalesce(struct alloc_hdr *hdr, struct chunk *chunk1, struct chunk *chunk2)
{
	/* put two chunks together. They must be adjacent, and chunk1 must be "before" chunk2, and they
	 * must be on the same list. This means that we know that the list is either pointing to chunk1
	 * (and so wont change) or is pointing to neither chunk1 or chunk2 (and so wont change). */
	/* TX */
	/* INTERNAL TRANSACTION */
	// 2 list removes, 1 list add
	if(!can_coalesce(hdr, chunk1, chunk2))
		return;

	size_t new_sz = chunk_size(chunk1) + chunk_size(chunk2);
	TX_ALLOC_BEGIN(hdr)
	{
		struct chunk *next = follow_chunk_ptr(hdr, chunk2->nxt);
		TX_ALLOC_RECORD(hdr, &chunk2->nxt, 0);
		TX_ALLOC_RECORD(hdr, &chunk2->prv, 0);
		TX_ALLOC_RECORD(hdr, &next->prv, 0);
		TX_ALLOC_RECORD(hdr, &chunk1->nxt, 0);
		TX_ALLOC_RECORD(hdr, &chunk1->len, ADD_COMMIT);
		next->prv = chunk2->prv;
		chunk1->nxt = chunk2->nxt;
		chunk2->nxt = chunk2->prv = 0;
		chunk1->len = new_sz;
	}
	TX_ALLOC_END(hdr);
	assert(chunk_size(chunk1) == new_sz);
}

static int try_find_in_bin(struct alloc_hdr *hdr, int size_class, struct alloc_req *req)
{
	if(size_class == -1)
		return 0;
	uint32_t *entry = NULL;
	for(int i = 0; i < MAX_BUCKET; i++) {
		if(hdr->bins[size_class][i]) {
			entry = &hdr->bins[size_class][i];
			break;
		}
	}
	if(entry) {
		struct chunk *chunk = follow_chunk_ptr(hdr, *entry);
		TX_ALLOC_BEGIN(hdr)
		{
			TX_ALLOC_RECORD(hdr, entry, 0);
			void *p = make_external_ptr(hdr, chunk, req);
			TX_ALLOC_RECORD(hdr, req->owner, EXT_PTR | ADD_COMMIT);
			*entry = 0;
			*req->owner = p;
		}
		TX_ALLOC_END(hdr);
		return 1;
	}
	return 0;
}

static int try_steal(struct alloc_hdr *hdr, struct alloc_req *req)
{
	/* INTERNAL TRANSACTION */
	uint32_t *sorted = get_sorted_list_head(hdr);
	if(*sorted) {
		/* this is read-only, except for splitting, which internally is transactional */
		struct chunk *chunk = follow_chunk_ptr(hdr, *sorted);
		do {
			struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
			if(chunk_split(hdr, chunk, sorted, req)) {
				log("  stolen from sorted");
				return 1;
			}
			chunk = next;
		} while(*sorted && offset_from_chunk_ptr(hdr, chunk) != *sorted);
	}
	/* this is read-only, except for splitting, which internally is transactional */
	uint32_t *huge = get_huge_list_head(hdr);
	if(*huge) {
		struct chunk *chunk = follow_chunk_ptr(hdr, *huge);
		do {
			struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
			if(chunk_split(hdr, chunk, huge, req)) {
				log("  stolen from huge");
				return 1;
			}
			chunk = next;
		} while(*huge && offset_from_chunk_ptr(hdr, chunk) != *huge);
	}
	return 0;
}

static int try_unsorted(struct alloc_hdr *hdr, struct alloc_req *req)
{
	/* INTERNAL TRANSACTION */
	uint32_t *list = get_unsorted_list_head(hdr);
	if(!*list)
		return 0;
	struct chunk *chunk = follow_chunk_ptr(hdr, *list);
	do {
		struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);

		/* if our request fits in this chunk (with some amount of leniency for oversized chunks),
		 * just use that. */
		if(chunk->len >= (uint32_t)req->len && chunk->len < (uint32_t)req->len + 64) {
			TX_ALLOC_BEGIN(hdr)
			{
				remove_chunk_from_list(hdr, chunk, list);
				void *p = make_external_ptr(hdr, chunk, req);
				TX_ALLOC_RECORD(hdr, req->owner, EXT_PTR | ADD_COMMIT);
				*req->owner = p;
			}
			TX_ALLOC_END(hdr);
			return 1;
		}
		log("forcing sort of chunk sc %d %d\n", chunk->len, offset_from_chunk_ptr(hdr, chunk));
		/* while we're here, let's sort any unsorted chunk we come across that doesn't work. */
		uint32_t *sorted = get_sorted_list_head(hdr);
		TX_ALLOC_BEGIN(hdr)
		{
			remove_chunk_from_list(hdr, chunk, list);
			add_chunk_to_sorted_list(hdr, chunk, sorted, -1);
		}
		TX_ALLOC_END(hdr);

		chunk = next;
	} while(*list);
	return 0;
}

static int try_reclaim_top(struct alloc_hdr *hdr, struct chunk *chunk, uint32_t *list, void **owner)
{
	/* INTERNAL TRANSACTION */
	/* TX */
	if(hdr->top == offset_from_chunk_ptr(hdr, chunk) + chunk_size(chunk)) {
		TX_ALLOC_BEGIN(hdr)
		{
			if(list)
				remove_chunk_from_list(hdr, chunk, list);
			log("  reclaim top by %ld\n", chunk_size(chunk));
			if(owner)
				TX_ALLOC_RECORD(hdr, owner, EXT_PTR);
			TX_ALLOC_RECORD(hdr, &hdr->top, ADD_COMMIT);
			if(owner)
				*owner = NULL;
			hdr->top -= chunk_size(chunk);
		}
		TX_ALLOC_END(hdr);
		try_release_pages(hdr, hdr->top, hdr->high_watermark);
		return 1;
	}
	return 0;
}

static void dump_all_bins(struct alloc_hdr *hdr, int large)
{
	/* INTERNAL TRANSACTION */
	log("dumping all bins");
	uint32_t *sorted = get_sorted_list_head(hdr);
	for(int i = 0; i < NR_SMALL + (large ? NR_LARGE : 0); i++) {
		TX_ALLOC_BEGIN(hdr)
		{
			for(int j = 0; j < MAX_BUCKET; j++) {
				if(hdr->bins[i][j]) {
					struct chunk *chunk = follow_chunk_ptr(hdr, hdr->bins[i][j]);
					TX_ALLOC_RECORD(hdr, &hdr->bins[i][j], ADD_COMMIT);
					hdr->bins[i][j] = 0;
					add_chunk_to_sorted_list(hdr, chunk, sorted, -1);
				}
			}
		}
		TX_ALLOC_END(hdr);
	}
}

/* coalesce the sorted list, trying to combined chunks. There are several options for how much time
 * to spend doing this:
 *     heavy = 0: do a single pass, just trying to coalesce
 *     heavy = 1: do a pass, and if there were more than 32 items in the sorted list, set heavy = 2
 *     heavy = 2: do a pass, and then dump all bins, and do another pass with heavy = 0.
 * If heavy is 1, and we set heavy to 2, it happens before we dump bins and retry, thus if heavy is
 * 1, it acts like its 2 if the sorted list is long. After doing a pass, we try to reclaim the top
 * of the heap. */
static void coalesce_sorted(struct alloc_hdr *hdr, int heavy)
{
	/* all writes happen inside other functions that themselves do transactions */
	uint32_t *list = get_sorted_list_head(hdr);
	if(!*list)
		return;
	if(heavy == 2)
		dump_all_bins(hdr, 1);
	struct chunk *chunk, *next = NULL, *next2 = NULL;
	chunk = follow_chunk_ptr(hdr, *list);
	uint32_t count = 0;
	while(chunk->nxt != *list) {
		next = follow_chunk_ptr(hdr, chunk->nxt);
		next2 = follow_chunk_ptr(hdr, next->nxt);
		if(next == next2 || next == chunk)
			break;
		if(can_coalesce(hdr, chunk, next)) {
			chunk_coalesce(hdr, chunk, next);
			chunk = next2;
		} else {
			chunk = next;
		}
		count++;
	}
	if(*list) {
		chunk = follow_chunk_ptr(hdr, *list);
		chunk = follow_chunk_ptr(hdr, chunk->prv);
		/* removes from list internally */
		try_reclaim_top(hdr, chunk, list, NULL);
	}
	if(count > 32 && heavy == 1) {
		heavy = 2;
	}

	if(heavy == 2) {
		dump_all_bins(hdr, 1);
		coalesce_sorted(hdr, 0);
	}
	if(count < 12 && heavy >= 0) {
		coalesce_sorted(hdr, -1);
	}
}

static int try_new_chunk(struct alloc_hdr *hdr, struct alloc_req *req)
{
	/* this function internally does a transaction */
	return create_chunk(hdr, req);
}

static int try_huge(struct alloc_hdr *hdr, struct alloc_req *req)
{
	/* all writes happen inside chunk_split, which is transactional */
	uint32_t *list = get_huge_list_head(hdr);
	if(*list) {
		struct chunk *chunk = follow_chunk_ptr(hdr, *list);
		do {
			struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
			if((chunk_size(chunk) == req->len)) {
				TX_ALLOC_BEGIN(hdr)
				{
					remove_chunk_from_list(hdr, chunk, list);
					void *p = make_external_ptr(hdr, chunk, req);
					TX_ALLOC_RECORD(hdr, req->owner, EXT_PTR | ADD_COMMIT);
					*req->owner = p;
				}
				TX_ALLOC_END(hdr);
				return 1;
			} else if(chunk_split(hdr, chunk, list, req)) {
				return 1;
			}
			chunk = next;
		} while(*list && offset_from_chunk_ptr(hdr, chunk) != *list);
	}
	return 0;
}

#define COUNT_RELEASED 1
static int list_at_least_n(struct alloc_hdr *hdr, uint32_t *list, uint32_t n, int flags)
{
	if(!*list)
		return n == 0;
	struct chunk *chunk = follow_chunk_ptr(hdr, *list);
	uint32_t c = 0, r = 0;
	do {
		if(chunk->flags & RELEASED)
			r++;
		else
			c++;
		chunk = follow_chunk_ptr(hdr, chunk->nxt);
	} while(*list && offset_from_chunk_ptr(hdr, chunk) != *list);
	if(flags & COUNT_RELEASED)
		c += r;
	return c >= n;
}

static int some_bookkeeping_to_do(struct alloc_hdr *hdr)
{
	/* if any of these lists are long-ish, we have work to do */
	int sorted_bk = list_at_least_n(hdr, get_sorted_list_head(hdr), 8, 0);
	int unsorted_bk = list_at_least_n(hdr, get_sorted_list_head(hdr), 8, 0);
	return sorted_bk || unsorted_bk;
}

static void external_call_preamble(twzobj *obj, struct alloc_hdr *hdr, uint64_t flags)
{
	/* toggle our volatility state if we need to. If we are going un-volatile, we need to make sure
	 * to flush the line. */
	TX_ALLOC_RECOVER(hdr);
	if(flags & TWZ_ALLOC_VOLATILE) {
		hdr->flags |= HDR_F_VOLATILE;
	} else if(hdr->flags & HDR_F_VOLATILE) {
		hdr->flags &= ~HDR_F_VOLATILE;
		_clwb(&hdr->flags);
		_pfence();
	}
	if(hdr->tofree) {
		twz_free(obj, hdr->tofree, &hdr->tofree, flags);
	}
}

static int __twz_alloc(twzobj *obj,
  struct alloc_hdr *hdr,
  size_t len,
  void **owner,
  uint64_t flags,
  void (*ctor)(void *, void *),
  void *data)
{
	mutex_acquire(&hdr->lock);
	external_call_preamble(obj, hdr, flags);

	/* add to len until we have something usable */
	len += ALLOC_CHUNK_HDR_SZ;
	uint32_t alignment = alignment_from_flags(flags);
	if(alignment) {
		alignment = ALIGN(alignment, 16);
		len += alignment;
	}
	len = ALIGN(len, 16);

	struct alloc_req req;
	req.owner = owner;
	req.flags = flags;
	req.len = len;
	req.data = data;
	req.ctor = ctor;
	req.obj = obj;

	int size_class = get_size_class(len);
	/* if we're allocating a huge chunk we can afford the overhead of bookkeeping. */
	if(size_class == -1) {
		/* these do transactions internally */
		dump_all_bins(hdr, 1);
		coalesce_sorted(hdr, 2);
	}
	log("allocating normal chunk for len %ld (sc %d)", len, size_class);
	/* try fast bins. If the chunk is huge, we'll go through the huge list. Not too fast, but hey
	 * huge chunks have costs. Non-huge chunks are in fast bins, which require a single write to
	 * take out of */
	if(size_class != -1) {
		if(try_find_in_bin(hdr, size_class, &req)) {
			log("  found in bin");
			verify_allocator(hdr);
			mutex_release(&hdr->lock);
			return 0;
		}
	} else {
		if(try_huge(hdr, &req)) {
			log("  found in huge");
			verify_allocator(hdr);
			mutex_release(&hdr->lock);
			return 0;
		}
	}

	/* nothing in the fast bins. Try the unsorted list, and while we're here, sort anything in the
	 * unsorted list that didn't get sorted before. */
	if(try_unsorted(hdr, &req)) {
		log("  unsorted");
		verify_allocator(hdr);
		mutex_release(&hdr->lock);
		return 0;
	}

	/* if we're allocating something large, and we need to do some bookkeeping, we'll dump
	 * everything and sort. This shouldn't happen too often */
	if(size_class > NR_SMALL && some_bookkeeping_to_do(hdr)) {
		dump_all_bins(hdr, size_class < NR_SMALL + NR_LARGE ? 0 : 1);
		coalesce_sorted(hdr, 0);
	}

	/* okay, lets try to steal from the sorted list or the huge list, grabbing chunks that are too
	 * big and splitting them. */
	if(try_steal(hdr, &req)) {
		log("  stolen");
		verify_allocator(hdr);
		mutex_release(&hdr->lock);
		return 0;
	}

	/* if all that failed, grab a new chunk off the top of the heap */
	if(try_new_chunk(hdr, &req)) {
		log("  allocating new");
		verify_allocator(hdr);
		mutex_release(&hdr->lock);
		return 0;
	}

	mutex_release(&hdr->lock);
	return -ENOMEM;
}

int twz_alloc(twzobj *obj,
  size_t len,
  void **owner,
  uint64_t flags,
  void (*ctor)(void *, void *),
  void *data)
{
	struct alloc_hdr *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
	mutex_acquire(&hdr->lock);
	int r = __twz_alloc(obj, hdr, len, owner, flags, ctor, data);
	mutex_release(&hdr->lock);
	return r;
}

static void try_release_huge_chunks(struct alloc_hdr *hdr)
{
	uint32_t *list = get_huge_list_head(hdr);
	if(*list) {
		struct chunk *chunk = follow_chunk_ptr(hdr, *list);
		do {
			struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
			if(!(chunk->flags & RELEASED)) {
				log("trying to release huge chunk %x (%d)",
				  offset_from_chunk_ptr(hdr, chunk),
				  chunk->len);
				try_release_pages(hdr,
				  offset_from_chunk_ptr(hdr, chunk) + ALLOC_CHUNK_HDR_SZ,
				  offset_from_chunk_ptr(hdr, chunk) + chunk->len);
				/* doesnt need to be transactional */
				chunk->flags |= RELEASED;
				if(!(hdr->flags & HDR_F_VOLATILE)) {
					_clwb(&chunk->flags);
					_pfence();
				}
			}
			chunk = next;
		} while(*list && offset_from_chunk_ptr(hdr, chunk) != *list);
	}
	list = get_sorted_list_head(hdr);
	if(*list) {
		struct chunk *chunk = follow_chunk_ptr(hdr, *list);
		do {
			struct chunk *next = follow_chunk_ptr(hdr, chunk->nxt);
			if(!(chunk->flags & RELEASED) && is_huge(chunk)) {
				log("trying to release huge chunk %x (%d)",
				  offset_from_chunk_ptr(hdr, chunk),
				  chunk->len);
				try_release_pages(hdr,
				  offset_from_chunk_ptr(hdr, chunk) + ALLOC_CHUNK_HDR_SZ,
				  offset_from_chunk_ptr(hdr, chunk) + chunk->len);
				/* doesnt need to be transactional */
				chunk->flags = RELEASED;
				if(!(hdr->flags & HDR_F_VOLATILE)) {
					_clwb(&chunk->flags);
					_pfence();
				}
			}
			chunk = next;
		} while(*list && offset_from_chunk_ptr(hdr, chunk) != *list);
	}
}

static void __twz_free(twzobj *obj, struct alloc_hdr *hdr, void *p, void **owner, uint64_t flags)
{
	external_call_preamble(obj, hdr, flags);
	if(p == NULL) {
		return;
	}
	p = twz_ptr_local(p);
	p = twz_object_lea(obj, p);
	struct chunk *chunk = (struct chunk *)((char *)p - ALLOC_CHUNK_HDR_SZ);
	assert(chunk->canary == CANARY);
	if(chunk->flags & ALIGNED) {
		/* undo alignment, if it was aligned */
		assert(chunk->off > 0);
		log("undoing ALIGN %d : %d", chunk->off, offset_from_chunk_ptr(hdr, chunk));
		chunk = (struct chunk *)((char *)p - (ALLOC_CHUNK_HDR_SZ + chunk->off));
		log("undone: %d", offset_from_chunk_ptr(hdr, chunk));
		assert(!(chunk->flags & ALIGNED));
		assert(chunk->canary == CANARY);
	}
	int size_class = get_size_class_for_chunk(chunk);
	if(size_class == -1) {
		/* these internally do transactions */
		dump_all_bins(hdr, 1);
		coalesce_sorted(hdr, 2);
	}

	/* first, see if we can just return to the top of the heap */
	if(try_reclaim_top(hdr, chunk, NULL, owner)) {
		verify_allocator(hdr);
		return;
	}

	log("freeing normal chunk in size_class %d (%d)", get_size_class_for_chunk(chunk), chunk->len);
	/* otherwise, lets toss it "somewhere", where this means try to put it in a fast bin, and if we
	 * can't, try sorting. If we give up on sorting, it'll end up in the unsorted list */
	TX_ALLOC_BEGIN(hdr)
	{
		put_chunk_somewhere(hdr, chunk);
		TX_ALLOC_RECORD(hdr, owner, EXT_PTR | ADD_COMMIT);
		*owner = NULL;
	}
	TX_ALLOC_END(hdr);

	verify_allocator(hdr);
	log("COAL");
	/* and, finally, try coalescing. We can spend a bit of time. */
	coalesce_sorted(hdr, 0);
	try_release_huge_chunks(hdr);
	verify_allocator(hdr);
}

void twz_free(twzobj *obj, void *p, void **owner, uint64_t flags)
{
	struct alloc_hdr *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
	mutex_acquire(&hdr->lock);
	__twz_free(obj, hdr, p, owner, flags);
	mutex_release(&hdr->lock);
}

int twz_realloc(twzobj *obj, void *p, void **owner, size_t newlen, uint64_t flags)
{
	struct alloc_hdr *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
	mutex_acquire(&hdr->lock);
	external_call_preamble(obj, hdr, flags);

	p = twz_ptr_local(p);
	void *vsrc = twz_object_lea(obj, p);
	struct chunk *chunk = (struct chunk *)((char *)vsrc - ALLOC_CHUNK_HDR_SZ);

	/* A bit inefficient, but: if we are allocating less than what we already have, then don't
	 * bother doing anything. Note that this still works if the chunk is a phantom chunk because it
	 * also stores the length value. */
	if(chunk->len >= newlen) {
		mutex_release(&hdr->lock);
		return 0;
	}

	/* plan: allocate and store the owned pointer into a temporary, internal location. Then copy the
	 * original data into this temporary location, flush it if necessary, and swap the external
	 * owned pointer over. Later we can free the temporary owned pointer. */
	int r = __twz_alloc(obj, hdr, newlen, &hdr->tofree, flags, TWZ_ALLOC_CTOR_ZERO, NULL);
	if(r) {
		mutex_release(&hdr->lock);
		return r;
	}

	void *vdst = twz_object_lea(obj, hdr->tofree);
	memcpy(vdst, vsrc, chunk->len);
	if(!(flags & TWZ_ALLOC_VOLATILE)) {
		_clwb_len(vdst, chunk->len);
		_pfence();
	}
	TX_ALLOC_BEGIN(hdr)
	{
		TX_ALLOC_RECORD(hdr, owner, EXT_PTR | ADD_COMMIT);
		*owner = hdr->tofree;
	}
	TX_ALLOC_END(hdr);
	/* what happens if there's a failure here? Well, external_call_preamble will have to free a
	 * non-null tofree pointer if there is one as part of recovery. */
	__twz_free(obj, hdr, hdr->tofree, &hdr->tofree, flags);
	mutex_release(&hdr->lock);
	return 0;
}

static void init_alloc(struct alloc_hdr *hdr, size_t len)
{
	if(hdr->magic == MAGIC) {
		verify_allocator(hdr);
		return;
	}

	hdr->len = len;
	for(int i = 0; i < NR_SMALL + NR_LARGE; i++) {
		for(int j = 0; j < MAX_BUCKET; j++) {
			hdr->bins[i][j] = 0;
		}
	}
	hdr->unsorted = 0;
	hdr->huge = 0;
	hdr->sorted = 0;
	hdr->logsz = LOG_SIZE;
	hdr->top = ALIGN(sizeof(*hdr), 16) + hdr->logsz;
	hdr->tmpend = hdr->end = 0;
	mutex_init(&hdr->lock);
	_clwb_len(hdr, sizeof(*hdr));
	_pfence();
	hdr->magic = MAGIC;
	_clwb(&hdr->magic);
	_pfence();
}

int twz_object_init_alloc(twzobj *obj, size_t offset)
{
	/* align offset by 16 */
	offset = (offset + 15) & ~15;
	offset += OBJ_NULLPAGE_SIZE;

	struct alloc_hdr *hdr = twz_object_lea(obj, (void *)offset);
	init_alloc(hdr, OBJ_TOPDATA - offset);

	return twz_object_addext(obj, ALLOC_METAINFO_TAG, (void *)offset);
}

/*
void *twz_object_alloc(twzobj *obj, size_t sz)
{
    struct twzoa_header *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
    if(!hdr) {
        libtwz_panic("tried to allocate data in an object that does not respond to ALLOC API.");
    }

    return oa_hdr_alloc(obj, hdr, sz);
}

void twz_object_free(twzobj *obj, void *p)
{
    struct twzoa_header *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
    if(!hdr) {
        libtwz_panic("tried to free data in an object that does not respond to ALLOC API.");
    }

    return oa_hdr_free(obj, hdr, twz_ptr_local(p));
}
*/
