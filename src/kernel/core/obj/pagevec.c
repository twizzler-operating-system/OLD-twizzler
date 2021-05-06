#include <object.h>
void pagevec_combine(struct pagevec *a, struct pagevec *b)
{
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

struct pagevec *pagevec_new(void)
{
	panic("");
}

size_t pagevec_len(struct pagevec *pv)
{
	return pv->pages.length;
}

struct pagevec *object_new_pagevec(struct object *obj, size_t idx, size_t *off)
{
	panic("");
}

int pagevec_get_page(struct pagevec *pv, size_t idx, struct page **page, int flags)
{
	return -1;
}
