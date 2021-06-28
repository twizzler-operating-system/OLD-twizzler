#include <clksrc.h>
#include <kalloc.h>
#include <kso.h>
#include <lib/bitmap.h>
#include <lib/blake2.h>
#include <lib/iter.h>
#include <lib/rb.h>
#include <memory.h>
#include <nvdimm.h>
#include <object.h>
#include <objspace.h>
#include <page.h>
#include <pager.h>
#include <processor.h>
#include <range.h>
#include <slab.h>
#include <tmpmap.h>
#include <twz/meta.h>
#include <twz/sys/syscall.h>

static _Atomic size_t obj_count = 0;
static struct rbroot obj_tree = RBINIT;

static struct spinlock objlock = SPINLOCK_INIT;

void obj_print_stats(void)
{
	printk("KNOWN OBJECTS: %ld\n", obj_count);
}

static int __obj_compar_key(struct object *a, objid_t b)
{
	if(a->id > b)
		return 1;
	else if(a->id < b)
		return -1;
	return 0;
}

static int __obj_compar(struct object *a, struct object *b)
{
	return __obj_compar_key(a, b->id);
}

static void _obj_init(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->tslock = SPINLOCK_INIT;
	obj->omap_root = RBINIT;
	obj->range_tree = RBINIT;
	obj->tstable_root = RBINIT;
	obj->page_requests_root = RBINIT;
	obj->kso_type = KSO_NONE;
}

void obj_init(struct object *obj)
{
	_obj_init(NULL, obj);
	obj->flags = 0;
	obj->id = 0;
	krc_init(&obj->refs);
	obj->ties_root = RBINIT;
	obj->range_tree = RBINIT;
	list_init(&obj->sleepers);
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	assert(krc_iszero(&obj->refs));
}

static DECLARE_SLABCACHE(sc_objs, sizeof(struct object), _obj_init, NULL, _obj_dtor, NULL, NULL);

static struct kso_calls *_kso_calls[KSO_MAX];

void kso_register(int t, struct kso_calls *c)
{
	_kso_calls[t] = c;
}

void kso_detach_event(struct thread *thr, bool entry, int sysc)
{
	for(size_t i = 0; i < KSO_MAX; i++) {
		if(_kso_calls[i] && _kso_calls[i]->detach_event) {
			_kso_calls[i]->detach_event(thr, entry, sysc);
		}
	}
}

void obj_kso_init(struct object *obj, enum kso_type ksot)
{
	obj->kso_type = ksot;
	obj->kso_calls = _kso_calls[ksot];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}

void object_init_kso_data(struct object *obj, enum kso_type kt)
{
	obj->kso_type = kt;
	obj->kso_calls = _kso_calls[kt];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}

static inline struct object *__obj_alloc(enum kso_type ksot, objid_t id)
{
	struct object *obj = slabcache_alloc(&sc_objs, NULL);

	obj_init(obj);
	obj->id = id;
	obj_kso_init(obj, ksot);

	obj_count++;

	return obj;
}

