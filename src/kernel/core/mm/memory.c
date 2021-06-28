#include <debug.h>
#include <kheap.h>
#include <lib/iter.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <pmap.h>
#include <slots.h>
#include <thread.h>
#include <tmpmap.h>
#include <vmm.h>
static DECLARE_LIST(physical_regions);
static bool mm_ready = false;

static const char *memory_type_strings[] = {
	[MEMORY_AVAILABLE] = "System RAM",
	[MEMORY_RESERVED] = "Reserved",
	[MEMORY_CODE] = "Firmware Code",
	[MEMORY_BAD] = "Bad Memory",
	[MEMORY_RECLAIMABLE] = "Reclaimable System Memory",
	[MEMORY_UNKNOWN] = "Unknown Memory",
	[MEMORY_KERNEL_IMAGE] = "Kernel Image",
};

static const char *memory_subtype_strings[] = {
	[MEMORY_SUBTYPE_NONE] = "",
	[MEMORY_AVAILABLE_VOLATILE] = "(volatile)",
	[MEMORY_AVAILABLE_PERSISTENT] = "(persistent)",
};

void mm_register_region(struct memregion *reg)
{
	if(reg->start == 0 && reg->type == MEMORY_AVAILABLE) {
		reg->start += mm_page_size(0);
		reg->length -= mm_page_size(0);
	}
	printk("[mm] registering memory region %lx -> %lx %s %s\n",
	  reg->start,
	  reg->start + reg->length - 1,
	  memory_type_strings[reg->type],
	  memory_subtype_strings[reg->subtype]);
	list_insert(&physical_regions, &reg->entry);
}

void mm_init_region(struct memregion *reg,
  uintptr_t start,
  size_t len,
  enum memory_type type,
  enum memory_subtype st)
{
	reg->start = start;
	reg->length = len;
	reg->flags = 0;
	reg->type = type;
	reg->subtype = st;
	reg->off = 0;
}

size_t mm_early_count = 0;
size_t mm_late_count = 0;
_Atomic size_t mm_kernel_alloced_total = 0;
_Atomic size_t mm_kernel_alloced_current = 0;
extern size_t mm_page_count;
extern size_t mm_page_alloc_count;
extern size_t mm_page_bootstrap_count;
extern size_t mm_page_alloced;

bool mm_is_ready(void)
{
	return mm_ready;
}

#include <twz/sys/dev/memory.h>

struct memory_stats mm_stats = { 0 };
struct page_stats mm_page_stats[MAX_PGLEVEL + 1];

void mm_print_stats(void)
{
	printk("TODO: A print allocation stats\n");
#if 0
	printk("early allocation: %ld KB\n", mm_early_count / 1024);
	printk("late  allocation: %ld KB\n", mm_late_count / 1024);
	printk("page  allocation: %ld KB\n", mm_page_alloc_count / 1024);
	printk("total allocation: %ld KB\n", mm_kernel_alloced_total);
	printk("cur   allocation: %ld KB\n", mm_kernel_alloced_current);
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
		printk("allocator: avail = %lx; free = %lx\n", alloc->available_memory, alloc->free_memory);
	}
	// mm_print_kalloc_stats();
	mm_page_print_stats();
#endif
}

#include <device.h>
#include <init.h>
#include <object.h>
#include <twz/sys/dev/system.h>
static struct object *mem_object = NULL;
static struct memory_stats_header *msh = NULL;
static void __init_mem_object(void *_a __unused)
{
#if 0
	struct object *so = get_system_object();
	struct object *d = device_register(DEVICE_BT_SYSTEM, 1ul << 24);
	char name[128];
	snprintf(name, 128, "MEMORY STATS");
	kso_setname(d, name);

	struct bus_repr *brepr = bus_get_repr(so);
	kso_attach(so, d, brepr->max_children);
	device_release_headers(so);
	mem_object = d; /* krc: move */
	msh = device_get_devspecific(mem_object);
#endif
}
POST_INIT(__init_mem_object, NULL);

void mm_update_stats(void)
{
	/* TODO A */
#if 0
	if(msh) {
		pmap_collect_stats(&mm_stats);
		tmpmap_collect_stats(&mm_stats);
		mm_stats.pages_early_used = mm_early_count;

		size_t ma_free = 0, ma_total = 0, ma_unfreed = 0, ma_used = 0;
		size_t count = 0;
		spinlock_acquire_save(&allocator_lock);
		foreach(e, list, &allocators) {
			struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);

			ma_total += alloc->length;
			ma_used += alloc->length - (alloc->free_memory + alloc->available_memory);
			ma_unfreed += alloc->available_memory;
			ma_free += alloc->free_memory;
			count++;
		}
		spinlock_release_restore(&allocator_lock);

		mm_stats.memalloc_nr_objects = count;
		mm_stats.memalloc_total = ma_total;
		mm_stats.memalloc_used = ma_used;
		mm_stats.memalloc_unfreed = ma_unfreed;
		mm_stats.memalloc_free = ma_free;

		msh->stats = mm_stats;
	}
#endif
}

void mm_init_phase_2(void)
{
	mm_page_init();
	kheap_start_dynamic();
	kalloc_system_init();
	printk("[mm] memory management bootstrapping completed\n");
	mm_ready = true;
	atomic_thread_fence(memory_order_seq_cst);
}

void *mm_early_ptov(uintptr_t phys)
{
	return (void *)(phys += PHYSICAL_MAP_START);
}

uintptr_t mm_region_alloc_raw(size_t len, size_t align, int flags)
{
	assert(len);
	align = (align == 0) ? 8 : align;
	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);
		spinlock_acquire_save(&reg->lock);
		size_t extra = align_up(reg->start, align) - reg->start;
		bool is_bootstrap = reg->start < MEMORY_BOOTSTRAP_MAX;
		bool has_space = reg->length > (len + extra) && reg->start > 0;
		bool is_allocable =
		  reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_VOLATILE;
		if((is_bootstrap || !(flags & REGION_ALLOC_BOOTSTRAP)) && is_allocable && has_space) {
			reg->start = align_up(reg->start, align);
			uintptr_t alloc = reg->start;
			reg->start += len;
			reg->length -= (len + extra);
			assert(alloc);
			spinlock_release_restore(&reg->lock);
			return alloc;
		}
		spinlock_release_restore(&reg->lock);
	}
	return 0;
}

void mm_early_alloc(uintptr_t *phys, void **virt, size_t len, size_t align)
{
	uintptr_t alloc = mm_region_alloc_raw(len, align, REGION_ALLOC_BOOTSTRAP);
	if(alloc == 0)
		panic("out of early-alloc memory");
	mm_early_count += len;
	if(phys)
		*phys = alloc;
	void *v = mm_early_ptov(alloc);
	if(virt)
		*virt = v;
	memset(v, 0, len);
}

void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags)
{
	if(VADDR_IS_USER(addr) || VADDR_IS_USER(ip)) {
		vm_context_fault(ip, addr, flags);
	} else {
		panic("kernel page fault: %lx, %x at ip=%lx", addr, flags, ip);
	}
}
