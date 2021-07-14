#pragma once

/** @file
 * @brief Slab allocator routines.
 *
 * The slab allocator manages collections of same-sized (and if this was a language that didn't
 * suck, I would just say same _type_ but oh well) objects. Yes, Twizzler also has a notion of
 * "objects" but for this file, what we're talking about is like a standard, programming-language
 * object, a piece of data that is like a struct foo or something.
 *
 * For each type of object you want to manage in a slab allocator, you create a slabcache. The
 * slabcache maintains a set of slabs, which are contiguous regions of virtual memory that contain a
 * set of objects. Allocation works by finding a slab that has an available object, running a 'ctor'
 * (constructor) function on it, and returning a pointer to that object. Freeing runs a 'dtor'
 * (destructor) function on an object and marks is as free for future allocation.
 *
 * However, if there are no slabs with available objects, we allocate a new slab (and thus a bunch
 * of new objects), and run an 'init' (initialization) function on each object in the slab. If we
 * decide to destroy a slab (because none of its objects are allocated and we're facing memory
 * pressure), then we run a 'fini' (finalization) function on each object in the slab before
 * returning that slab back for other use in the memory system.
 *
 * So, an object has a lifecycle:
 *    - init (go from zero'd to initialized). Used for, say, initing a linked list in the object, or
 *      creating a spinlock in the object.
 *    - ctor (go from initialized to constructed). Used for, say, assigning an ID to the object, or
 *      any other state that must be set up on each allocation.
 *    - allocated
 *    - dtor (go from allocated to destructed). Should be put back into a state where we can later
 *      allocate and ctor the object and have its contents be fine for use.
 *    - fini (go from destructed to ready-for-free). Should be used to free anything allocated in
 *      the object.
 *
 * Often, to improve allocation time for objects that have pointers to allocated memory inside them,
 * we can put those allocations in the init function and the freeing inside the fini function. This
 * makes allocation much faster, at the cost of having cached these allocations.
 *
 * The init, ctor, dtor, and fini functions take two arguments, and must always succeed. They cannot
 * allocate data from slabcaches. The first argument is a data pointer (discussed below) and the
 * second is the object from the slab (eg if allocating, it's the object to construct). For init and
 * fini functions, the data pointer is controlled by the slabcache (the _pt argument of
 * DECLARE_SLABCACHE). For ctor and dtor the data pointer is passed to the allocation and free
 * routines respectively.
 */

#include <lib/list.h>
#include <spinlock.h>

#define SLAB_CANARY 0x12345678abcdef55

#pragma clang diagnostic push
/* this warning is generated because we have sentries in slabcache for the slab lists. It's okay,
 * actually, because we never use the data[] field for these entries. */
#pragma clang diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
struct slabcache;
struct slab {
	uint64_t canary;
	struct kheap_run *run;
	struct slab *next, *prev;
	unsigned __int128 alloc;
	struct slabcache *slabcache;
	_Alignas(16) char data[];
};

struct slabcache {
	const char *name;
	uint64_t canary;
	struct slab empty, partial, full;
	void (*init)(void *, void *);
	void (*ctor)(void *, void *);
	void (*dtor)(void *, void *);
	void (*fini)(void *, void *);
	size_t sz;
	void *ptr;
	struct spinlock lock;
	int __cached_nr_obj;
	_Atomic bool __init;
	struct list entry;
	struct {
		size_t empty, partial, full, total_slabs, total_alloced, total_freed, current_alloced;
	} stats;
};
#pragma clang diagnostic pop

#define SLAB_MARKER_MAGIC 0x885522aa

struct slabmarker {
	uint32_t marker_magic;
	uint32_t slot;
	uint64_t pad;
};

/** Declare a slabcache, from which slabs will be managed for a particular data type. This declares
 * a (probably file level) struct slabcache and initializes it. These parameters are the same as
 * \ref slabcache_init (except that function takes a pointer to the slabcache to initialize as its
 * first argument).
 * @param _name the name of this slabcache, for later statistics collection and debugging
 * @param _sz the size of the data types managed by these slabs
 * @param in init function for objects, run on each object when allocating a slab of objects.
 * @param ct ctor function for objects, run on each allocation from the slab cache on the allocated
 * object.
 * @param dt dtor function for objects, run on each free of an object back into a slab.
 * @param fi fini function for objects, run on each object in a slab when destroying a slab.
 * @param _pt user-controlled data pointer passed to init and fini functions.
 */
#define DECLARE_SLABCACHE(_name, _sz, in, ct, dt, fi, _pt)                                         \
	struct slabcache _name = {                                                                     \
		.name = #_name,                                                                            \
		.empty.next = &_name.empty,                                                                \
		.partial.next = &_name.partial,                                                            \
		.full.next = &_name.full,                                                                  \
		.sz = _sz + sizeof(struct slabmarker),                                                     \
		.init = in,                                                                                \
		.ctor = ct,                                                                                \
		.dtor = dt,                                                                                \
		.fini = fi,                                                                                \
		.ptr = _pt,                                                                                \
		.lock = SPINLOCK_INIT,                                                                     \
		.canary = SLAB_CANARY,                                                                     \
		.__cached_nr_obj = 0,                                                                      \
	}

/** Initialize a slabcache.
 * @param c the slabcache to initialize
 * @param name the name of this slabcache, for later statistics collection and debugging
 * @param sz the size of the data types managed by these slabs
 * @param init init function for objects, run on each object when allocating a slab of objects.
 * @param ctor ctor function for objects, run on each allocation from the slab cache on the
 * allocated object.
 * @param dtor dtor function for objects, run on each free of an object back into a slab.
 * @param fini fini function for objects, run on each object in a slab when destroying a slab.
 * @param ptr user-controlled data pointer passed to init and fini functions.
 */
void slabcache_init(struct slabcache *c,
  const char *name,
  size_t sz,
  void (*init)(void *, void *),
  void (*ctor)(void *, void *),
  void (*dtor)(void *, void *),
  void (*fini)(void *, void *),
  void *ptr);

/** Free any slabs that dont have any allocated objects (running the fini function on all of them
 * first). */
void slabcache_reap(struct slabcache *c);

/** Free an object back into a slab, running c->dtor on it first.
 * @param c the slab cache to free back into
 * @param obj the object we are freeing
 * @param ptr the data pointer passed to dtor function
 */
void slabcache_free(struct slabcache *c, void *obj, void *ptr);

/** Allocate an object from a slab. If no slabs are available, create a new one and run c->init on
 * all the objects within. After finding an object, run ctor on it and return it.
 * @param c the slabcache to allocate from
 * @param ptr a data pointer passed to ctor.
 * @return an allocated object
 */
void *slabcache_alloc(struct slabcache *c, void *ptr);
void slabcache_all_print_stats(void);
void slabcache_print_stats(struct slabcache *sc);
