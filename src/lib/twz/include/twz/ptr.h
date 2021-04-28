#pragma once

#include <stddef.h>
#include <stdint.h>

#include <twz/obj.h>
#include <twz/sys/ptr.h>

#ifdef __cplusplus
extern "C" {
#endif

void *__twz_object_lea_foreign(twzobj *o, const void *p, uint32_t mask);

#define twz_ptr_islocal(p) ({ ((uintptr_t)(p) < OBJ_MAXSIZE); })
__attribute__((const)) static inline void *__twz_object_lea(twzobj *o, const void *p, uint32_t mask)
{
	if(__builtin_expect(twz_ptr_islocal(p), 1)) {
		return (void *)((uintptr_t)o->base + (uintptr_t)p);
	} else {
		void *r = __twz_object_lea_foreign(o, p, mask);
		return r;
	}
}

#define twz_object_lea(o, p) ({ (__typeof__(p)) __twz_object_lea((o), (p), ~0); })
#define twz_object_lea_mask(o, p, f) ({ (typeof(p)) __twz_object_lea((o), (p), (f)); })

// TODO: audit uses of _store_
#define TWZ_PTR_FLAGS_COPY 0xfffffffffffffffful
int __twz_ptr_store_guid(twzobj *o,
  const void **loc,
  twzobj *target,
  const void *p,
  uint64_t flags);

int __twz_ptr_store_name(twzobj *o,
  const void **loc,
  const char *name,
  const void *p,
  const void *resolver,
  uint64_t flags);

void *__twz_ptr_swizzle(twzobj *o, const void *p, uint64_t flags);

#define twz_ptr_store_guid(o, l, t, p, f)                                                          \
	({                                                                                             \
		typeof(*l) _lt = p;                                                                        \
		__twz_ptr_store_guid(o, (const void **)(l), (t), (p), (f));                                \
	})

#define twz_ptr_store_name(o, l, n, p, r, f)                                                       \
	({                                                                                             \
		typeof(*l) _lt = p;                                                                        \
		__twz_ptr_store_name(o, (const void **)(l), (n), (p), (r), (f));                           \
	})

#define twz_ptr_swizzle(o, p, f) ({ (typeof(p)) __twz_ptr_swizzle((o), (p), (f)); })

#ifdef __cplusplus
}
#endif
