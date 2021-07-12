#pragma once

#include <lib/list.h>
#include <twz/sys/kso.h>

/** @file
 * @brief Functions for managing Kernel State Objects (KSOs).
 *
 * A kernel state object is an object that the kernel uses to configure the running state of the
 * system. For example, security contexts are KSOs that define access control. KSOs form a graph,
 * where a KSO may attached to another KSO.
 *
 * A KSO has additional data that the kernel manages (can be considered a volatile cache of the data
 * derived from the KSO's persistent data). These are managed by kso_data in struct object (see \ref
 * include/object.h). */

struct thread;
struct sctx;

/** The kernel state struct for a view KSO. */
struct kso_view {
	/** All the struct vm_contexts derived from this view (see \ref include/vmap.h). */
	struct list contexts;
};

/** The kernel state struct for a thread KSO. */
struct kso_throbj {
	/** The running thread based on this KSO */
	struct thread *thread;
};

/** Security context KSO data */
struct kso_sctx {
	/** The security context derived from this KSO */
	struct sctx *sc;
};

/** Invalidation request arguments */
struct kso_invl_args {
	/** Object ID for the KSO we want to invalidate */
	objid_t id;
	/** Start of object data that we are invalidating */
	uint64_t offset;
	/** Length of the region we are invalidating */
	uint32_t length;
	/** Flags for invalidation (see userspace documentation include/twz/sys/syscall.h) */
	uint16_t flags;
	/** Result of invalidation (see userspace documentation include/twz/sys/syscall.h) */
	uint16_t result;
};

struct object;
/** Each KSO type has their own callbacks for each of these actions (this is standard object-based
 * callback). These may be NULL to indicate that the action doesn't exist */
struct kso_calls {
	/** Attach child to parent. Flags current unused (must be zero) */
	bool (*attach)(struct object *parent, struct object *child, int flags);
	/** Detach child from parent. Flags current unused (must be zero) */
	bool (*detach)(struct object *parent, struct object *child, int sysc, int flags);
	/** Deprecated -- TODO: update this */
	bool (*detach_event)(struct thread *thr, bool, int);
	/** Construct the kso_data for this object */
	void (*ctor)(struct object *);
	/** Destruct the kso_data for this object (called when destructing an object) */
	void (*dtor)(struct object *);
	/** Perform invalidation on a KSO */
	bool (*invl)(struct object *, struct kso_invl_args *);
};

/** Register a set of kso_calls with a KSO type (which is itself referenced by an integer -- see
 * userspace documentation include/twz/sys/kso.h). Called during kernel initialization, probably.
 * @param t KSO type
 * @param calls Pointer to set of KSO calls to associate with KSO t. Replaces any existing
 * registration. If calls is NULL, deregister calls for KSO t. */
void kso_register(int t, struct kso_calls *calls);

/** DEPRECATED (TODO: needs replacing) */
void kso_detach_event(struct thread *thr, bool entry, int sysc);

/** Attach a KSO to the root KSO object.
 * @param obj KSO object to attach to the root.
 * @param flags Currently unused (must be zero).
 * @param type The kso type of the child object.
 * @return Attachment number of the child after attaching.
 */
int kso_root_attach(struct object *obj, uint64_t flags, int type);

/** Detach an object from the root KSO.
 * @param idx the attachment number to detach */
void kso_root_detach(int idx);

/** Attach a child to a parent at attachment point loc.
 * @param parent the parent KSO
 * @param child the child KSO
 * @param the attachment number to place child in.
 */
void kso_attach(struct object *parent, struct object *child, size_t loc);

/** Set the name of the KSO (by copying the name into the KSO header) */
void kso_setname(struct object *obj, const char *name);

/** Get the system object (see \ref ksos) */
struct object *kso_get_system_object(void);

/** Lookup the kso_calls associated with kso_type ksot */
struct kso_calls *kso_lookup_calls(enum kso_type ksot);

/** Initialize an object to be a KSO for kso_type kt (runs the KSO's kso_call's ctor function). Not
 * thread-safe. */
void object_init_kso_data(struct object *obj, enum kso_type kt);

/** Get the KSO data for an object, initializing it as a KSO of type kt if it was not previously
 * initialized as a KSO. If the object is already initialized as a KSO that does not match kt,
 * return NULL. The initialization uses double-checked locking to ensure that it only calls ctor
 * once, and any subsequent calls will get the same pointer to the same kso_data. */
void *object_get_kso_data_checked(struct object *obj, enum kso_type kt);
