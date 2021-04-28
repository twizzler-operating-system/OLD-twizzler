#pragma once

#include <twz/objid.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __twzobj;
typedef struct __twzobj twzobj;

int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);
#define TIE_UNTIE 1
int twz_object_tie(twzobj *p, twzobj *c, int flags);
int twz_object_tie_guid(objid_t pid, objid_t cid, int flags);
int twz_object_delete_guid(objid_t id, int flags);
int twz_object_wire_guid(twzobj *view, objid_t id);
int twz_object_wire(twzobj *view, twzobj *);
int twz_object_unwire(twzobj *view, twzobj *);

int twz_object_kaction(twzobj *obj, long cmd, ...);

int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags);

int twz_object_ctl(twzobj *obj, int cmd, ...);

/* TODO: make these accessible in this file */
struct kernel_ostat;
struct kernel_ostat_page;
int twz_object_kstat(twzobj *obj, struct kernel_ostat *st);
int twz_object_kstat_page(twzobj *obj, size_t pgnr, struct kernel_ostat_page *st);

#ifdef __cplusplus
}
#endif
