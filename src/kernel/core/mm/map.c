#include <lib/iter.h>
#include <lib/list.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <rwlock.h>
#include <slab.h>
#include <thread.h>

struct vm_context kernel_ctx;

static void vm_context_init(void *data __unused, void *ptr)
{
	struct vm_context *ctx = ptr;
	arch_vm_context_init(ctx);
	ctx->root = RBINIT;
	ctx->lock = SPINLOCK_INIT;
}

static void vm_context_ctor(void *data __unused, void *ptr)
{
	struct vm_context *ctx = ptr;
}

static void vm_context_dtor(void *data __unused, void *ptr)
{
	struct vm_context *ctx = ptr;
	arch_vm_context_dtor(ctx);
}

static void vm_context_fini(void *data __unused, void *ptr)
{
	struct vm_context *ctx = ptr;
}

static DECLARE_SLABCACHE(sc_vm_context,
  sizeof(struct vm_context),
  vm_context_init,
  vm_context_ctor,
  vm_context_dtor,
  vm_context_fini,
  NULL);

static DECLARE_SLABCACHE(sc_vmap, sizeof(struct vmap), NULL, NULL, NULL, NULL, NULL);

void mm_map(uintptr_t addr, uintptr_t oaddr, size_t len, int flags)
{
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

struct vm_context *vm_context_create(void)
{
	return slabcache_alloc(&sc_vm_context);
}

void vm_context_free(struct vm_context *ctx)
{
	printk("TODO: release context\n");
	// slabcache_free(&sc_vm_context, ctx);
}

static struct vmap *vmap_create(uintptr_t addr, struct omap *omap, uint32_t veflags)
{
	struct vmap *vmap = slabcache_alloc(&sc_vmap);
	vmap->omap = omap;
	vmap->slot = addr / mm_objspace_region_size();
	vmap->flags = veflags;
	return vmap;
}

static int vmap_compar_key(struct vmap *v, size_t slot)
{
	if(v->slot > slot)
		return 1;
	if(v->slot < slot)
		return -1;
	return 0;
}

static int vmap_compar(struct vmap *a, struct vmap *b)
{
	return vmap_compar_key(a, b->slot);
}

static inline bool flag_mismatch(int flags, uint32_t veflags)
{
	bool ok = true;
	if((flags & FAULT_EXEC) && !(veflags & VE_EXEC))
		ok = false;
	if((flags & FAULT_WRITE) && !(veflags & VE_WRITE))
		ok = false;
	if(!(flags & FAULT_WRITE) && !(flags & FAULT_EXEC) && !(veflags & VE_READ))
		ok = false;
	return !ok;
}

static inline bool fault_is_perm(int flags)
{
	return flags & FAULT_ERROR_PERM;
}

static void raise_fault()
{
	panic("A");
}

struct vmap *vm_context_lookup_vmap(struct vm_context *ctx, uintptr_t virt)
{
	struct rbnode *node =
	  rb_search(&ctx->root, virt / mm_objspace_region_size(), struct vmap, node, vmap_compar_key);
	return node ? rb_entry(node, struct vmap, node) : NULL;
}

static void vm_context_add_vmap(struct vm_context *ctx, struct vmap *vmap)
{
	if(!rb_insert(&ctx->root, vmap, struct vmap, node, vmap_compar))
		panic("overwritten vmap");
	uint64_t flags = 0;
	if(vmap->flags & VE_READ)
		flags |= MAP_READ;
	if(vmap->flags & VE_WRITE)
		flags |= MAP_WRITE;
	if(vmap->flags & VE_EXEC)
		flags |= MAP_EXEC;
	arch_mm_map(ctx,
	  vmap->slot * mm_objspace_region_size(),
	  vmap->omap->region->addr,
	  mm_objspace_region_size(),
	  flags);
}

static void vm_context_remove_vmap(struct vm_context *ctx, struct vmap *vmap)
{
	rb_delete(&vmap->node, &ctx->root);
	arch_mm_unmap(ctx, vmap->slot * mm_objspace_region_size(), mm_objspace_region_size());
}

static bool read_view_entry(struct object *view, size_t slot, objid_t *id, uint32_t *veflags)
{
	struct viewentry ve;

	obj_read_data(current_thread->thrctrl,
	  offsetof(struct twzthread_ctrl_repr, fixed_points) + sizeof(struct viewentry) * slot,
	  sizeof(ve),
	  &ve);

	if(ve.flags & VE_VALID) {
		*id = ve.id;
		*veflags = ve.flags;
		return true;
	}

	obj_read_data(
	  view, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), &ve);
	*id = ve.id;
	*veflags = ve.flags;
	// printk("reading view entry %ld: " IDFMT " %x\n", slot, IDPR(*id), *veflags);
	return !!(ve.flags & VE_VALID);
}

