#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/_types.h>
#include <twz/objid.h>

struct __twzobj;
typedef struct __twzobj twzobj;

#ifdef __cplusplus
extern "C" {
#endif

struct twz_nament {
	objid_t id;
	size_t reclen;
	uint64_t flags;
	char name[];
};

#if 0
#endif

int twz_name_dfl_assign(objid_t id, const char *name);
int twz_name_dfl_resolve(const char *name, int, objid_t *);

ssize_t twz_name_dfl_getnames(const char *startname, struct twz_nament *ents, size_t len);

#define TWZ_NAME_RESOLVER_DFL NULL
#define TWZ_NAME_RESOLVER_HIER NULL
#define TWZ_NAME_RESOLVER_SOFN (void *)1ul

#ifdef __cplusplus
}
#endif
