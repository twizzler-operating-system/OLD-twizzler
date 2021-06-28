#pragma once

#define GET_PAGE_BLOCK 1
struct blocklist;
struct page_entry {
	struct page *page;
	struct blocklist *blocks;
};

struct pagevec {
	_Atomic size_t refs;
	struct spinlock lock;
	struct vector pages;
	struct list ranges;
};

int pagevec_get_page(struct pagevec *, size_t, struct page **, int flags);
struct pagevec *object_new_pagevec(struct object *, size_t, size_t *);
size_t pagevec_len(struct pagevec *);
void pagevec_set_page(struct pagevec *pv, size_t idx, struct page *page);
void pagevec_free(struct pagevec *pv);
void pagevec_append_page(struct pagevec *pv, struct page *page);
void pagevec_combine(struct pagevec *a, struct pagevec *b);
struct pagevec *pagevec_new(void);
void pagevec_lock(struct pagevec *);
void pagevec_unlock(struct pagevec *);
