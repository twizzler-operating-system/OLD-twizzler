#include <arch/x86_64.h>
#include <memory.h>
#include <processor.h>
extern struct vm_context kernel_ctx;

#define RECUR_FLAGS (VM_MAP_USER | VM_MAP_WRITE | PAGE_PRESENT)
int arch_mm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uint64_t mapflags)
{
	if(!ctx)
		ctx = &kernel_ctx;
	printk("TODO: make this the primary interface and make it map at different levels\n");
	uint64_t flags = PAGE_PRESENT;
	flags |= (mapflags & MAP_GLOBAL) ? VM_MAP_GLOBAL : 0;
	flags |= (mapflags & MAP_WRITE) ? VM_MAP_WRITE : 0;
	flags |= (mapflags & MAP_EXEC) ? 0 : VM_MAP_EXEC;
	flags |= (mapflags & MAP_KERNEL) ? 0 : VM_MAP_USER;

	struct arch_vm_context *arch = &ctx->arch;
	size_t i = 0;
	while(i < len) {
		size_t rem = len - i;
		int level = 0;
		if(is_aligned(virt + i, mm_page_size(2)) && is_aligned(phys + i, mm_page_size(2))
		   && rem >= mm_page_size(2)) {
			level = 2;
		} else if(is_aligned(virt + i, mm_page_size(1)) && is_aligned(phys + i, mm_page_size(1))
		          && rem >= mm_page_size(1)) {
			level = 1;
		}
		table_map(&arch->root, virt + i, phys + i, level, flags, RECUR_FLAGS, false);
		i += mm_page_size(level);
	}
	return 0;
}

#if 0
#define mm_vtoo(p) get_phys((uintptr_t)p)

static bool __do_vm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  int level,
  uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;
	assert(level == 2); /* TODO: remove level */

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	if(!ctx->arch.pml4[pml4_idx]) {
		table[is_kernel ? pml4_idx / 2 : pml4_idx] = kheap_allocate_pages(0x1000, 0);
		/* TODO: right flags? */
		ctx->arch.pml4[pml4_idx] = mm_vtoo(table[is_kernel ? pml4_idx / 2 : pml4_idx])
		                           | PAGE_PRESENT | VM_MAP_WRITE | VM_MAP_USER;
	}
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	if(pdpt[pdpt_idx]) {
		return false;
	}
	pdpt[pdpt_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
	return true;
}

bool arch_vm_getmap(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t *phys,
  int *level,
  uint64_t *flags)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	uintptr_t p = 0, f = 0;
	int l = 0;
	if(ctx->arch.pml4[pml4_idx] == 0) {
		return false;
	}

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	assert(pdpt != NULL);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		p = pdpt[pdpt_idx] & VM_PHYS_MASK;
		f = pdpt[pdpt_idx] & ~VM_PHYS_MASK;
		l = 2;
	}

	f &= ~PAGE_LARGE;
	f ^= VM_MAP_EXEC;
	if(phys)
		*phys = p;
	if(flags)
		*flags = f;
	if(level)
		*level = l;

	return true;
}

bool arch_vm_unmap(struct vm_context *ctx, uintptr_t virt)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	if(ctx->arch.pml4[pml4_idx] == 0) {
		return false;
	}

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	assert(pdpt != NULL);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		pdpt[pdpt_idx] = 0;
	}

	/* TODO: shootdowns... but right now, each thread has its own page tables. So maybe not. */
	asm volatile("invlpg (%0)" ::"r"(virt));

	return true;
}

bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	return __do_vm_map(ctx, virt, phys, level, flags);
}

#include <object.h>
#include <slots.h>
#define MB (1024ul * 1024ul)
/* So, these should probably not be arch-specific. Also, we should keep track of
 * slots, maybe? Refcounts? */
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct slot *slot)
{
	panic("A");
#if 0
	uintptr_t vaddr = (uintptr_t)SLOT_TO_VADDR(map->slot);
	uintptr_t oaddr = SLOT_TO_OADDR(slot->num);
	// bool is_kernel = VADDR_IS_KERNEL(vaddr);

	/* TODO: map global for some things? */
	/* TODO: map protections, what happens if fails */
	if(arch_vm_map(ctx, vaddr, oaddr, MAX_PGLEVEL, VM_MAP_USER | VM_MAP_EXEC | VM_MAP_WRITE)
	   == false) {
		// panic("map fail");
	}
#endif
}

