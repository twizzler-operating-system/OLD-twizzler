#include <arch/x86_64-vmx.h>
#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>

uintptr_t arch_mm_objspace_max_address(void)
{
	return 2ul << arch_processor_physical_width();
}

uintptr_t arch_mm_objspace_kernel_size(void)
{
	return align_up(arch_mm_objspace_max_address() / 2, mm_page_size(2));
}

size_t arch_mm_objspace_region_size(void)
{
	return 2 * 1024 * 1024;
}

extern struct object_space _bootstrap_object_space;

void arch_objspace_region_init(struct objspace_region *region)
{
	/* TODO: need arch_destroy */
	region->arch.table.flags = TABLE_OSPACE;
	region->arch.table.lock = RWLOCK_INIT;
}

void arch_objspace_print_mapping(struct object_space *space, uintptr_t virt)
{
	if(!space)
		space = &_bootstrap_object_space;
	struct table_level *table = &space->arch.root;
	int pml4 = PML4_IDX(virt);
	int pdpt = PDPT_IDX(virt);
	int pd = PD_IDX(virt);
	int pt = PT_IDX(virt);

	printk("=== mapping %lx in space %p (%d %d %d %d)\n", virt, space, pml4, pdpt, pd, pt);

	if(table->table[pml4]) {
		printk("pml4: %lx\n", table->table[pml4]);
	} else {
		return;
	}
	table = table->children[pml4];

	if(table->table[pdpt]) {
		if(table->table[pdpt] & PAGE_LARGE) {
			printk("pdpt: %lx 1G\n", table->table[pdpt]);
		} else {
			printk("pdpt: %lx\n", table->table[pdpt]);
		}
	} else {
		return;
	}
	table = table->children[pdpt];

	if(table->table[pd]) {
		if(table->table[pd] & PAGE_LARGE) {
			printk("  pd: %lx 2M\n", table->table[pd]);
		} else {
			printk("  pd: %lx\n", table->table[pd]);
		}
	} else {
		return;
	}
	table = table->children[pd];

	if(table->table[pt]) {
		printk("  pt: %lx 4K\n", table->table[pt]);
	} else {
		return;
	}
}

void arch_objspace_map(struct object_space *space,
  uintptr_t virt,
  struct page *pages[],
  size_t count,
  uint64_t mapflags)
{
	if(!space)
		space = &_bootstrap_object_space;

	uint64_t flags = EPT_IGNORE_PAT;
	flags |= (mapflags & MAP_WRITE) ? EPT_WRITE : 0;
	flags |= (mapflags & MAP_READ) ? EPT_READ : 0;
	flags |= (mapflags & MAP_EXEC) ? EPT_EXEC : 0;

	struct arch_object_space *arch = &space->arch;
	for(size_t i = 0; i < count; i++, virt += mm_page_size(0)) {
		uintptr_t addr;
		uint64_t cf = EPT_MEMTYPE_WB;
		if(pages) {
			addr = mm_page_addr(pages[i]);
			switch(PAGE_CACHE_TYPE(pages[i])) {
				case PAGE_CACHE_WB:
				default:
					break;
				case PAGE_CACHE_UC:
					cf = EPT_MEMTYPE_UC;
					break;
				case PAGE_CACHE_WT:
					cf = EPT_MEMTYPE_WT;
					break;
				case PAGE_CACHE_WC:
					cf = EPT_MEMTYPE_WC;
					break;
			}
		} else if(mapflags & MAP_ZERO) {
			addr = mm_page_alloc_addr(PAGE_ZERO);
		} else if(!(mapflags & MAP_TABLE_PREALLOC)) {
			panic("invalid page mapping strategy");
		}
		if(mapflags & MAP_TABLE_PREALLOC) {
			assert(pages == NULL);
			table_premap(&arch->root, virt, 0, EPT_WRITE | EPT_READ | EPT_EXEC, true);
		} else {
			table_map(
			  &arch->root, virt, addr, 0, flags | cf, EPT_WRITE | EPT_READ | EPT_EXEC, true);
		}
	}
}