void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags)
{
	/* TODO: ensure addr is user */
	// printk("A: %ld %lx %lx %x\n", current_thread->id, ip, addr, flags);

	if(fault_is_perm(flags)) {
		printk("perm fault\n");

		uint32_t veflags;
		objid_t id;
		size_t slot = addr / OBJ_MAXSIZE;
		if(!read_view_entry(current_thread->ctx->viewobj, slot, &id, &veflags)) {
			raise_fault();
			return;
		}
		printk("==> " IDFMT " %x\n", IDPR(id), veflags);

		raise_fault();
		return;
	}

	size_t off = addr % OBJ_MAXSIZE;

	if(off < OBJ_NULLPAGE_SIZE) {
		printk("null fault\n");
		raise_fault();
		return;
	}

	uint32_t veflags;
	objid_t id;
	size_t slot = addr / OBJ_MAXSIZE;
	if(!read_view_entry(current_thread->ctx->viewobj, slot, &id, &veflags)) {
		raise_fault();
		return;
	}

	if(flag_mismatch(flags, veflags)) {
		printk("mismatch fault\n");
		raise_fault();
		return;
	}

	struct object *obj = obj_lookup(id, 0);
	if(!obj) {
		printk("no obj\n");
		raise_fault();
		return;
	}

	struct omap *omap = mm_objspace_get_object_map(obj, off / mm_page_size(0));
	struct vmap *vmap = vmap_create(addr, omap, veflags);

	// printk("mapping slot %p %ld: %p\n", current_thread->ctx, vmap->slot, vmap);

	vm_context_add_vmap(current_thread->ctx, vmap);
	obj_put(obj);
}

struct object *vm_vaddr_lookup_obj(void *a, uint64_t *off)
{
	spinlock_acquire_save(&current_thread->ctx->lock);

	uintptr_t addr = (uintptr_t)a;
	struct vmap *vmap = vm_context_lookup_vmap(current_thread->ctx, addr);

	if(!vmap) {
		uint32_t veflags;
		objid_t id;
		size_t slot = addr / OBJ_MAXSIZE;
		if(!read_view_entry(current_thread->ctx->viewobj, slot, &id, &veflags)) {
			spinlock_release_restore(&current_thread->ctx->lock);
			return NULL;
		}
		struct object *obj = obj_lookup(id, 0);
		if(!obj) {
			spinlock_release_restore(&current_thread->ctx->lock);
			return NULL;
		}

		struct omap *omap = mm_objspace_get_object_map(obj, (addr % OBJ_MAXSIZE) / mm_page_size(0));
		vmap = vmap_create(addr, omap, veflags);
		// printk("mapping slot %p %ld: %p\n", current_thread->ctx, vmap->slot, vmap);
		vm_context_add_vmap(current_thread->ctx, vmap);
	}

	krc_get(&vmap->omap->obj->refs);
	struct object *obj = vmap->omap->obj;
	if(off)
		*off = addr % OBJ_MAXSIZE;

	spinlock_release_restore(&current_thread->ctx->lock);
	return obj;
}

bool vm_setview(struct thread *t, struct object *viewobj)
{
	for(int i = 0; i < MAX_BACK_VIEWS; i++) {
		if(t->backup_views[i].id == viewobj->id) {
			t->ctx = t->backup_views[i].ctx;
			return true;
		}
	}

	printk("SETTING VIEW for %ld to " IDFMT "\n", t->id, IDPR(viewobj->id));
	t->ctx = vm_context_create();
	krc_get(&viewobj->refs);
	struct kso_view *kv = object_get_kso_data_checked(viewobj, KSO_VIEW);
	/* TODO: better locking */
	spinlock_acquire_save(&viewobj->lock);
	list_insert(&kv->contexts, &t->ctx->entry);
	t->ctx->viewobj = viewobj;
	spinlock_release_restore(&viewobj->lock);

	for(int i = 0; i < MAX_BACK_VIEWS; i++) {
		if(t->backup_views[i].id == 0) {
			t->backup_views[i].id = viewobj->id;
			t->backup_views[i].ctx = t->ctx;
			return true;
		}
	}
	return true;
}

