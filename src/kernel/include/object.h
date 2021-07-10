#pragma once

/** @file
 * @brief Object management
 *
 * See \ref objects for an overview of objects.
 */

#include <arch/object.h>
#include <krc.h>
#include <lib/inthash.h>
#include <lib/rb.h>
#include <lib/vector.h>
#include <rwlock.h>
#include <spinlock.h>
#include <twz/obj.h>
#include <twz/sys/kso.h>
#include <twz/sys/obj.h>

struct object_tie {
	struct object *child;
	struct rbnode node;
	size_t count;
};

#define OF_PINNED 1
#define OF_IDCACHED 2
#define OF_IDSAFE 4
#define OF_KERNEL 8
#define OF_PERSIST 0x10
#define OF_ALLOC 0x20
#define OF_CPF_VALID 0x40
#define OF_DELETE 0x80
#define OF_HIDDEN 0x100
#define OF_PAGER 0x200
#define OF_PARTIAL 0x400

struct object {
	uint128_t id;
	struct krc refs;
	_Atomic uint64_t flags;

	uint32_t cache_mode;
	uint32_t cached_pflags;

	_Atomic enum kso_type kso_type;
	void *kso_data;
	struct kso_calls *kso_calls;
	long (*kaction)(struct object *, long, long);

	/* general object lock */
	struct spinlock lock;

	/* lock for object contents */
	struct rwlock rwlock;

	/* lock for thread-sync operations */
	struct spinlock tslock;

	struct spinlock sleepers_lock;
	struct list sleepers;

	struct rbroot tstable_root, page_requests_root;
	struct rbroot range_tree, omap_root;
	struct rbroot ties_root;
	struct rbnode node;
};

void obj_print_stats(void);

struct object *obj_create(uint128_t id, enum kso_type);
struct object *obj_create_clone(uint128_t id, struct object *, enum kso_type ksot);

#define OBJ_LOOKUP_HIDDEN 1
struct object *obj_lookup(uint128_t id, int flags);
bool obj_verify_id(struct object *obj, bool cache_result, bool uncache);
void obj_put(struct object *o);
void obj_assign_id(struct object *obj, objid_t id);
objid_t obj_compute_id(struct object *obj);
void obj_init(struct object *obj);
void obj_tie(struct object *, struct object *);
void obj_tie_free(struct object *obj);
int obj_untie(struct object *parent, struct object *child);

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val);
bool obj_get_pflags(struct object *obj, uint32_t *pf);
int obj_check_permission(struct object *obj, uint64_t flags);
int obj_check_permission_ip(struct object *obj, uint64_t flags, uint64_t ip);

#define OP_LP_ZERO_OK 1
#define OP_LP_DO_COPY 2
struct page;
int object_operate_on_locked_page(struct object *obj,
  size_t page,
  int flags,
  void (*fn)(struct object *obj, size_t, struct page *page, void *data, uint64_t),
  void *data);

void object_insert_page(struct object *obj, size_t pagenr, struct page *page);

struct object_copy_spec {
	struct object *src;
	size_t start_src;
	size_t start_dst;
	size_t length;
};

void object_copy(struct object *dest, struct object_copy_spec *specs, size_t count);