void arch_objspace_unmap(struct object_space *space, uintptr_t addr, size_t nrpages, int flags)
{
	if(!space)
		space = &_bootstrap_object_space;
	struct arch_object_space *arch = &space->arch;
	for(size_t i = 0; i < nrpages; i++) {
		table_unmap(&arch->root, addr + i * mm_page_size(0), flags);
	}
}

bool arch_objspace_region_map_page(struct objspace_region *region,
  size_t idx,
  struct page *page,
  uint64_t flags)
{
	assert(idx < 512);
	/* TODO: do we want to ignore PAT? */
	uint64_t mapflags = EPT_IGNORE_PAT;
	mapflags |= (flags & MAP_READ) ? EPT_READ : 0;
	mapflags |= (flags & MAP_WRITE) ? EPT_WRITE : 0;
	mapflags |= (flags & MAP_EXEC) ? EPT_EXEC | (1 << 10) : 0;

	switch(PAGE_CACHE_TYPE(page)) {
		case PAGE_CACHE_WB:
			mapflags |= EPT_MEMTYPE_WB;
			break;
		case PAGE_CACHE_UC:
			mapflags |= EPT_MEMTYPE_UC;
			break;
		case PAGE_CACHE_WT:
			mapflags |= EPT_MEMTYPE_WT;
			break;
		case PAGE_CACHE_WC:
			mapflags |= EPT_MEMTYPE_WC;
			break;
	}

	if(flags & PAGE_MAP_COW)
		mapflags &= ~EPT_WRITE;

	struct rwlock_result res = rwlock_wlock(&region->arch.table.lock, 0);
	table_realize(&region->arch.table);
	bool ret = true;
	if(region->arch.table.table[idx] == 0) {
		region->arch.table.count++;
		ret = false;
	}
	region->arch.table.table[idx] = mapflags | mm_page_addr(page);
	rwlock_wunlock(&res);
	return ret;
}

void arch_objspace_region_cow(struct objspace_region *region, size_t start, size_t len)
{
	struct rwlock_result res = rwlock_wlock(&region->arch.table.lock, 0);
	if(region->arch.table.table == NULL) {
		rwlock_wunlock(&res);
		return;
	}

	for(size_t i = 0; i < len; i++) {
		region->arch.table.table[i + start] &= ~EPT_WRITE;
	}

	rwlock_wunlock(&res);
}

void arch_objspace_region_unmap(struct objspace_region *region, size_t start, size_t len)
{
	struct rwlock_result res = rwlock_wlock(&region->arch.table.lock, 0);
	if(region->arch.table.table == NULL) {
		rwlock_wunlock(&res);
		return;
	}

	for(size_t i = 0; i < len; i++) {
		if(region->arch.table.table[i + start]) {
			region->arch.table.count--;
		}
		region->arch.table.table[i + start] = 0;
		assert(region->arch.table.children[i + start] == NULL);
	}

	rwlock_wunlock(&res);
}

void arch_objspace_region_map(struct object_space *space,
  struct objspace_region *region,
  uint64_t flags)
{
	assert(space);
	assert(region->addr >= arch_mm_objspace_kernel_size());

	struct table_level *table = &space->arch.root;
	table_realize(table);

	int pml4_idx = PML4_IDX(region->addr);
	int pdpt_idx = PDPT_IDX(region->addr);
	int pd_idx = PD_IDX(region->addr);

	assert(PT_IDX(region->addr) == 0);

	uint64_t mapflags = 0;
	mapflags |= (flags & MAP_READ) ? EPT_READ : 0;
	mapflags |= (flags & MAP_WRITE) ? EPT_WRITE : 0;
	mapflags |= (flags & MAP_EXEC) ? EPT_EXEC | (1 << 10) : 0;

	struct rwlock_result res = rwlock_wlock(&table->lock, RWLOCK_RECURSE);
	table = table_get_next_level(
	  table, pml4_idx, EPT_READ | EPT_WRITE | EPT_EXEC | (1 << 10), true, NULL);
	table = table_get_next_level(
	  table, pdpt_idx, EPT_READ | EPT_WRITE | EPT_EXEC | (1 << 10), true, NULL);
	table_realize(&region->arch.table);
	if(!table->table[pd_idx])
		table->count++;
	table->table[pd_idx] = region->arch.table.phys | mapflags;
	table->children[pd_idx] = &region->arch.table;
	rwlock_wunlock(&res);
}

