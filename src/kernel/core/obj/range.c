#include <object.h>
#include <page.h>
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

struct range *object_find_next_range(struct object *obj, size_t pagenr)
{
	struct rbnode *_node = obj->range_tree.node;
	struct rbnode *_res = NULL;
	struct range *best = NULL;
	while(_node) {
		struct range *range = rb_entry(_node, struct range, node);

		if(range->start > pagenr) {
			_node = _node->right;
		} else if(range->start <= pagenr && pagenr < range->start + range->len) {
			return range;
		} else {
			if(!best || range->start < best->start)
				best = range;
			_node = _node->left;
		}
	}
	return best;
}

struct range *object_add_range(struct object *obj,
  struct pagevec *pv,
  size_t start,
  size_t len,
  size_t off)
{
	struct range *r = slabcache_alloc(&sc_range, NULL);
	r->pv = pv;
	r->obj = obj;
	r->pv_offset = off;
	r->len = len;
	r->start = start;
	if(pv) {
		pv->refs++;
		list_insert(&pv->ranges, &r->entry);
	}
	rb_insert(&obj->range_tree, r, struct range, node, __range_compar);
	return r;
}

void range_cut_half(struct range *range, size_t len)
{
	panic("A");
}

void range_toss(struct range *range)
{
	list_remove(&range->entry);
	printk("TODO: clean up pv\n");
	range->pv->refs--;
}

struct range *range_split(struct range *range, size_t rp)
{
	printk("A split range %ld %ld %ld %ld\n", range->start, range->len, range->pv_offset, rp);
	assert(rp < range->len);

	if(rp != range->len - 1) {
		/* new range for the last part */
		object_add_range(range->obj,
		  range->pv,
		  range->start + rp + 1,
		  range->len - (rp + 1),
		  range->pv_offset + rp + 1);
	}

	if(rp > 0) {
		struct range *newrange =
		  object_add_range(range->obj, range->pv, range->start + rp, 1, range->pv_offset + rp);
		range->len = rp;
		return newrange;
	} else {
		range->len = 1;
		return range;
	}
}

void range_clone(struct range *range)
{
	struct pagevec *pv = pagevec_new();
	for(size_t i = 0; i < range->len; i++) {
		struct page *page;
		int r = pagevec_get_page(range->pv, i + range->pv_offset, &page, 0);
		assert(r == 0 && page); // TODO
		                        /* TODO: we could map all the pages at once, too */
		page = mm_page_clone(page);
		pagevec_set_page(pv, i, page);
	}
	range_toss(range);
	range->pv_offset = 0;
	pv->refs = 1;
	range->pv = pv;
	list_insert(&pv->ranges, &range->entry);
}

size_t range_pv_idx(struct range *range, size_t idx)
{
	return (idx - range->start) + range->pv_offset;
}
