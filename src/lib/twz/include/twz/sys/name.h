#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct __twzobj;
typedef struct __twzobj twzobj;

twzobj *twz_name_get_root(void);
int twz_name_assign_namespace(objid_t id, const char *name);
int twz_name_switch_root(twzobj *obj);

#ifdef __cplusplus
}
#endif
