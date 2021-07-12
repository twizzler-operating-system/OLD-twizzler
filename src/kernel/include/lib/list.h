#pragma once

/** @file
 * @brief Linked list functions
 *
 * This header provides an intrusive sentinel-based double-linked circular linked list. This is to
 * reduce the number of edge-cases as much as possible, and lists are simple enough that having a
 * separate 'root' type is probably not worth it. It is intrusive because, well, kernel data
 * structures tend to be because we want generic support without additional memory allocation.
 *
 * A list does not provide its own locking functions, and these functions are not thread-safe.
 *
 * As this is an intrusive list, here's a running example we'll use for this file. Say we have a
 * struct foo that we want to put in a list. We'll do that by embedding a list node in that struct:
 * struct foo {
 *     ...
 *     struct list entry;
 *     ...
 * };
 */

#include <assert.h>

/** The core list node struct, to be embedded in a struct that you want in a list */
struct list {
	struct list *next, *prev;
};

/** Declare a new list with name 'name', usually at file-level. */
#define DECLARE_LIST(name) struct list name = { &name, &name }

/** Initialize a list be making this list node 'l' a sentinel */
static inline void list_init(struct list *l)
{
	l->prev = l;
	l->next = l;
}

/** Check if list is empty (sentinel points to itself) */
#define list_empty(l) ((l)->next == (l))

/** Count the length of a list. WARNING -- this is a O(N) function. */
static inline size_t list_len(struct list *list)
{
	struct list *start = list;
	list = list->next;
	size_t c = 0;
	while(list != start) {
		list = list->next;
		c++;
	}
	return c;
}

/** Insert a list node into the list. Using the running example, you'd do something like:
 *    struct foo foo;
 *    list_insert(&list_sentinel, &foo.entry);
 * This will insert struct foo into the list defined by list_sentinel.
 *
 * The list parameter does not _have_ to be a sentinel of a list. If you care about list order, the
 * semantics of this insert is that the list node 'entry' will be inserted directly after the list
 * node 'list', which we'll call the head if 'list' is the sentinel.
 */
static inline void list_insert(struct list *list, struct list *entry)
{
#if CONFIG_DEBUG
	assert(list->next && list->prev);
#endif
	entry->prev = list;
	entry->next = list->next;
	entry->prev->next = entry;
	entry->next->prev = entry;
}

/** Remove a given list node entry from a list. */
static inline void list_remove(struct list *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
#if CONFIG_DEBUG
	if(entry->next != entry)
		entry->next = entry->prev = NULL;
#endif
}

/** Pop a list node from the list. This takes from the head of the list. Thus, the list_insert and
 * list_pop functions together form stack semantics. */
static inline struct list *list_pop(struct list *l)
{
	struct list *next = l->next;
	list_remove(next);
	return next == l ? NULL : next;
}

/** Dequeue a list node from the list. This takes from the tail of the list. Thus, the list_insert
 * and list_dequeue functions together form queue semantics. */
static inline struct list *list_dequeue(struct list *l)
{
	struct list *prev = l->prev;
	list_remove(prev);
	return prev == l ? NULL : prev;
}

/** Get the struct that contains a given list node entry. This is part of the intrusive nature of
 * the list, so using the running example, let's say we pop an entry from the list, and we want to
 * recover the struct that contains it:
 *     struct list *node = list_pop(&list);
 *     struct foo *foo = list_entry(node, struct foo, entry);
 */
#define list_entry(e, type, memb) container_of(e, type, memb)

/** Given a struct that contains a list node, traverse the list forward by 1:
 *     struct foo *foo = ...;
 *     struct foo *next_foo = list_entry_next(foo, entry);
 */
#define list_entry_next(item, memb) list_entry((item)->memb.next, typeof(*(item)), memb)

/** Similar to list_entry_next but traverse backwards */
#define list_entry_prev(item, memb) list_entry((item)->memb.prev, typeof(*(item)), memb)

/** Iteration functions. Say you want to traverse an entire list. You'd do this by:
 *     for(struct list *node = list_iter_start(&list_sentinel); node !=
 *         list_iter_end(&list_sentinel); node = list_iter_next(node)) {
 *             ...
 *     }
 */
#define list_iter_start(list) (list)->next

/** See \ref list_iter_start */
#define list_iter_end(list) list

/** See \ref list_iter_start */
#define list_iter_next(e) (e)->next
/** See \ref list_iter_start */
#define list_iter_prev(e) (e)->prev
