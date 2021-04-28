#pragma once

#include <stdint.h>
#ifdef __cplusplus
#include <atomic>
#endif

struct memory_stats {
#ifdef __cplusplus
	std::atomic_uint_least64_t pages_early_used;
	std::atomic_uint_least64_t memalloc_nr_objects;
	std::atomic_uint_least64_t memalloc_total;
	std::atomic_uint_least64_t memalloc_used;
	std::atomic_uint_least64_t memalloc_unfreed;
	std::atomic_uint_least64_t memalloc_free;
	std::atomic_uint_least64_t pmap_used;
	std::atomic_uint_least64_t tmpmap_used;
#else
	_Atomic uint64_t pages_early_used;
	_Atomic uint64_t memalloc_nr_objects;
	_Atomic uint64_t memalloc_total;
	_Atomic uint64_t memalloc_used;
	_Atomic uint64_t memalloc_unfreed;
	_Atomic uint64_t memalloc_free;
	_Atomic uint64_t pmap_used;
	_Atomic uint64_t tmpmap_used;
#endif
};

#define PAGE_STATS_INFO_CRITICAL 1
#define PAGE_STATS_INFO_ZERO 2

struct page_stats {
#ifdef __cplusplus
	std::atomic_uint_least64_t page_size;
	std::atomic_uint_least64_t info;
	std::atomic_uint_least64_t avail;
#else
	_Atomic uint64_t page_size;
	_Atomic uint64_t info;
	_Atomic uint64_t avail;
#endif
};

struct memory_stats_header {
	struct memory_stats stats;
	uint64_t nr_page_groups;
	struct page_stats page_stats[];
};

#include <twz/objid.h>

struct nv_header {
	uint64_t devid;
	uint32_t regid;
	uint32_t flags;
	uint64_t meta_lo;
	uint64_t meta_hi;
};

#define NVD_HDR_MAGIC 0x12345678
struct nvdimm_region_header {
	uint32_t magic;
	uint32_t version;
	uint64_t flags;

	uint64_t total_pages;
	uint64_t used_pages;

	objid_t nameroot;

	uint32_t pg_used_num[];
};
