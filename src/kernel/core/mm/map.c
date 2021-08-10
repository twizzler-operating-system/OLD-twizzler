#include <kso.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <processor.h>
#include <rwlock.h>
#include <slab.h>
#include <thread.h>
#include <vmm.h>

struct vm_context kernel_ctx;

static DECLARE_SLABCACHE(sc_vmap, sizeof(struct vmap), NULL, NULL, NULL, NULL, NULL);

static void vm_context_remove_vmap(struct vm_context *ctx, struct vmap *vmap)
{
	rb_delete(&vmap->node, &ctx->root);
	arch_mm_unmap(ctx, vmap->slot * mm_objspace_region_size(), mm_objspace_region_size());

	vmap->omap->refs--;
	vmap->omap = NULL;
	obj_put(vmap->obj);
	vmap->obj = NULL;
}

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
	ctx->root = RBINIT;
}

static void vm_context_dtor(void *data __unused, void *ptr)
{
	struct vm_context *ctx = ptr;
	ctx->viewobj = NULL;
	struct rbnode *node, *next = rb_first(&ctx->root);
	while((node = next)) {
		next = rb_next(node);
		struct vmap *map = rb_entry(node, struct vmap, node);
		vm_context_remove_vmap(ctx, map);
		slabcache_free(&sc_vmap, map, NULL);
	}
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

void mm_map(uintptr_t addr, uintptr_t oaddr, size_t len, int flags)
{
	if(flags & MAP_WIRE) {
		assert(addr >= KERNEL_REGION_START);
	}
	assert(addr >= KERNEL_REGION_START);
	arch_mm_map(NULL, addr, oaddr, len, flags);
}

struct vm_context *vm_context_create(void)
{
	return slabcache_alloc(&sc_vm_context, NULL);
}

void vm_context_free(struct vm_context *ctx)
{
	struct kso_view *view = object_get_kso_data_checked(ctx->viewobj, KSO_VIEW);
	struct rwlock_result res = rwlock_wlock(&ctx->viewobj->rwlock, 0);
	assert(view);
	list_remove(&ctx->entry);
	rwlock_wunlock(&res);
	obj_put(ctx->viewobj);
	slabcache_free(&sc_vm_context, ctx, NULL);
}

static struct vmap *vmap_create(uintptr_t addr, struct omap *omap, uint32_t veflags)
{
	struct vmap *vmap = slabcache_alloc(&sc_vmap, NULL);
	vmap->omap = omap;
	vmap->slot = addr / mm_objspace_region_size();
	vmap->flags = veflags;
	vmap->obj = omap->obj;
	krc_get(&vmap->obj->refs);
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

static struct vmap *vm_context_lookup_vmap(struct vm_context *ctx, uintptr_t virt)
{
	struct rbnode *node =
	  rb_search(&ctx->root, virt / mm_objspace_region_size(), struct vmap, node, vmap_compar_key);
	return node ? rb_entry(node, struct vmap, node) : NULL;
}

struct object *vm_context_lookup_object(struct vm_context *ctx, uintptr_t virt)
{
	struct object *obj = NULL;
	spinlock_acquire_save(&ctx->lock);
	struct vmap *vmap = vm_context_lookup_vmap(ctx, virt);
	if(vmap) {
		obj = vmap->obj;
		krc_get(&obj->refs);
	}
	spinlock_release_restore(&ctx->lock);
	return obj;
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

static bool read_view_entry(struct object *view, size_t slot, objid_t *id, uint32_t *veflags)
{
	struct viewentry ve;

	obj_read_data(
	  view, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), &ve);
	*id = ve.id;
	*veflags = ve.flags;
	return !!(ve.flags & VE_VALID);
}

void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags)
{
	assert(current_thread && current_thread->ctx);
	if(VADDR_IS_USER(ip) && !VADDR_IS_USER(addr)) {
		struct fault_exception_info fei = twz_fault_build_exception_info((void *)ip,
		  FAULT_EXCEPTION_SOFTWARE | FAULT_EXCEPTION_PAGEFAULT,
		  addr,
		  ((flags & FAULT_EXEC) ? FEI_EXEC : 0) | ((flags & FAULT_WRITE) ? FEI_WRITE : FEI_READ));
		thread_raise_fault(current_thread, FAULT_EXCEPTION, &fei, sizeof(fei));
		return;
	}

	if(fault_is_perm(flags)) {
		spinlock_acquire_save(&current_thread->ctx->lock);
		struct vmap *vmap = vm_context_lookup_vmap(current_thread->ctx, addr);
		if(!vmap) {
			panic("permission fault raised for non-present mapping");
		}
		struct fault_object_info foi = twz_fault_build_object_info(vmap->obj->id,
		  (void *)ip,
		  (void *)addr,
		  ((flags & FAULT_WRITE) ? FAULT_OBJECT_WRITE : FAULT_OBJECT_READ)
		    | ((flags & FAULT_EXEC) ? FAULT_OBJECT_EXEC : 0));
		spinlock_release_restore(&current_thread->ctx->lock);
		thread_raise_fault(current_thread, FAULT_OBJECT, &foi, sizeof(foi));
		return;
	}

	size_t off = addr % OBJ_MAXSIZE;

	if(off < OBJ_NULLPAGE_SIZE) {
		struct fault_null_info fni = twz_fault_build_null_info((void *)ip, (void *)addr);
		thread_raise_fault(current_thread, FAULT_NULL, &fni, sizeof(fni));
		return;
	}

	uint32_t veflags;
	objid_t id;
	size_t slot = addr / OBJ_MAXSIZE;
	if(!read_view_entry(current_thread->ctx->viewobj, slot, &id, &veflags)) {
		struct fault_object_info foi = twz_fault_build_object_info(0,
		  (void *)ip,
		  (void *)addr,
		  ((flags & FAULT_WRITE) ? FAULT_OBJECT_WRITE : FAULT_OBJECT_READ)
		    | ((flags & FAULT_EXEC) ? FAULT_OBJECT_EXEC : 0) | FAULT_OBJECT_NOMAP);
		thread_raise_fault(current_thread, FAULT_OBJECT, &foi, sizeof(foi));
		return;
	}

	if(flag_mismatch(flags, veflags)) {
		struct fault_object_info foi = twz_fault_build_object_info(id,
		  (void *)ip,
		  (void *)addr,
		  ((flags & FAULT_WRITE) ? FAULT_OBJECT_WRITE : FAULT_OBJECT_READ)
		    | ((flags & FAULT_EXEC) ? FAULT_OBJECT_EXEC : 0));
		thread_raise_fault(current_thread, FAULT_OBJECT, &foi, sizeof(foi));
		return;
	}

	struct object *obj = obj_lookup(id, 0);
	if(!obj) {
		struct fault_object_info foi = twz_fault_build_object_info(id,
		  (void *)ip,
		  (void *)addr,
		  ((flags & FAULT_WRITE) ? FAULT_OBJECT_WRITE : FAULT_OBJECT_READ)
		    | ((flags & FAULT_EXEC) ? FAULT_OBJECT_EXEC : 0) | FAULT_OBJECT_EXIST);
		thread_raise_fault(current_thread, FAULT_OBJECT, &foi, sizeof(foi));
		return;
	}

	struct omap *omap = mm_objspace_get_object_map(obj, off / mm_page_size(0));
	struct vmap *vmap = vmap_create(addr, omap, veflags);

	spinlock_acquire_save(&current_thread->ctx->lock);
	vm_context_add_vmap(current_thread->ctx, vmap);
	spinlock_release_restore(&current_thread->ctx->lock);
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

	t->ctx = vm_context_create();
	krc_get(&viewobj->refs);
	struct kso_view *kv = object_get_kso_data_checked(viewobj, KSO_VIEW);
	struct rwlock_result res = rwlock_wlock(&viewobj->rwlock, 0);
	list_insert(&kv->contexts, &t->ctx->entry);
	t->ctx->viewobj = viewobj;
	rwlock_wunlock(&res);

	for(int i = 0; i < MAX_BACK_VIEWS; i++) {
		if(t->backup_views[i].id == 0) {
			t->backup_views[i].id = viewobj->id;
			t->backup_views[i].ctx = t->ctx;
			return true;
		}
	}
	return true;
}

static bool _vm_view_invl(struct object *obj, struct kso_invl_args *invl)
{
	if(invl->length == 0)
		return true;
	struct rwlock_result res = rwlock_rlock(&obj->rwlock, 0);
	struct kso_view *view = object_get_kso_data_checked(obj, KSO_VIEW);
	if(!view) {
		rwlock_runlock(&res);
		return false;
	}

	foreach(e, list, &view->contexts) {
		struct vm_context *ctx = list_entry(e, struct vm_context, entry);
		spinlock_acquire_save(&ctx->lock);
		size_t slot_start = invl->offset / mm_objspace_region_size();
		size_t slot_count = ((invl->length - 1) / mm_objspace_region_size()) + 1;
		size_t slot_end = slot_start + slot_count;

		struct rbnode *node;
		for(; slot_start < slot_end; slot_start++) {
			/* TODO (opt): make this faster, with a "search next" type lookup */
			node = rb_search(&ctx->root, slot_start, struct vmap, node, vmap_compar_key);
			if(node) {
				break;
			}
		}

		struct rbnode *next;
		for(; node && slot_start < slot_end; node = next) {
			next = rb_next(node);
			struct vmap *vmap = rb_entry(node, struct vmap, node);
			if(vmap->slot < slot_end) {
				vm_context_remove_vmap(ctx, vmap);
				arch_mm_virtual_invalidate(
				  ctx, vmap->slot * mm_objspace_region_size(), mm_objspace_region_size());
				slabcache_free(&sc_vmap, vmap, NULL);
			} else {
				next = NULL;
			}
		}

		spinlock_release_restore(&ctx->lock);
	}

	rwlock_runlock(&res);
	return true;
}

static void __view_ctor(struct object *obj)
{
	struct kso_view *kv = obj->kso_data = kalloc(sizeof(struct kso_view), 0);
	list_init(&kv->contexts);
}

static void __view_dtor(struct object *obj)
{
	struct kso_view *kv = obj->kso_data;
	assert(list_empty(&kv->contexts));
	kfree(obj->kso_data);
}

static struct kso_calls _kso_view = {
	.ctor = __view_ctor,
	.dtor = __view_dtor,
	.attach = NULL,
	.detach = NULL,
	.invl = _vm_view_invl,
};

__initializer static void _init_kso_view(void)
{
	kso_register(KSO_VIEW, &_kso_view);
}
