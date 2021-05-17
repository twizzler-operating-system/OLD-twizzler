#include <memory.h>
#include <object.h>
#include <processor.h>
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

static void __view_ctor(struct object *obj)
{
	struct kso_view *kv = obj->kso_data = kalloc(sizeof(struct kso_view), 0);
	list_init(&kv->contexts);
}

static bool _vm_view_invl(struct object *obj, struct kso_invl_args *invl)
{
	panic("A");
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

struct vm_context *vm_context_create(void)
{
	return slabcache_alloc(&sc_vm_context);
}

void vm_context_free(struct vm_context *ctx)
{
	slabcache_free(&sc_vm_context, ctx);
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

bool vm_setview(struct thread *t, struct object *viewobj)
{
	for(int i = 0; i < MAX_BACK_VIEWS; i++) {
		if(t->backup_views[i].id == viewobj->id) {
			t->ctx = t->backup_views[i].ctx;
			return true;
		}
	}
	obj_kso_init(viewobj, KSO_VIEW);

	t->ctx = vm_context_create();
	spinlock_acquire_save(&viewobj->lock);
	krc_get(&viewobj->refs);
	t->ctx->viewobj = viewobj;
	struct kso_view *kv = object_get_kso_data_checked(viewobj, KSO_VIEW);
	list_insert(&kv->contexts, &t->ctx->entry);
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
