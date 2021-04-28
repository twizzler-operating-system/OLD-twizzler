#pragma once

#define twz_ptr_local(p) ({ (typeof(p))((uintptr_t)(p) & (OBJ_MAXSIZE - 1)); })
#define twz_ptr_rebase(fe, p)                                                                      \
	({ (typeof(p))((uintptr_t)SLOT_TO_VADDR(fe) | (uintptr_t)twz_ptr_local(p)); })
