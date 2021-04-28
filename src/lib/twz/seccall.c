#include <stdlib.h>
#include <twz/gate.h>
#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/view.h>

void twz_secure_api_setup_tmp_stack(void)
{
	uint32_t fl;
	twz_view_get(NULL, TWZSLOT_TMPSTACK, NULL, &fl);
	if(!(fl & VE_VALID)) {
		objid_t id;
		if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id) < 0) {
			abort();
		}
		twz_view_fixedset(NULL, TWZSLOT_TMPSTACK, id, VE_VALID | VE_WRITE | VE_READ);
	}
}
