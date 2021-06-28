#include <kheap.h>
#include <memory.h>
#include <objspace.h>
#include <page.h>
#include <spinlock.h>
#include <vmm.h>

#define KHEAP_SIZE 1024 * 1024 * 1024ul

static _Atomic uintptr_t kheap_start = KERNEL_VIRTUAL_HEAP_BASE;

#define KHEAP_MAP_FLAGS MAP_WIRE | MAP_KERNEL | MAP_GLOBAL | MAP_READ | MAP_WRITE

/* TODO: ensure that an allocation will always be able to allocate enough memory to make the
 * allocation work (that is, we may have to extend heap space or objspace mapping before we fully
 * run out */

struct kheap_bucket {
	_Atomic size_t count;
	struct list list;
	struct spinlock lock;
};

static struct kheap_bucket *buckets;
static size_t nr_buckets;

static _Atomic uintptr_t kheap_end = 0;
static _Atomic uintptr_t kheap_top = 0;
static uintptr_t kheap_oaddr_start = 0;

static struct kheap_run *kheap_run_page = NULL;
static size_t kheap_run_page_idx = 0;
static struct spinlock kheap_run_page_lock = SPINLOCK_INIT;

uintptr_t kheap_run_get_objspace(struct kheap_run *run)
{
	return ((uintptr_t)run->start - kheap_start) + kheap_oaddr_start;
}

uintptr_t kheap_run_get_phys(struct kheap_run *run)
{
	uintptr_t oaddr = kheap_run_get_objspace(run);
	return mm_objspace_get_phys(NULL, oaddr);
}

static void *kheap_reserve_static(size_t len)
{
	if(kheap_end) {
		panic("tried to reserve static kheap space after dynamic phase started");
	}
	len = align_up(len, mm_page_size(0));
	uintptr_t ret = atomic_fetch_add(&kheap_start, len);
	return (void *)ret;
}

static void kheap_add_to_bucket(size_t nr, struct kheap_run *run)
{
	spinlock_acquire_save(&buckets[nr].lock);
	list_insert(&buckets[nr].list, &run->entry);
	buckets[nr].count++;
	spinlock_release_restore(&buckets[nr].lock);
}

static struct kheap_run *kheap_take_from_bucket(size_t nr, bool quick)
{
	if(buckets[nr].count == 0 && quick)
		return NULL;
	spinlock_acquire_save(&buckets[nr].lock);
	struct kheap_run *run = NULL;
	if(!list_empty(&buckets[nr].list)) {
		buckets[nr].count--;
		run = list_entry(list_pop(&buckets[nr].list), struct kheap_run, entry);
	}
	spinlock_release_restore(&buckets[nr].lock);
	return run;
}

static void *kheap_allocate_from_end(void)
{
	void *p = (void *)atomic_fetch_add(&kheap_top, mm_objspace_region_size());
	uintptr_t oaddr = ((uintptr_t)p - kheap_start) + kheap_oaddr_start;
	mm_objspace_kernel_fill(oaddr,
	  NULL,
	  mm_objspace_region_size() / mm_page_size(0),
	  KHEAP_MAP_FLAGS | MAP_REPLACE | MAP_ZERO);
	return p;
}

void kheap_start_dynamic(void)
{
	kheap_start = align_up(kheap_start, mm_objspace_region_size());
	size_t x = mm_objspace_region_size();
	size_t i = 1;
	while(x > mm_page_size(0)) {
		i++;
		x /= 2;
	}
	printk("[mm] kheap creating %ld buckets\n", i);
	nr_buckets = i;
	mm_early_alloc(NULL, (void **)&buckets, sizeof(buckets[0]) * nr_buckets, 0);
	for(i = 0; i < nr_buckets; i++) {
		list_init(&buckets[i].list);
		buckets[i].lock = SPINLOCK_INIT;
		buckets[i].count = 0;
	}

	kheap_top = kheap_start;
	kheap_end = kheap_top + KHEAP_SIZE;
	kheap_oaddr_start = mm_objspace_kernel_reserve(KHEAP_SIZE * 2);
	kheap_oaddr_start = align_up(kheap_oaddr_start, mm_page_size(2));
	mm_map(kheap_top, kheap_oaddr_start, KHEAP_SIZE, KHEAP_MAP_FLAGS);

	mm_objspace_kernel_fill(kheap_oaddr_start,
	  NULL,
	  KHEAP_SIZE / mm_page_size(0),
	  KHEAP_MAP_FLAGS | MAP_REPLACE | MAP_TABLE_PREALLOC);

	kheap_run_page = kheap_allocate_from_end();
	kheap_run_page_lock = SPINLOCK_INIT;
}

static struct kheap_run *kheap_new_run_struct(void)
{
	spinlock_acquire_save(&kheap_run_page_lock);
	if(kheap_run_page_idx >= (mm_objspace_region_size() / sizeof(struct kheap_run))) {
		kheap_run_page = kheap_allocate_from_end();
		kheap_run_page_idx = 0;
	}
	struct kheap_run *ret = &kheap_run_page[kheap_run_page_idx++];
	spinlock_release_restore(&kheap_run_page_lock);
	return ret;
}

static void kheap_put_run_somewhere(struct kheap_run *run)
{
	size_t i;
	for(i = nr_buckets - 1; i > 0; i--) {
		if(run->nr_pages >= (1 << i))
			break;
	}
	kheap_add_to_bucket(i, run);
}

static void kheap_split_run(struct kheap_run *run, size_t nrpg)
{
	struct kheap_run *newrun = kheap_new_run_struct();
	newrun->nr_pages = run->nr_pages - nrpg;
	newrun->start = (void *)((uintptr_t)run->start + mm_page_size(0) * nrpg);
	run->nr_pages = nrpg;
	kheap_put_run_somewhere(newrun);
}

struct kheap_run *kheap_allocate(size_t len)
{
	if(!kheap_end) {
		panic("tried dynamic kheap allocation while bootstrapping memory management");
	}
	len = align_up(len, mm_page_size(0));
	size_t np = len / mm_page_size(0);
	size_t smallest;
	for(smallest = 0; smallest < nr_buckets && (1 << smallest) < np; smallest++)
		;
	if(smallest == nr_buckets)
		panic("tried to allocate region of length %ld from kheap (maximum of %ld allocation size)",
		  len,
		  mm_objspace_region_size());
	assert((1 << smallest) >= np);

	for(size_t i = smallest; i < nr_buckets; i++) {
		struct kheap_run *run = kheap_take_from_bucket(i, true);
		if(run) {
			if(i > smallest) {
				kheap_split_run(run, np);
			}
			assert(run->nr_pages >= np);
			/* TODO: we can avoid this memset sometimes, probably */
			memset(run->start, 0, run->nr_pages * mm_page_size(0));
			return run;
		}
	}

	struct kheap_run *run = kheap_new_run_struct();
	void *p = kheap_allocate_from_end();
	run->start = p;
	run->nr_pages = mm_objspace_region_size() / mm_page_size(0);
	kheap_split_run(run, np);
	assert(run->nr_pages >= np);
	/* TODO: we can avoid this memset sometimes, probably */
	memset(run->start, 0, run->nr_pages * mm_page_size(0));
	return run;
}

void kheap_free(struct kheap_run *run)
{
	/* TODO: coalescing, splitting, bookkeeping */
	kheap_put_run_somewhere(run);
}

void *kheap_allocate_pages(size_t len, int flags)
{
	panic("A");
}

void kheap_free_pages(void *p)
{
	panic("A");
}
