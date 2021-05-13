#include <object.h>
#include <slab.h>

static void __range_init(void *d __unused, void *obj)
{
	struct range *r = obj;
	r->lock = SPINLOCK_INIT;
}

static DECLARE_SLABCACHE(sc_range, sizeof(struct range), __range_init, NULL, NULL, NULL, NULL);

static int __range_compar_key(struct range *a, size_t pg)
{
	if(a->start > pg)
		return 1;
	else if(a->start + a->len <= pg)
		return -1;
	return 0;
}

static int __range_compar(struct range *a, struct range *b)
{
	return __range_compar_key(a, b->start);
}

struct range *object_find_range(struct object *obj, size_t page)
{
	struct rbnode *node = rb_search(&obj->range_tree, page, struct range, node, __range_compar_key);
	if(!node)
		return NULL;
	return rb_entry(node, struct range, node);
}

struct range *object_add_range(struct object *obj,
  struct pagevec *pv,
  size_t start,
  size_t len,
  size_t off)
{
	struct range *r = slabcache_alloc(&sc_range);
	r->pv = pv;
	pv->refs++;
	r->obj = obj;
	r->pv_offset = off;
	r->len = len;
	r->start = start;
	list_insert(&pv->ranges, &r->entry);
	rb_insert(&obj->range_tree, r, struct range, node, __range_compar);
	return r;
}

struct range *range_split(struct range *range, size_t rp)
{
	panic("");
}

void range_clone(struct range *range)
{
}

size_t range_pv_idx(struct range *range, size_t idx)
{
	return (idx - range->start) + range->pv_offset;
}