struct object *obj_create(uint128_t id, enum kso_type ksot)
{
	struct object *obj = __obj_alloc(ksot, id);
	if(id) {
		spinlock_acquire_save(&objlock);
		if(rb_search(&obj_tree, id, struct object, node, __obj_compar_key)) {
			panic("duplicate object created");
		}
		rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

void obj_assign_id(struct object *obj, objid_t id)
{
	spinlock_acquire_save(&objlock);
	if(obj->id) {
		panic("tried to reassign object ID");
	}
	if(rb_search(&obj_tree, id, struct object, node, __obj_compar_key)) {
		panic("duplicate object created");
	}
	obj->id = id;
	rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
	spinlock_release_restore(&objlock);
}

struct object *obj_lookup(uint128_t id, int flags)
{
	spinlock_acquire_save(&objlock);
	struct rbnode *node = rb_search(&obj_tree, id, struct object, node, __obj_compar_key);

	struct object *obj = node ? rb_entry(node, struct object, node) : NULL;
	if(node) {
		krc_get(&obj->refs);
		spinlock_release_restore(&objlock);

		spinlock_acquire_save(&obj->lock);
		if((obj->flags & OF_HIDDEN) && !(flags & OBJ_LOOKUP_HIDDEN)) {
			spinlock_release_restore(&obj->lock);
			obj_put(obj);
			return NULL;
		}
		spinlock_release_restore(&obj->lock);
	} else {
		spinlock_release_restore(&objlock);
		return NULL;
	}
	return obj;
}

static void _obj_release(void *_obj)
{
	struct object *obj = _obj;
#if CONFIG_DEBUG_OBJECT_LIFE || 0
	printk("OBJ RELEASE: " IDFMT " (%d)\n", IDPR(obj->id), obj->kso_type);
#endif
	if(obj->flags & OF_DELETE) {
#if CONFIG_DEBUG_OBJECT_LIFE || 0
		printk("FINAL DELETE object " IDFMT "\n", IDPR(obj->id));
#endif

		obj_tie_free(obj);

		assert(rb_empty(&obj->tstable_root));
		assert(rb_empty(&obj->page_requests_root));
		assert(rb_empty(&obj->ties_root));

		struct rbnode *next;
		/* TODO: we could collect all the nodes in a vector, reset the tree, and then free() them,
		 * that way we don't need to call rb_delete for each one */
		for(struct rbnode *node = rb_first(&obj->range_tree); node; node = next) {
			next = rb_next(node);
			struct range *range = rb_entry(node, struct range, node);
			range_toss(range);
			rb_delete(node, &obj->range_tree);
			range_free(range);
		}

		for(struct rbnode *node = rb_first(&obj->omap_root); node; node = next) {
			next = rb_next(node);
			struct omap *omap = rb_entry(node, struct omap, objnode);
			rb_delete(node, &obj->omap_root);
			omap_free(omap);
		}

		assert(rb_empty(&obj->range_tree));
		assert(rb_empty(&obj->omap_root));

		if(obj->kso_type != KSO_NONE) {
			if(obj->kso_calls && obj->kso_calls->dtor) {
				obj->kso_calls->dtor(obj);
			}
			obj->kso_type = KSO_NONE;
		}

		obj_count--;
		slabcache_free(&sc_objs, obj, NULL);
	}
}

void obj_put(struct object *o)
{
	if(krc_put_locked(&o->refs, &objlock)) {
		if(o->flags & OF_DELETE) {
			rb_delete(&o->node, &obj_tree);
		}
		spinlock_release_restore(&objlock);
		_obj_release(o);
	}
}

bool obj_get_pflags(struct object *obj, uint32_t *pf)
{
	*pf = 0;
	spinlock_acquire_save(&obj->lock);
	if(obj->flags & OF_CPF_VALID) {
		*pf = obj->cached_pflags;
		spinlock_release_restore(&obj->lock);
		return true;
	}

	spinlock_release_restore(&obj->lock);
	struct metainfo mi;
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	if(mi.magic != MI_MAGIC)
		return false;
	spinlock_acquire_save(&obj->lock);
	*pf = obj->cached_pflags = mi.p_flags;
	atomic_thread_fence(memory_order_seq_cst);
	obj->flags |= OF_CPF_VALID;
	spinlock_release_restore(&obj->lock);
	return true;
}

objid_t obj_compute_id(struct object *obj)
{
	struct metainfo mi = {};
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	atomic_thread_fence(memory_order_seq_cst);

	if(mi.magic != MI_MAGIC) {
		panic("TODO remove this panic");
	}

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &mi.nonce, sizeof(mi.nonce));
	blake2b_update(&S, &mi.p_flags, sizeof(mi.p_flags));
	blake2b_update(&S, &mi.kuid, sizeof(mi.kuid));
	size_t tl = 0;
	if(unlikely(mi.p_flags & MIP_HASHDATA)) {
		size_t slen = mi.sz;
		if(slen > OBJ_TOPDATA)
			slen = OBJ_TOPDATA;
		for(size_t s = 0; s < slen; s += mm_page_size(0)) {
			size_t rem = mm_page_size(0);
			if(s + mm_page_size(0) > slen) {
				rem = slen - s;
			}
			assert(rem <= mm_page_size(0));

			char buf[rem];
			obj_read_data(obj, s, rem, buf);

			blake2b_update(&S, buf, rem);
			tl += rem;
		}
		size_t mdbottom = OBJ_METAPAGE_SIZE + sizeof(struct fotentry) * mi.fotentries;
		size_t pos = OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + mdbottom);
		size_t thispage = mm_page_size(0);
		for(size_t s = pos; s < OBJ_MAXSIZE - OBJ_NULLPAGE_SIZE; s += thispage) {
			size_t offset = pos % mm_page_size(0);
			size_t len = mm_page_size(0) - offset;
			char buf[len];
			obj_read_data(obj, s, len, buf);
			blake2b_update(&S, buf, len);
			tl += len;
		}
	}

	unsigned char tmp[32];
	blake2b_final(&S, tmp, 32);
	// long d = krdtsc();
	_Alignas(16) unsigned char out[16];
	for(int i = 0; i < 16; i++) {
		out[i] = tmp[i] ^ tmp[i + 16];
	}
	return *(objid_t *)out;
}

bool obj_verify_id(struct object *obj, bool cache_result, bool uncache)
{
	/* avoid the lock here when checking for KERNEL because this flag is set during creation and
	 * never unset */
	if(obj->id == 0 || (obj->flags & OF_KERNEL))
		return true;
	bool result = false;
	spinlock_acquire_save(&obj->lock);

	if(obj->flags & OF_IDSAFE) {
		result = !!(obj->flags & OF_IDCACHED);
	} else {
		spinlock_release_restore(&obj->lock);
		objid_t c = obj_compute_id(obj);
		spinlock_acquire_save(&obj->lock);
		result = c == obj->id;
		obj->flags |= result && cache_result ? OF_IDCACHED : 0;
	}
	if(uncache) {
		obj->flags &= ~OF_IDSAFE;
	} else {
		obj->flags |= OF_IDSAFE;
	}
	spinlock_release_restore(&obj->lock);
	return result;
}
