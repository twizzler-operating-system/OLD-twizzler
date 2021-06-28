#pragma once

#define TMPMAP_MAX_PAGES 1024

struct page;
void tmpmap_unmap_page(void *addr);
void *tmpmap_map_page(struct page *page);
struct memory_stats;
void tmpmap_collect_stats(struct memory_stats *stats);
void *tmpmap_map_pages(struct page *pages[], size_t count);
