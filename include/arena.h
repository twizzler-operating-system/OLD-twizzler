#pragma once

/** @file
 * @brief Arena allocation
 *
 * Useful for allocating memory that you know can be freed all at once.
 */

#include <debug.h>
#include <memory.h>
#include <spinlock.h>
#include <system.h>

#include <lib/iter.h>
#include <lib/list.h>

struct arena_node {
	size_t used;
	size_t len;
	struct list entry;
	char data[];
};

struct arena {
	struct list nodes, full;
	_Atomic size_t nc, fc;
	struct spinlock lock;
};

/** Initialize an arena allocator. Must be called before other functions on the arena allocator. */
static inline void arena_create(struct arena *arena)
{
	arena->lock = SPINLOCK_INIT;
	struct arena_node *node =
	  (struct arena_node *)mm_memory_alloc(mm_page_size(0), PM_TYPE_ANY, false);
	node->len = mm_page_size(0);
	node->used = sizeof(struct arena_node);
	list_init(&arena->nodes);
	list_init(&arena->full);
	arena->nc = 1;
	arena->fc = 0;
	list_insert(&arena->nodes, &node->entry);
}

/** Allocate memory from the arena. This memory cannot be freed without destroying the entire arena
 * (that's kinda the point of an arena allocator). The memory is zeroed before returning.
 *
 * @param arena The arena to allocate from
 * @param length Length of the allocation in bytes
 * @return Virtual pointer to the allocated memory. The memory will be aligned on 16. */
static inline void *arena_allocate(struct arena *arena, size_t length)
{
	/* TODO (perf): this could be optimized by saving the last-allocated pointer to start from
	 * instead of starting from the beginning each time. */
	length = (length & ~15) + 16;
	assert((length + sizeof(struct arena_node) < mm_page_size(0)));
	spinlock_acquire_save(&arena->lock);

	size_t count = 0;
	struct list *e, *next;
	for(e = list_iter_start(&arena->nodes); e != list_iter_end(&arena->nodes); e = next) {
		struct arena_node *node = list_entry(e, struct arena_node, entry);
		next = list_iter_next(e);
		if(node->used + length < node->len) {
			void *ret = (void *)((uintptr_t)node + node->used);
			node->used += length;
			spinlock_release_restore(&arena->lock);
			memset(ret, 0, length);
			return ret;
		} else {
			/* TODO: maybe only do this if we're doing a reasonably small allocation? */
			list_remove(e);
			list_insert(&arena->full, e);
			arena->fc++;
			arena->nc--;
		}
		count++;
	}

	spinlock_release_restore(&arena->lock);
	struct arena_node *node = (void *)mm_memory_alloc(mm_page_size(0), PM_TYPE_ANY, false);
	spinlock_acquire_save(&arena->lock);
	arena->nc++;
	list_insert(&arena->nodes, &node->entry);
	node->len = mm_page_size(0);
	node->used = sizeof(struct arena_node);
	void *ret = (void *)((uintptr_t)node + node->used);
	node->used += length;
	spinlock_release_restore(&arena->lock);
	memset(ret, 0, length);
	return ret;

	/*
	struct arena_node *node = arena->start, *prev = NULL;
	while(node && (node->used + length >= node->len)) {
	    prev = node;
	    node = node->next;
	}

	if(!node) {
	    assert(prev != NULL);
	    size_t len = __round_up_pow2(length * 2);
	    if(len < mm_page_size(0))
	        len = mm_page_size(0);
	    node = prev->next = (void *)mm_memory_alloc(len, PM_TYPE_ANY, true);
	    node->len = len;
	    node->used = sizeof(struct arena_node);
	}

	void *ret = (void *)((uintptr_t)node + node->used);
	node->used += length;

	spinlock_release_restore(&arena->lock);
	return ret;
	*/
}

/** Destroy the arena and free all memory that was allocated. */
static inline void arena_destroy(struct arena *arena)
{
	struct list *e, *next;
	for(e = list_iter_start(&arena->nodes); e != list_iter_end(&arena->nodes); e = next) {
		next = list_iter_next(e);
		struct arena_node *node = list_entry(e, struct arena_node, entry);
		mm_memory_dealloc(node);
	}
	for(e = list_iter_start(&arena->full); e != list_iter_end(&arena->full); e = next) {
		next = list_iter_next(e);
		struct arena_node *node = list_entry(e, struct arena_node, entry);
		mm_memory_dealloc(node);
	}
}
