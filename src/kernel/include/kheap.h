#pragma once
#include <lib/list.h>
struct page;
void kheap_start_dynamic(void);

struct kheap_run {
	void *start;
	size_t nr_pages;
	struct list entry;
};

void kheap_free(struct kheap_run *run);
struct kheap_run *kheap_allocate(size_t len);

uintptr_t kheap_run_get_phys(struct kheap_run *run);
uintptr_t kheap_run_get_objspace(struct kheap_run *run);
