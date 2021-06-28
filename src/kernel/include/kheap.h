#pragma once
#include <lib/list.h>
struct page;
void kheap_start_dynamic(void);
void *kheap_map_pages(struct page *pages, size_t count);
void *kheap_allocate_pages(size_t len, int flags);
void kheap_free_pages(void *p);

struct kheap_run {
	void *start;
	size_t nr_pages;
	struct list entry;
};
uintptr_t kheap_run_get_phys(struct kheap_run *run);
uintptr_t kheap_run_get_objspace(struct kheap_run *run);

void kheap_free(struct kheap_run *run);
struct kheap_run *kheap_allocate(size_t len);
