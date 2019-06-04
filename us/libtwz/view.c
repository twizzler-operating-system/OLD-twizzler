#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/_objid.h>
#include <twz/_slots.h>
#include <twz/_sys.h>
#include <twz/_view.h>
#include <twz/obj.h>
static inline int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -1;
	}
	struct viewentry *ves =
	  obj ? (struct virtentry *)obj->base : (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	uint32_t old = atomic_fetch_and(&ves[slot].flags, ~VE_VALID);
	ves[slot].id = target;
	ves[slot].res0 = 0;
	ves[slot].res1 = 0;
	atomic_store(&ves[slot].flags, flags | VE_VALID);

	if(old & VE_VALID) {
		struct sys_invalidate_op op = {
			.offset = slot * OBJ_MAXSIZE,
			.length = 1,
			.flags = KSOI_VALID,
		};
		sys_invalidate(&op, 1);
	}

	return 0;
}
