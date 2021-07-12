#pragma once

#include <kalloc.h>
#include <string.h>

/** @file
 * @brief Vector (growable array) functions.
 *
 * Provides a vector type where we can store an array of items, and grow or shrink that array
 * dynamically. This data structure uses dynamic memory allocation. It is not thread-safe.
 *
 * One common thing with a vector type is pointer invalidation. Essentially, when we perform a 'get'
 * on the vector, we get a pointer into the vector. But later, we can call a function that grows the
 * vector, and that could move the vector, causing any prior pointer we got to no longer be valid.
 */

/** Vector (growable array) type. All fields private. Will store a contiguous array of entry_size
 * entries. */
struct vector {
	void *start;
	size_t entry_size;
	size_t length;
	size_t capacity;
};

/** Get a pointer to a vector element. If idx is greater than the vector's current length, return
 * NULL. Does not invalidate pointers. The returned pointer is valid until an invalidating call to
 * vector functions is performed. Note that there is no thread safety with respect to this pointer
 * invalidation. */
static inline void *vector_get(struct vector *vec, size_t idx)
{
	if(idx >= vec->length)
		return NULL;
	return (void *)((char *)vec->start + idx * vec->entry_size);
}

/** Set a vector element. This will copy in vec->entry_size bytes in from p into the vector at
 * position idx. If idx is greater than vec->length, this function does nothing. Does not invalidate
 * pointers.*/
static inline void vector_set(struct vector *vec, size_t idx, void *p)
{
	if(idx >= vec->length)
		return;
	if(p)
		memcpy((char *)vec->start + idx * vec->entry_size, p, vec->entry_size);
	else
		memset((char *)vec->start + idx * vec->entry_size, 0, vec->entry_size);
}

/** Reserve space in a vector (does not change length, just capacity). Can invalidate pointers. */
static inline void vector_reserve(struct vector *vec, size_t len)
{
	if(vec->capacity >= len)
		return;
	vec->start = krealloc(vec->start, len * vec->entry_size, 0);
	vec->capacity = len;
}

/** Push an item onto a vector (copy in data). Will modify capacity and length to extend the array
 * by 1 to hold the new item. Can invalidate pointers. */
static inline size_t vector_push(struct vector *vec, void *p)
{
	if(vec->length == vec->capacity) {
		vector_reserve(vec, vec->capacity ? vec->capacity * 2 : 4);
	}
	size_t idx = vec->length++;
	vector_set(vec, idx, p);
	return idx;
}

/** Push multiple (nr) items onto the vector. Will copy in data from *item for each one. Can
 * invalidate pointers. */
static inline void vector_grow(struct vector *vec, void *item, size_t nr)
{
	vector_reserve(vec, vec->length + nr);
	for(size_t i = 0; i < nr; i++)
		vector_push(vec, item);
}

/** Set a vector element (see vector_set). Will grow the vector if idx points to an entry outside
 * the vector. Can invalidate pointers. */
static inline size_t vector_set_grow(struct vector *vec, size_t idx, void *item)
{
	if(idx >= vec->length)
		vector_grow(vec, NULL, (idx - vec->length) + 1);
	vector_set(vec, idx, item);
	return idx;
}

/** Remove an item from the end of a vector. This reduces the length of the vector, but not the
 * capacity. This function does not invalidate pointers. */
static inline void *vector_pop(struct vector *vec)
{
	if(vec->length == 0)
		return NULL;
	void *ret = vector_get(vec, vec->length - 1);
	vec->length--;
	return ret;
}

#define vector_foreach(x, i, v) for(size_t i = 0, void *x = NULL; x = vector_get(v, i); i++)

/** Concatenate two vectors together. This invalidates pointers of the first vector, and not the
 * second vector. The two vectors must have the same entry_size.
 * @param vec the first vector, the one that will get added to.
 * @param other the vector to source items from.
 */
static inline void vector_concat(struct vector *vec, struct vector *other)
{
	assert(vec->entry_size == other->entry_size);
	for(size_t i = 0; i < vec->length; i++) {
		void *entry = vector_get(vec, i);
		vector_push(vec, entry);
	}
}

/** Free memory allocated by a vector */
static inline void vector_destroy(struct vector *vec)
{
	if(vec->start) {
		kfree(vec->start);
		vec->start = NULL;
	}
}

/** Initialize a vector. Each item on the vector will be of size >= entry_size, aligned up by
 * alignment. This function does not allocate memory. */
static inline void vector_init(struct vector *vec, size_t entry_size, size_t alignment)
{
	vec->start = NULL;
	vec->length = vec->capacity = 0;
	vec->entry_size = align_up(entry_size, alignment);
}

#define vector_init_type(vec, type) vector_init(vec, sizeof(type), alignof(type))
