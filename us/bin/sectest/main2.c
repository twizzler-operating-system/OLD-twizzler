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

static long do_the_thing(struct secure_api *api, int arg)
{
	// printf("doing the thing\n");
	return twz_secure_api_call1(api, 1, arg);
}

int main()
{
	struct secure_api api;
	twz_secure_api_open_name("/dev/stapi", &api);

	//	debug_printf(
	//	  "DOING BECOME :: " IDFMT " :: %p\n", IDPR(twz_object_guid(&new_view)), test_fn);
	// fprintf(stderr, "HELLO, switch to view " IDFMT "\n", IDPR(sah->view));
	long a = rdtsc();
	long r = do_the_thing(&api, 5678);
	long iters = 1000;
	long *vals = malloc(sizeof(long) * iters);
	long *vals2 = malloc(sizeof(long) * iters);
	// sys_attach(0, api.hdr->sctx, 0, KSO_SECCTX);
	/*struct sys_become_args args = {
	    .target_view = api.hdr->view,
	    .target_rip = TWZ_GATE_CALL(NULL, 1),
	    .rsp = (void *)(TWZSLOT_STACK * OBJ_MAXSIZE - 0x8000),
	};*/

	for(long i = 0; i < 1000; i++) {
		//	debug_printf("DOING CALL\n");

		struct sys_become_args argsa = {
			.target_view = api.hdr->view,
			.target_rip = (uint64_t)TWZ_GATE_CALL(NULL, 1),
			.rax = 0,
			.rbx = 0,
			.rcx = 0,
			.rdx = 0,
			.rdi = 0,
			.rsi = 0,
			.rsp = 0,
			.rbp = 0,
			.r8 = 0,
			.r9 = 0,
			.r10 = 0,
			.r11 = 0,
			.r12 = 0,
			.r13 = 0,
			.r14 = 0,
			.r15 = 0,
			.sctx_hint = api.hdr->sctx,
		};

		a = rdtsc();
		r = sys_become(&argsa, 0, 0);

		//	r = do_the_thing(&api, 5678);
		long b = rdtsc();
		asm volatile("" ::: "memory");
		struct sys_become_args args = {
			.target_view = api.hdr->view,
			.target_rip = 0,
			.rsp = (void *)123123,
			.r14 = 3,
		};
		asm volatile("" ::"m"(args) : "memory");
		// long r = sys_become(&args, 0, 0);

		//	debug_printf("RETURNED FROM CALL\n");
		vals[i] = r - a;
		vals2[i] = b - r;
	}
	for(long i = 0; i < 1000; i++)
		fprintf(stderr, "### TIME sapi_call %ld %ld\n", vals[i], vals2[i]);

	return 0;
}
