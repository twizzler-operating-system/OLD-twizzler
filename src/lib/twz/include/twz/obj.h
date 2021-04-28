#pragma once

enum object_backing_type {
	OBJ_VOLATILE = 0,
	OBJ_PERSISTENT = 1,
	OBJ_DMA = 3,
};

#define OBJ_NULLPAGE_SIZE 0x1000
#define OBJ_METAPAGE_SIZE 0x800
#define OBJ_MAXSIZE (1ul << 30)

#define TWZ_OC_HASHDATA 0x1
#define TWZ_OC_DFL_READ 0x4
#define TWZ_OC_DFL_WRITE 0x8
#define TWZ_OC_DFL_EXEC 0x10
#define TWZ_OC_DFL_USE 0x20
#define TWZ_OC_DFL_DEL 0x40
#define TWZ_OC_ZERONONCE 0x1000
#define TWZ_OC_TIED_NONE 0x10000
#define TWZ_OC_TIED_VIEW 0x20000

#ifndef __KERNEL__

#include <stddef.h>
#include <stdint.h>

#include <twz/objid.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
#else
#include <stdatomic.h>
#endif

struct __twzobj {
	void *base;
#ifdef __cplusplus
	std::
#endif
	  atomic_uint_least64_t flags;
	objid_t id;
	uint32_t vf;
};

typedef struct __twzobj twzobj;

int twz_object_init_ptr(twzobj *, const void *);

int twz_object_init_guid(twzobj *obj, objid_t id, uint32_t flags);

int twz_object_init_name(twzobj *obj, const char *name, uint32_t flags);

void twz_object_release(twzobj *obj);

#define TWZ_KU_USER ((void *)0xfffffffffffffffful)
int twz_object_new(twzobj *obj,
  twzobj *src,
  twzobj *ku,
  enum object_backing_type type,
  uint64_t flags);

#define TWZ_OD_IMMEDIATE 1
int twz_object_delete(twzobj *obj, int flags);

objid_t twz_object_guid(twzobj *o);

void *twz_object_base(twzobj *);
enum twz_object_setsz_mode {
	TWZ_OSSM_ABSOLUTE,
	TWZ_OSSM_RELATIVE,
};

#include <twz/_types.h>
void twz_object_setsz(twzobj *obj, enum twz_object_setsz_mode mode, ssize_t amount);

#ifdef __cplusplus
}
#endif
#endif
