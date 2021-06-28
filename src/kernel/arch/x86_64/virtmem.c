#include <arch/x86_64.h>
#include <memory.h>
#include <processor.h>
#include <vmm.h>

void arch_mm_print_ctx(struct vm_context *ctx)
{
	table_print_recur(&ctx->arch.root, 3, 0, 0);
}

#define RECUR_FLAGS (VM_MAP_USER | VM_MAP_WRITE | PAGE_PRESENT)
int arch_mm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uint64_t mapflags)
{
	if(!ctx)
		ctx = &kernel_ctx;
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

void arch_mm_unmap(struct vm_context *ctx, uintptr_t virt, size_t len)
{
	struct arch_vm_context *arch = &ctx->arch;
	size_t i = 0;
	while(i < len) {
		size_t rem = len - i;
		int level = 0;
		if(is_aligned(virt + i, mm_page_size(2)) && rem >= mm_page_size(2)) {
			level = 2;
		} else if(is_aligned(virt + i, mm_page_size(1)) && rem >= mm_page_size(1)) {
			level = 1;
		}

		table_unmap(&arch->root, virt + i, 0);
		i += mm_page_size(level);
	}
}

void arch_mm_virtual_invalidate(struct vm_context *ctx, uintptr_t virt, size_t len)
{
	/* TODO: A do this better */
	asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

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
		memset(&kernel_ctx, 0, sizeof(kernel_ctx));
		arch->root.lock = RWLOCK_INIT;
		arch->root.flags = 0;
		table_realize(&arch->root);

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
	ctx->arch.root.flags = 0;
	ctx->arch.root.lock = RWLOCK_INIT;
	table_realize(&ctx->arch.root);
	for(int i = 256; i < 512; i++) {
		ctx->arch.root.table[i] = kernel_ctx.arch.root.table[i];
		ctx->arch.root.children[i] = kernel_ctx.arch.root.children[i];
		ctx->arch.root.count++;
	}
}
