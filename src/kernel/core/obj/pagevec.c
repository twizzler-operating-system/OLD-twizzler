#include <object.h>
#include <page.h>
#include <slab.h>

static void __pagevec_init(void *d __unused, void *obj)
{
	struct pagevec *pv = obj;
	pv->lock = SPINLOCK_INIT;
	list_init(&pv->ranges);
}

static void __pagevec_ctor(void *d __unused, void *obj)
{
	struct pagevec *pv = obj;
	vector_init(&pv->pages, sizeof(struct page_entry), _Alignof(struct page_entry));
	pv->refs = 0;
}

static void __pagevec_dtor(void *d __unused, void *obj)
{
	struct pagevec *pv = obj;
	assert(list_empty(&pv->ranges));
	vector_destroy(&pv->pages);
}

static DECLARE_SLABCACHE(sc_pagevec,
  sizeof(struct pagevec),
  __pagevec_init,
  __pagevec_ctor,
  __pagevec_dtor,
  NULL,
  NULL);

void pagevec_combine(struct pagevec *a, struct pagevec *b)
{
	panic("A");
	// TODO: make sure we free b or something
	assert(a->refs <= 1 && b->refs <= 1);
	vector_concat(&a->pages, &b->pages);
}

void pagevec_append_page(struct pagevec *pv, struct page *page)
{
	struct page_entry entry = {
		.page = page,
	};
	vector_push(&pv->pages, &entry);
}

size_t pagevec_len(struct pagevec *pv)
{
	return pv->pages.length;
}

struct pagevec *object_new_pagevec(struct object *obj, size_t idx, size_t *off)
{
	struct pagevec *pv = slabcache_alloc(&sc_pagevec, NULL);
	pagevec_append_page(pv, NULL);
	/* TODO: merge with other ones? */
	*off = 0;
	return pv;
}

struct pagevec *pagevec_new(void)
{
	return slabcache_alloc(&sc_pagevec, NULL);
}

void pagevec_free(struct pagevec *pv)
{
	assert(pv->refs == 0);
	assert(list_empty(&pv->ranges));
	void *r;
	while((r = vector_pop(&pv->pages))) {
		struct page_entry *entry = r;
		if(entry->page) {
			mm_page_free(entry->page);
		}
	}
	slabcache_free(&sc_pagevec, pv, NULL);
}

void pagevec_set_page(struct pagevec *pv, size_t idx, struct page *page)
{
	struct page_entry ne = {
		.page = page,
	};
	vector_set_grow(&pv->pages, idx, &ne);
}

int pagevec_get_page(struct pagevec *pv, size_t idx, struct page **page, int flags)
{
	struct page_entry *entry = vector_get(&pv->pages, idx);
	if(!entry) {
		struct page_entry newentry = {
			.page = mm_page_alloc(PAGE_ZERO),
		};
		vector_set_grow(&pv->pages, idx, &newentry);
		*page = newentry.page;
		return 0;
	} else if(!entry->page) {
		entry->page = mm_page_alloc(PAGE_ZERO);
		*page = entry->page;
		return 0;
	}
	*page = entry->page;
	return 0;
}
