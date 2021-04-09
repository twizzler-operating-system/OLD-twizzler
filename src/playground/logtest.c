#include <stdio.h>

#include <twz/gate.h>

#include <logboi/logboi.h>

#include <twz/io.h>
#include <twz/thread.h>

#include <twz/name.h>
#include <twz/security.h>

int main()
{
	struct secure_api api;
	if(twz_secure_api_open_name("/dev/logboi", &api)) {
		fprintf(stderr, "couldn't open logboi API\n");
		return 1;
	}
	objid_t id = 0;
	int r = logboi_open_connection(&api, "test-program", 0, &id);
	// printf("::: %d : " IDFMT "\n", r, IDPR(id));

	// printf("logtest thr id : " IDFMT "\n", IDPR(twz_thread_repr_base()->reprid));
	twzobj logbuf;
	twz_object_init_guid(&logbuf, id, FE_READ | FE_WRITE);
	twzio_write(&logbuf, "logging test!\n", 14, 0, 0);

	twzobj obj;
	if((r = twz_object_new(&obj, NULL, TWZ_KU_USER, TWZ_OC_TIED_NONE | TWZ_OC_DFL_READ))) {
		fprintf(stderr, "failed to create test obj: %d\n", r);
		return 1;
	}

	fprintf(stderr, ":::: %p" IDFMT "\n", &obj, IDPR(twz_object_guid(&obj)));
	twz_object_set_user_perms(&obj, SCP_READ | SCP_WRITE);

	twz_name_assign(twz_object_guid(&obj), "/testobj");

	volatile int *x = twz_object_base(&obj);
	*x = 123;
	twz_object_setsz(&obj, TWZ_OSSM_ABSOLUTE, 4);

	// sleep(1);
	//	rr = twzio_write(&logbuf, "another logging test!\n", 22, 0, 0);
	// sleep(1);
}
