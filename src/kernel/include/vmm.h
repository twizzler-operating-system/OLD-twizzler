#pragma once

#include <__mm_bits.h>
#include <krc.h>
#include <lib/list.h>
#include <lib/rb.h>
#include <twz/sys/view.h>
struct object;
struct omap;

struct vm_context {
	struct arch_vm_context arch;
	struct object *viewobj;
	struct rbroot root;
	struct spinlock lock;
	struct list entry;
};

void arch_mm_switch_context(struct vm_context *vm);
void arch_vm_context_init(struct vm_context *ctx);
void arch_vm_context_dtor(struct vm_context *ctx);
void arch_mm_context_destroy(struct vm_context *ctx);
void vm_context_destroy(struct vm_context *v);
struct vm_context *vm_context_create(void);
void vm_context_free(struct vm_context *ctx);
void arch_mm_print_ctx(struct vm_context *ctx);
void arch_mm_virtual_invalidate(struct vm_context *ctx, uintptr_t virt, size_t len);

extern struct vm_context kernel_ctx;

struct vmap {
	struct omap *omap;
	struct object *obj;
	size_t slot;
	uint32_t flags;
	struct rbnode node;
};

void kso_view_write(struct object *obj, size_t slot, struct viewentry *v);
bool vm_setview(struct thread *, struct object *viewobj);
struct object *vm_vaddr_lookup_obj(void *addr, uint64_t *off);
void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags);
struct object *vm_context_lookup_object(struct vm_context *ctx, uintptr_t virt);

#define FAULT_EXEC 0x1
#define FAULT_WRITE 0x2
#define FAULT_USER 0x4
#define FAULT_ERROR_PERM 0x10
#define FAULT_ERROR_PRES 0x20
void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags);

void arch_mm_unmap(struct vm_context *ctx, uintptr_t virt, size_t len);
void mm_map(uintptr_t addr, uintptr_t oaddr, size_t len, int flags);
int arch_mm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uint64_t mapflags);
