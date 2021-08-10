#pragma once

#include <stdint.h>
#include <twz/_types.h>
#include <twz/mutex.h>
#include <twz/objid.h>
#include <twz/sys/kso.h>
#include <twz/sys/slots.h>

struct __twzobj;
typedef struct __twzobj twzobj;

#ifdef __cplusplus
#include <atomic>
extern "C" {
#else
#include <stdatomic.h>
#include <stdbool.h>
#endif /* __cplusplus */

#define VE_READ 0x4
#define VE_WRITE 0x8
#define VE_EXEC 0x10
#define VE_VALID 0x1000

#define __VE_OFFSET (KSO_NAME_MAXLEN + 16 + 32)

#define __VE_FAULT_HANDLER_OFFSET (__VE_OFFSET - 32)
#define __VE_DBL_FAULT_HANDLER_OFFSET (__VE_OFFSET - 24)

struct viewentry {
	objid_t id;
	uint64_t res0;
#ifdef __cplusplus
	std::atomic_uint_least32_t flags;
#else
	atomic_uint_least32_t flags;
#endif
	uint32_t res1;
};

#ifndef __cplusplus
_Static_assert(sizeof(struct viewentry) == 32, "");
_Static_assert(offsetof(struct viewentry, flags) == 24, "");
#endif

struct __viewrepr_bucket {
	objid_t id;
	uint32_t slot;
	uint32_t flags;
	int32_t chain;
	uint32_t refs;
};

#define NR_VIEW_BUCKETS 1024

struct twzview_repr {
	struct kso_hdr hdr;
	void *fault_handler;
	void *dbl_fault_handler;
	uint64_t fault_mask;
	uint64_t fault_flags;
	struct viewentry ves[TWZSLOT_MAX_SLOT + 1];
	void *async_entry;
	uint64_t resv;
	struct mutex lock;
	uint8_t bitmap[(TWZSLOT_MAX_SLOT + 1) / 8];
	struct __viewrepr_bucket buckets[NR_VIEW_BUCKETS];
	struct __viewrepr_bucket chain[TWZSLOT_MAX_SLOT + 1];
};

#ifdef __cplusplus
_Static_assert(offsetof(struct twzview_repr, ves) == __VE_OFFSET,
  "Offset of ves must be equal to __VE_OFFSET");

_Static_assert(offsetof(struct twzview_repr, fault_handler) == __VE_FAULT_HANDLER_OFFSET,
  "Offset of fault_handler must be equal to __VE_FAULT_HANDLER_OFFSET");

_Static_assert(offsetof(struct twzview_repr, dbl_fault_handler) == __VE_DBL_FAULT_HANDLER_OFFSET,
  "Offset of fault_handler must be equal to __VE_DBL_FAULT_HANDLER_OFFSET");
#endif

#ifndef __KERNEL__

void twz_view_get(twzobj *obj, size_t slot, objid_t *target, uint32_t *flags);
void twz_view_set(twzobj *obj, size_t slot, objid_t target, uint32_t flags);
void twz_view_object_init(twzobj *obj);

int twz_vaddr_to_obj(const void *v, objid_t *id, uint32_t *fl);
ssize_t twz_view_allocate_slot(twzobj *obj, objid_t id, uint32_t flags);
void twz_view_release_slot(twzobj *obj, objid_t id, uint32_t flags, size_t slot);

#define VIEW_CLONE_ENTRIES 1
#define VIEW_CLONE_BITMAP 2

int twz_view_clone(twzobj *old,
  twzobj *nobj,
  int flags,
  bool (*fn)(twzobj *, size_t, objid_t, uint32_t, objid_t *, uint32_t *));
#endif

#ifdef __cplusplus
}
#endif
