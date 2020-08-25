#include <twz/obj.h>
#include <twz/view.h>

void twz_secure_api_create(twzobj *obj)
{
	struct secure_api_header *hdr = twz_object_base(obj);

	twzobj view;
	twz_view_object_init(&view);
	hdr->view = twz_object_guid(&view);
}

int do_the_thing(struct secure_api_header *hdr, int arg)
{
	twz_secure_api_call1(hdr, DOTHETHING_GATE, arg);
}

#define twz_secure_api_call(hdr, gate, ...)                                                        \
	({                                                                                             \
		twz_secure_api_setup_tmp_stack();                                                          \
		struct sys_become_args args = {                                                            \
			.target_view = hdr->view,                                                              \
			.target_rip = TWZ_GATE_CALL(NULL, gate),                                               \
			.rsp = (void *)(TWZSLOT_TMPSTACK * OBJ_MAXSIZE + 0x200000),                            \
		};                                                                                         \
		long r = sys_become(&args, 0, 0);                                                          \
		r;                                                                                         \
	})
