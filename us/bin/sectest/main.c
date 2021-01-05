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

#include <time.h>

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

DECLARE_SAPI_ENTRY(test_fn, 1, long, int arg, long arg2)
{
	long a = rdtsc();
	// debug_printf("INSIDE CALL %ld\n", arg2);
	// debug_printf("Hello from test fn: %d\n", arg);
	return a;
}

long do_the_thing(struct secure_api *api, int arg)
{
	// printf("doing the thing\n");
	return twz_secure_api_call1(api, 1, arg);
}

#include <twz/name.h>
int main()
{
	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "test");
	struct secure_api_header *sah = twz_object_base(&api_obj);

	twz_name_assign(twz_object_guid(&api_obj), "/dev/stapi");

	debug_printf("CREATED CONTEXT " IDFMT "\n", IDPR(sah->sctx));
#if 0
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
		struct secure_api api;
		api.hdr = sah;
		// fprintf(stderr, "HELLO, switch to view " IDFMT "\n", IDPR(sah->view));
		long a = rdtsc();
		long r = do_the_thing(&api, 5678);
		long iters = 1000;
		long *vals = malloc(sizeof(long) * iters);
		for(long i = 0; i < 1000; i++) {
			a = rdtsc();
			debug_printf("DOING CALL\n");
			r = do_the_thing(&api, 5678);
			debug_printf("RETURNED FROM CALL\n");
			vals[i] = r - a;
		}
		for(long i = 0; i < 1000; i++)
			fprintf(stderr, "### TIME sapi_call %ld\n", vals[i]);
		// child(&context, &dataobj, &libobj);
		exit(0);
	}

	int s;
	wait(&s);
#endif
	for(;;)
		usleep(10000000ul);

	return 0;
}
