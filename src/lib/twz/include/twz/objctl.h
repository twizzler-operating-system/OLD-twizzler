#pragma once

#include <twz/__twz.h>

#include <stdint.h>
#include <twz/_types.h>

#ifdef __cplusplus
extern "C" {
#endif

__must_check int twz_object_kaction(twzobj *obj, long cmd, ...);

__must_check int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags);

__must_check int twz_object_ctl(twzobj *obj, int cmd, ...);

#include <twz/_sys.h>
int twz_object_kstat(twzobj *obj, struct kernel_ostat *st);
int twz_object_kstat_page(twzobj *obj, size_t pgnr, struct kernel_ostat_page *st);

#ifdef __cplusplus
}
#endif
