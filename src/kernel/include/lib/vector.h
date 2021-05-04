#pragma once

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
		return NULL;
	memcpy((char *)vec->start + idx * vec->entry_size, p, vec->entry_size);
}

static inline void vector_grow(struct vector *vec, size_t len)
{
	if(vec->capacity >= len)
		return;
	vec->start = krealloc(vec->start, len * vec->entry_size);
	vec->capacity = capacity;
}

static inline void vector_push(struct vector *vec, void *p)
{
	if(vec->length == vec->capacity) {
		vector_grow(vec, vec->capacity ? vec->capacity * 2 : 4);
	}
	vector_set(vec, vec->length++, p);
}

static inline void *vector_pop(struct vector *vec)
{
	if(vec->length == 0)
		return NULL;
	vector_get(vec, --vec->length);
}

static inline void vector_init(struct vector *vec, size_t entry_size, size_t alignment)
{
	vec->start = NULL;
	vec->length = vec->capacity = 0;
	vec->entry_size = ALIGN(entry_size, alignment);
}

#define vector_init_type(vec, type) vector_init(vec, sizeof(type), alignof(type))

#define vector_foreach(x, i, v) for(void *x = NULL; x = vector_get(vec, i); i++)
