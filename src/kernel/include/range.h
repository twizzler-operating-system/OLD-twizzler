#pragma once

struct range {
	size_t start;
	size_t len;
	size_t pv_offset;
	struct spinlock lock;
	struct pagevec *pv;
	struct object *obj;
	struct rbnode node;
	struct list entry;
};

size_t range_pv_idx(struct range *, size_t);
struct range *range_split(struct range *, size_t);
void range_cut_half(struct range *range, size_t len);
void range_toss(struct range *range);
void range_free(struct range *range);

void range_clone(struct range *);
struct range *object_add_range(struct object *, struct pagevec *, size_t, size_t, size_t);
struct range *object_find_range(struct object *, size_t);
struct range *object_find_next_range(struct object *obj, size_t pagenr);
