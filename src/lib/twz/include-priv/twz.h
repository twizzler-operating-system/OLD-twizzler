#pragma once

#include <twz/meta.h>
#include <twz/obj.h>

#define twz_slot_to_base(s) ({ (void *)(SLOT_TO_VADDR(s) + OBJ_NULLPAGE_SIZE); })

static inline struct fotentry *_twz_object_get_fote(twzobj *obj, size_t e)
{
	struct metainfo *mi = twz_object_meta(obj);
	return (struct fotentry *)((char *)mi - sizeof(struct fotentry) * e);
}

#define TWZ_OBJ_VALID 1
#define TWZ_OBJ_NORELEASE 2
#define TWZ_OBJ_ID 4
#define TWZ_OBJ_CACHE 64
_Bool objid_parse(const char *name, size_t len, objid_t *id);
void twz_object_lea_add_cache_sofn(twzobj *o, size_t slot, void *fn);
void twz_object_lea_add_cache(twzobj *o, size_t slot, uint64_t res);

#define _FE_ALLOC 0x10000
#define _FE_VALID 0x20000

struct fotentry;
int twz_fot_indirect_resolve(twzobj *obj,
  struct fotentry *fe,
  const void *p,
  void **vptr,
  uint64_t *info);
int twz_name_resolve(twzobj *obj,
  const char *name,
  int (*fn)(twzobj *, const char *, int, objid_t *),
  int flags,
  objid_t *id);
