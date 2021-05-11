#pragma once

#include <kalloc.h>
#include <string.h>

struct vector {
	void *start;
	size_t entry_size;
	size_t length;
	size_t capacity;
};

static inline void *vector_get(struct vector *vec, size_t idx)
{
	if(idx >= vec->length)
		return NULL;
	return (void *)((char *)vec->start + idx * vec->entry_size);
}

static inline void vector_set(struct vector *vec, size_t idx, void *p)
{
	if(idx >= vec->length)
		return;
	memcpy((char *)vec->start + idx * vec->entry_size, p, vec->entry_size);
}

static inline void vector_reserve(struct vector *vec, size_t len)
{
	if(vec->capacity >= len)
		return;
	vec->start = krealloc(vec->start, len * vec->entry_size, 0);
	vec->capacity = len;
}

static inline void vector_push(struct vector *vec, void *p)
{
	if(vec->length == vec->capacity) {
		vector_reserve(vec, vec->capacity ? vec->capacity * 2 : 4);
	}
	vector_set(vec, vec->length++, p);
}

static inline void vector_grow(struct vector *vec, void *item, size_t nr)
{
	vector_reserve(vec, vec->length + nr);
	for(size_t i = 0; i < nr; i++)
		vector_push(vec, item);
}

static inline void *vector_pop(struct vector *vec)
{
	if(vec->length == 0)
		return NULL;
	return vector_get(vec, --vec->length);
}

#define vector_foreach(x, i, v) for(size_t i = 0, void *x = NULL; x = vector_get(v, i); i++)

static inline void vector_concat(struct vector *vec, struct vector *other)
{
	assert(vec->entry_size == other->entry_size);
	for(size_t i = 0; i < vec->length; i++) {
		void *entry = vector_get(vec, i);
		vector_push(vec, entry);
	}
}

static inline void vector_destroy(struct vector *vec)
{
	if(vec->start) {
		kfree(vec->start);
		vec->start = NULL;
	}
}

static inline void vector_init(struct vector *vec, size_t entry_size, size_t alignment)
{
	vec->start = NULL;
	vec->length = vec->capacity = 0;
	vec->entry_size = align_up(entry_size, alignment);
}

#define vector_init_type(vec, type) vector_init(vec, sizeof(type), alignof(type))