void arch_vm_unmap_object(struct vm_context *ctx, struct vmap *map)
{
	panic("A");
#if 0
	uintptr_t vaddr = (uintptr_t)SLOT_TO_VADDR(map->slot);

	if(arch_vm_unmap(ctx, vaddr) == false) {
#if CONFIG_DEBUG
		panic("failed to unmap object: was already unmapped");
#endif
	}
#endif
}
#endif

#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x)-PHYS_ADDR_DELTA)
void arch_mm_switch_context(struct vm_context *ctx)
{
	bool inv = false;
	if(ctx == NULL) {
		ctx = &kernel_ctx;
		inv = true;
	}
	uint64_t op = ctx->arch.root.phys;
	// op |= ctx->arch.id;
	// if(!inv)
	//	op |= (1ul << 63);
	// printk("SWITCH %lx\n", op);
	asm volatile("mov %0, %%cr3" ::"r"(op) : "memory");
}

void x86_64_vm_kernel_context_init(void)
{
	static bool _init = false;
	if(!_init) {
		_init = true;

		struct arch_vm_context *arch = &kernel_ctx.arch;
		arch->root.lock = RWLOCK_INIT;
		memset(&kernel_ctx, 0, sizeof(kernel_ctx));
		table_realize(&arch->root, false);

		int pml4_slot = PML4_IDX(KERNEL_VIRTUAL_BASE);
		int pdpt_slot = PDPT_IDX(KERNEL_VIRTUAL_BASE);

		struct table_level *table =
		  table_get_next_level(&arch->root, pml4_slot, RECUR_FLAGS, false, NULL);
		table->table[pdpt_slot] = VM_MAP_WRITE | VM_MAP_GLOBAL | PAGE_LARGE | PAGE_PRESENT;

		pml4_slot = PML4_IDX(PHYSICAL_MAP_START);
		pdpt_slot = PDPT_IDX(PHYSICAL_MAP_START);

		table = table_get_next_level(&arch->root, pml4_slot, RECUR_FLAGS, false, NULL);
		table->table[pdpt_slot] = VM_MAP_WRITE | VM_MAP_GLOBAL | PAGE_LARGE | PAGE_PRESENT;
	}

	asm volatile("mov %0, %%cr3" ::"r"(kernel_ctx.arch.root.phys) : "memory");
}

void arch_mm_context_destroy(struct vm_context *ctx)
{
	panic("A");
#if 0
	for(int i = 0; i < 256; i++) {
		if(ctx->arch.user_pdpts[i]) {
			kheap_free_pages(ctx->arch.user_pdpts[i]);
			ctx->arch.user_pdpts[i] = NULL;
			ctx->arch.pml4[i] = 0;
		}
	}
#endif
}

static _Atomic int context_id = 0;

void arch_vm_context_dtor(struct vm_context *ctx)
{
	for(int i = 0; i < 256; i++) {
		if(ctx->arch.root.children[i]) {
			table_free_downward(ctx->arch.root.children[i]);
			ctx->arch.root.children[i] = 0;
			ctx->arch.root.table[i] = 0;
		}
	}
}

void arch_vm_context_init(struct vm_context *ctx)
{
	/* copy table references over for kernel memory space */
	table_realize(&ctx->arch.root, false);
	for(int i = 256; i < 512; i++) {
		ctx->arch.root.table[i] = kernel_ctx.arch.root.table[i];
		ctx->arch.root.children[i] = kernel_ctx.arch.root.children[i];
		ctx->arch.root.count++;
	}
#if 0
	ctx->arch.pml4 = kheap_allocate_pages(0x1000, 0);
	ctx->arch.pml4_phys = mm_vtoo(ctx->arch.pml4);
	for(int i = 0; i < 256; i++) {
		ctx->arch.pml4[i] = 0;
	}
	for(int i = 256; i < 512; i++) {
		ctx->arch.pml4[i] = ((uint64_t *)kernel_ctx.arch.pml4)[i];
	}

	ctx->arch.kernel_pdpts = kernel_virts_pdpt;
	ctx->arch.user_pdpts = kheap_allocate_pages(256 * sizeof(void *), 0);
	ctx->arch.id = ++context_id;
#endif
}
