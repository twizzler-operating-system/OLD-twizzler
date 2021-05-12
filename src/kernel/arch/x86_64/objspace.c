#include <arch/x86_64-vmx.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <processor.h>

uintptr_t arch_mm_objspace_max_address(void)
{
	return 2ul << arch_processor_physical_width();
}

size_t arch_mm_objspace_region_size(void)
{
	return 2 * 1024 * 1024;
}

extern struct object_space _bootstrap_object_space;

void arch_objspace_map(struct object_space *space,
  uintptr_t virt,
  struct page *pages,
  size_t count,
  uint64_t mapflags)
{
	if(!space)
		space = &_bootstrap_object_space;
	uint64_t flags = 0;
	flags |= (mapflags & MAP_WRITE) ? EPT_WRITE : 0;
	flags |= (mapflags & MAP_READ) ? EPT_READ : 0;
	flags |= (mapflags & MAP_EXEC) ? EPT_EXEC : 0;
	flags |= EPT_MEMTYPE_WB | EPT_IGNORE_PAT;

	struct arch_object_space *arch = &space->arch;
	printk("TODO: do cache type\n");
	for(size_t i = 0; i < count; i++, virt += mm_page_size(0)) {
		uintptr_t addr;
		if(pages) {
			addr = pages[i].addr;
		} else if(mapflags & MAP_ZERO) {
			addr = mm_page_alloc_addr(PAGE_ZERO);
			/* TODO: release this page struct for reuse? */
		} else if(!(mapflags & MAP_TABLE_PREALLOC)) {
			panic("invalid page mapping strategy");
		}
		if(mapflags & MAP_TABLE_PREALLOC)
			table_premap(&arch->root, virt, 0, EPT_WRITE | EPT_READ | EPT_EXEC, true);
		else
			table_map(&arch->root, virt, addr, 0, flags, EPT_WRITE | EPT_READ | EPT_EXEC, true);
	}
}

uintptr_t arch_mm_objspace_get_phys(uintptr_t oaddr)
{
	struct object_space *space = &_bootstrap_object_space;
	uint64_t entry;
	int level;
	struct arch_object_space *arch = &space->arch;
	if(!table_readmap(&arch->root, oaddr, &entry, &level)) {
		return (uintptr_t)-1;
	}
	return entry & EPT_PAGE_MASK;
}

void arch_mm_objspace_invalidate(uintptr_t start, size_t len, int flags)
{
	x86_64_invvpid(start, len);
}
