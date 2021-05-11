#include <memory.h>

struct vm_context kernel_ctx;

void mm_map(uintptr_t addr, uintptr_t oaddr, size_t len, int flags)
{
	printk("::::: %lx %lx %lx %x\n", addr, oaddr, len, flags);
	if(flags & MAP_WIRE) {
		assert(addr >= KERNEL_REGION_START);
	}
	assert(addr >= KERNEL_REGION_START);
	arch_mm_map(NULL, addr, oaddr, len, flags);
}

int mm_map_object_vm(struct vm_context *vm, struct object *obj, size_t page)
{
	struct omap *omap = mm_objspace_get_object_map(obj, page);
	if(!omap)
		return -1;
	panic("A");
	return 0;
}

void vm_context_put()
{
	panic("A");
}

void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags)
{
	panic("A: %lx %lx %x", ip, addr, flags);
}

struct object *vm_vaddr_lookup_obj(void *a, uint64_t *off)
{
	panic("A");
}

bool vm_vaddr_lookup(void *a, objid_t *id, uint64_t *off)
{
	panic("A");
}

bool vm_setview(struct thread *t, struct object *v)
{
	panic("A");
}