static void __view_ctor(struct object *obj)
{
	struct kso_view *kv = obj->kso_data = kalloc(sizeof(struct kso_view), 0);
	list_init(&kv->contexts);
}

static bool _vm_view_invl(struct object *obj, struct kso_invl_args *invl)
{
	if(invl->length == 0)
		return true;
	struct rwlock_result res = rwlock_rlock(&obj->rwlock, 0);

	struct kso_view *view = object_get_kso_data_checked(obj, KSO_VIEW);
	if(!view)
		return false;
	// printk("::::::::: %lx %x\n", invl->offset, invl->length);
	// printk("invl :::: %p %p\n", current_thread->ctx->viewobj, obj);

	foreach(e, list, &view->contexts) {
		struct vm_context *ctx = list_entry(e, struct vm_context, entry);
		// printk("invalidating context :: %p (%p)\n", ctx, current_thread->ctx);
		spinlock_acquire_save(&ctx->lock);
		size_t slot_start = invl->offset / mm_objspace_region_size();
		size_t slot_count = ((invl->length - 1) / mm_objspace_region_size()) + 1;
		size_t slot_end = slot_start + slot_count;

		struct rbnode *node;
		for(; slot_start < slot_end; slot_start++) {
			/* TODO: make this faster */
			node = rb_search(&ctx->root, slot_start, struct vmap, node, vmap_compar_key);
			// printk("search slot %p %ld: %p\n", ctx, slot_start, node);
			if(node) {
				break;
			}
		}

		struct rbnode *next;
		for(; node && slot_start < slot_end; node = next) {
			next = rb_next(node);
			struct vmap *vmap = rb_entry(node, struct vmap, node);
			if(vmap->slot < slot_end) {
				//		printk("UNMAP VIA INAL %ld\n", vmap->slot);
				vm_context_remove_vmap(ctx, vmap);
				printk("TODO: arch-dep\n");
				asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
			} else {
				next = NULL;
			}
		}

		spinlock_release_restore(&ctx->lock);
	}

	rwlock_runlock(&res);

	if(invl->offset == 0x400680000000) {
		// arch_mm_print_ctx(current_thread->ctx);
	}

	return true;
#if 0
	spinlock_acquire_save(&obj->lock);

	foreach(e, list, &obj->view.contexts) {
		struct vm_context *ctx = list_entry(e, struct vm_context, entry);
		spinlock_acquire_save(&ctx->lock);

		for(size_t slot = invl->offset / mm_page_size(MAX_PGLEVEL);
		    slot <= (invl->offset + invl->length) / mm_page_size(MAX_PGLEVEL);
		    slot++) {
			struct rbnode *node = rb_search(&ctx->root, slot, struct vmap, node, vmap_compar_key);
			if(node) {
				struct vmap *map = rb_entry(node, struct vmap, node);
#if CONFIG_DEBUG_OBJECT_SLOT
				printk("UNMAP VIA INVAL: " IDFMT " mapcount %ld\n",
				  IDPR(map->obj->id),
				  map->obj->mapcount.count);
#endif
				if(map->obj != obj) {
					vm_map_disestablish(ctx, map);
				} else {
					/* TODO: right now this is okay, but maybe we'll want to handle this case in the
					 * future to be more general. Basically, if we invalidate the entry holding the
					 * object defining the view we're invalidating in, we'd deadlock. But ... I
					 * don't think this should ever happen. */
					printk("[vm] warning - invalidating view entry of invalidation target\n");
				}
			}
		}

		spinlock_release_restore(&ctx->lock);
	}
	spinlock_release_restore(&obj->lock);
	return true;
#endif
}

static struct kso_calls _kso_view = {
	.ctor = __view_ctor,
	.dtor = NULL,
	.attach = NULL,
	.detach = NULL,
	.invl = _vm_view_invl,
};

__initializer static void _init_kso_view(void)
{
	kso_register(KSO_VIEW, &_kso_view);
}