uintptr_t arch_mm_objspace_get_phys(struct object_space *space, uintptr_t oaddr)
{
	if(space == NULL)
		space = &_bootstrap_object_space;
	uint64_t entry;
	int level;
	struct arch_object_space *arch = &space->arch;
	if(!table_readmap(&arch->root, oaddr, &entry, &level)) {
		return (uintptr_t)-1;
	}
	return entry & EPT_PAGE_MASK;
}

void arch_mm_objspace_invalidate(struct object_space *space, uintptr_t start, size_t len, int flags)
{
	/* TODO: take space into account */
	if(space == NULL)
		space = &_bootstrap_object_space;
	x86_64_invvpid(start, len);
	processor_send_ipi(
	  PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_SHOOTDOWN, NULL, PROCESSOR_IPI_NOWAIT);
}

void arch_object_space_init_bootstrap(struct object_space *space)
{
	space->arch.root.flags = TABLE_OSPACE;
	space->arch.root.lock = RWLOCK_INIT;
	table_realize(&space->arch.root);

	int pml4_max = PML4_IDX(arch_mm_objspace_kernel_size() - 1) + 1;
	int pdpt_max = PDPT_IDX(arch_mm_objspace_kernel_size() - 1) + 1;

	/* TODO: this is not verified for pdpt_max < 512 */
	for(int pml4 = 0; pml4 < pml4_max; pml4++) {
		space->arch.root.children[pml4] = table_level_new(true);
		table_realize(space->arch.root.children[pml4]);
		space->arch.root.table[pml4] =
		  space->arch.root.children[pml4]->phys | EPT_WRITE | EPT_READ | EPT_EXEC;
		if((pml4 + 1 == pml4_max) && pdpt_max != 512) {
			for(int pdpt = 0; pdpt < pdpt_max; pdpt++) {
				space->arch.root.children[pml4]->count++;
				space->arch.root.children[pml4]->children[pdpt] = table_level_new(true);
				table_realize(space->arch.root.children[pml4]->children[pdpt]);
				space->arch.root.children[pml4]->table[pdpt] =
				  space->arch.root.children[pml4]->children[pdpt]->phys | EPT_READ | EPT_WRITE
				  | EPT_EXEC;
			}
		}
	}
}

void arch_object_space_init(struct object_space *space)
{
	space->arch.root.flags = TABLE_OSPACE;
	space->arch.root.lock = RWLOCK_INIT;
	table_realize(&space->arch.root);
	int pml4_max = PML4_IDX(arch_mm_objspace_kernel_size() - 1) + 1;
	int pdpt_max = PDPT_IDX(arch_mm_objspace_kernel_size() - 1) + 1;
	if(pdpt_max != 512) {
		panic("NI -- sub-pml4 kernel region in object space");
	}
	/* TODO: this is not verified for pdpt_max < 512 */
	for(int pml4 = 0; pml4 < pml4_max; pml4++) {
		if((pml4 + 1 == pml4_max) && pdpt_max != 512) {
			struct table_level *table = _bootstrap_object_space.arch.root.children[pml4];
			space->arch.root.children[pml4] = table_level_new(true);
			table_realize(space->arch.root.children[pml4]);
			space->arch.root.table[pml4] =
			  space->arch.root.children[pml4]->phys | EPT_WRITE | EPT_READ | EPT_EXEC;
			for(int pdpt = 0; pdpt < pdpt_max; pdpt++) {
				space->arch.root.children[pml4]->count++;
				space->arch.root.children[pml4]->children[pdpt] = table->children[pdpt];
				space->arch.root.children[pml4]->table[pdpt] = table->table[pdpt];
			}
		} else {
			space->arch.root.children[pml4] = _bootstrap_object_space.arch.root.children[pml4];
			space->arch.root.table[pml4] = _bootstrap_object_space.arch.root.table[pml4];
		}
	}
}

void arch_object_space_fini(struct object_space *space)
{
	table_level_destroy(&space->arch.root);
}
