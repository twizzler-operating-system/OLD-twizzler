#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <twz/alloc.h>
#include <twz/obj.h>
#include <twz/security.h>

#include <twz/fault.h>
#include <twz/gate.h>
#include <twz/twztry.h>

#include <dlfcn.h>
#include <twz/debug.h>
#include <twz/view.h>

DECLARE_SAPI_ENTRY(test_fn, 1, int, int arg)
{
	debug_printf("Hello from test fn: %d\n", arg);
	return 1234;
}

int do_the_thing(struct secure_api_header *hdr, int arg)
{
	printf("doing the thing\n");
	return twz_secure_api_call1(hdr, 1, arg);
}

int main()
{
	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "test");
	struct secure_api_header *sah = twz_object_base(&api_obj);
	fprintf(stderr, "HJAHAHA\n");

	if(!fork()) {
		//	debug_printf(
		//	  "DOING BECOME :: " IDFMT " :: %p\n", IDPR(twz_object_guid(&new_view)), test_fn);
		//	int r = sys_attach(0, twz_object_guid(&context), 0, KSO_SECCTX);
		//	fprintf(stderr, "ATTACH: %d\n", r);
		/*
		struct sys_become_args args = {
		    .target_view = twz_object_guid(&view),
		    .target_rip = TWZ_GATE_CALL(NULL, 1),
		    .rsp = (void *)(TWZSLOT_STACK * OBJ_MAXSIZE - 0x8000),
		};
		int r = sys_become(&args, 0, 0);*/
		fprintf(stderr, "HELLO, switch to view " IDFMT "\n", IDPR(sah->view));
		int r = do_the_thing(sah, 5678);
		fprintf(stderr, "ret: become: %d\n", r);
		// child(&context, &dataobj, &libobj);
		exit(0);
	}

	int s;
	wait(&s);

	return 0;
}
