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
int test_fn(int arg)
{
	debug_printf("HELLO FROM TEST FN: %d\n", arg);
	return 1234;
}

TWZ_GATE_SHARED(test_fn, 1);

int do_the_thing(struct secure_api_header *hdr, int arg)
{
	twz_secure_api_call1(hdr, 1, arg);
}

int main()
{
#if 0
	void *dl = dlopen("/usr/lib/stdl.so", RTLD_LAZY | RTLD_GLOBAL);

	if(!dl) {
		errx(1, "dlopen");
	}

	void *sym = dlsym(dl, "__twz_gate_gate_fn");
	void *sym2 = dlsym(dl, "gate_fn");
	if(!sym) {
		errx(1, "dlsym");
	}

	fprintf(stderr, "::::: %p %p\n", sym2, sym);

	void (*fn)(void) = sym;
	fn();

	return 0;
#endif
	twzobj context, pri, pub, dataobj;

	if(twz_object_new(&context, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	twz_sctx_init(&context, "test-context");

	struct sccap *cap;

	if(twz_object_new(&pri, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	if(twz_object_new(&pub, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE)) {
		errx(1, "failed to make new object");
	}

	twz_key_new(&pri, &pub);

	if(twz_object_new(&dataobj, NULL, &pub, 0)) {
		errx(1, "failed to make new object");
	}
	printf("Created object " IDFMT " with KUID " IDFMT "\n",
	  IDPR(twz_object_guid(&dataobj)),
	  IDPR(twz_object_guid(&pub)));

	twz_cap_create(&cap,
	  twz_object_guid(&dataobj),
	  twz_object_guid(&context),
	  SCP_READ,
	  NULL,
	  NULL,
	  SCHASH_SHA1,
	  SCENC_DSA,
	  &pri);

	printf("\n\nAdding cap for data " IDFMT " to " IDFMT "\n",
	  IDPR(twz_object_guid(&dataobj)),
	  IDPR(twz_object_guid(&context)));

	/* probably get the length from some other function? */
	twz_sctx_add(&context, twz_object_guid(&dataobj), cap, sizeof(*cap) + cap->slen, ~0, NULL);

	// if(twz_object_init_name(&libobj_orig, "/usr/bin/st-lib", FE_READ | FE_EXEC)) {
	//	abort();
	//}

	// if(twz_object_new(&libobj, &libobj_orig, &pub, 0)) {
	//	errx(1, "failed to make new lib obj\n");
	//}

	struct scgates gate = {
		.offset = 0x1200,
		.length = 4,
		.align = 4,
	};

	twzobj orig_view, new_view;
	twz_view_object_init(&orig_view);

	twz_object_new(&new_view, &orig_view, &pub, TWZ_OC_VOLATILE);

	twz_cap_create(&cap,
	  twz_object_guid(&new_view),
	  twz_object_guid(&context),
	  SCP_WRITE | SCP_READ | SCP_USE,
	  NULL,
	  NULL,
	  SCHASH_SHA1,
	  SCENC_DSA,
	  &pri);

	twz_sctx_set_gmask(&context, SCP_EXEC);

	twzobj exec = twz_object_from_ptr(main);
	printf("\n\nREGRANT EXEC ON " IDFMT "\n", IDPR(twz_object_guid(&exec)));
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr(libtwz_gate_return);
	printf("\n\nREGRANT EXEC ON " IDFMT "\n", IDPR(twz_object_guid(&exec)));
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	exec = twz_object_from_ptr((void *)0x4000C0069697);
	printf("\n\nREGRANT EXEC ON " IDFMT "\n", IDPR(twz_object_guid(&exec)));
	twz_sctx_add_dfl(&context, twz_object_guid(&exec), SCP_EXEC, NULL, SCF_TYPE_REGRANT_MASK);

	printf("\n\nAdding cap for lib  " IDFMT " to " IDFMT "\n",
	  IDPR(twz_object_guid(&new_view)),
	  IDPR(twz_object_guid(&context)));
	/* probably get the length from some other function? */
	twz_sctx_add(&context, twz_object_guid(&new_view), cap, sizeof(*cap) + cap->slen, ~0, NULL);

	struct secure_api_header sah = {
		.view = twz_object_guid(&new_view),
		.sctx = 0,
	};

	if(!fork()) {
		debug_printf(
		  "DOING BECOME :: " IDFMT " :: %p\n", IDPR(twz_object_guid(&new_view)), test_fn);
		int r = sys_attach(0, twz_object_guid(&context), 0, KSO_SECCTX);
		fprintf(stderr, "ATTACH: %d\n", r);
		/*
		struct sys_become_args args = {
		    .target_view = twz_object_guid(&view),
		    .target_rip = TWZ_GATE_CALL(NULL, 1),
		    .rsp = (void *)(TWZSLOT_STACK * OBJ_MAXSIZE - 0x8000),
		};
		int r = sys_become(&args, 0, 0);*/
		r = do_the_thing(&sah, 5678);
		fprintf(stderr, "ret: become: %d\n", r);
		// child(&context, &dataobj, &libobj);
		exit(0);
	}

	int s;
	wait(&s);

	return 0;
}
