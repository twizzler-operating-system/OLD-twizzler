/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

#include "../syscalls.h"
#include "v2.h"

asm(".global __return_from_clone_v2\n"
    "__return_from_clone_v2:"
    "movq $1, %rdi;"
    "callq resetup_queue;"
    "popq %r15;"
    "popq %r14;"
    "popq %r13;"
    "popq %r12;"
    "popq %r11;"
    "popq %r10;"
    "popq %r9;"
    "popq %r8;"
    "popq %rbp;"
    "popq %rsi;"
    "popq %rdi;"
    "popq %rdx;"
    "popq %rbx;"
    "popq %rax;" /* ignore the old rsp */
    "movq $0, %rax;"
    "ret;");

asm(".global __return_from_fork_v2\n"
    "__return_from_fork_v2:"
    "movq $0, %rdi;"
    "callq resetup_queue;"
    "popq %r15;"
    "popq %r14;"
    "popq %r13;"
    "popq %r12;"
    "popq %r11;"
    "popq %r10;"
    "popq %r9;"
    "popq %r8;"
    "popq %rbp;"
    "popq %rsi;"
    "popq %rdi;"
    "popq %rdx;"
    "popq %rbx;"
    "popq %rsp;"
    "movq $0, %rax;"
    "ret;");

#define _GNU_SOURCE
#include <sched.h>
extern uint64_t __return_from_clone_v2(void);
long hook_clone(struct syscall_args *args)
{
	unsigned long flags = args->a0;
	void *child_stack = (void *)args->a1;
	int *ptid = (int *)args->a2;
	int *ctid = (int *)args->a3;
	unsigned long newtls = args->a4;
	if(flags != 0x7d0f00) {
		return -ENOSYS;
	}

	struct twix_register_frame *frame = args->frame;
	memcpy((void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame)),
	  frame,
	  sizeof(struct twix_register_frame));
	child_stack = (void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame));

	int r;
	twzobj thread;
	if((r = twz_object_new(&thread,
	      NULL,
	      NULL,
	      OBJ_VOLATILE,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW))) {
		return r;
	}
	struct twzthread_repr *newrepr = twz_object_base(&thread);
	newrepr->reprid = twz_object_guid(&thread);

	struct twix_queue_entry tqe = build_tqe(
	  TWIX_CMD_CLONE, 0, 0, 3, ID_LO(twz_object_guid(&thread)), ID_HI(twz_object_guid(&thread)), 0);
	twix_sync_command(&tqe);

	if((r = tqe.ret) < 0) {
		return r;
	}

	if(flags & CLONE_CHILD_SETTID) {
		*ctid = tqe.ret;
	}
	struct sys_thrd_spawn_args sa = {
		.target_view = 0,
		.start_func = (void *)__return_from_clone_v2,
		.arg = NULL,
		.stack_base = (void *)child_stack, // twz_ptr_rebase(TWZSLOT_STACK, soff),
		.stack_size = 8,
		.tls_base = (void *)newtls,
		.thrd_ctrl = TWZSLOT_THRD,
	};

	if((r = sys_thrd_spawn(twz_object_guid(&thread), &sa, 0, NULL))) {
		return r;
	}

	void *arg = NULL;
	if(flags & CLONE_CHILD_CLEARTID) {
		/* TODO */
		arg = ctid;
	}

	if(flags & CLONE_PARENT_SETTID) {
		*ptid = tqe.ret;
	}

	return tqe.ret;
}

#include <sys/mman.h>

struct mmap_slot {
	twzobj obj;
	int prot;
	int flags;
	size_t slot;
	struct mmap_slot *next;
};

extern uint64_t __return_from_clone(void);
extern uint64_t __return_from_fork_v2(void);

static bool __fork_view_clone(twzobj *nobj,
  size_t i,
  objid_t oid,
  uint32_t oflags,
  objid_t *nid,
  uint32_t *nflags)
{
	(void)nobj;
	if(i == 0 || (i >= TWZSLOT_ALLOC_START && i <= TWZSLOT_ALLOC_MAX)) {
		*nid = oid;
		*nflags = oflags;
		return true;
	}

	return false;
}

/* TODO: handle cleanup */
long hook_fork(struct syscall_args *args)
{
	/* fork() is a multi-stage process. We need to:
	 * 1) Create a new thread, ready to execute.
	 * 2) Register that thread as a client with the unix-server, indicating that we are
	 * forking. 3) Clone our view object, creating new objects as necessary 4) Setup the
	 * thread to execute a call to the unix-server that registers it as a client, and then
	 * returns using the parent's frame. 5) Spawn the thread.
	 *
	 * The unix-server will recall that we are forking, and will then clone a process
	 * internally, and keep it ready for the new thread. */

	int r;
	twzobj current_view, new_view, new_stack;
	twz_view_object_init(&current_view);

	if((r = twz_object_new(&new_view,
	      &current_view,
	      NULL,
	      OBJ_VOLATILE,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_view_clone(NULL, &new_view, 0, __fork_view_clone))) {
		goto cleanup_view;
	}

	twzobj thread;
	if((r = twz_object_new(&thread,
	      NULL,
	      NULL,
	      OBJ_VOLATILE,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW))) {
		goto cleanup_view;
	}
	struct twzthread_repr *newrepr = twz_object_base(&thread);
	newrepr->reprid = twz_object_guid(&thread);

	twz_view_fixedset(
	  &thread, TWZSLOT_THRD, twz_object_guid(&thread), VE_READ | VE_WRITE | VE_FIXED);

	twz_view_set(&new_view, TWZSLOT_CVIEW, twz_object_guid(&new_view), VE_READ | VE_WRITE);

	void *rsp;
	asm volatile("mov %%rsp, %0" : "=r"(rsp)::"memory");
	twzobj curstack;
	twz_object_init_ptr(&curstack, rsp);

	if((r = twz_object_new(&new_stack,
	      &curstack,
	      NULL,
	      OBJ_VOLATILE,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE))) {
		goto cleanup_thread;
	}
	twz_view_set(&new_view, TWZSLOT_STACK, twz_object_guid(&new_stack), VE_READ | VE_WRITE);

	size_t slots_to_copy[] = {
		1, TWZSLOT_UNIX, 0x10004, 0x10006 /* mmap */
	};

	size_t slots_to_tie[] = { 0, 0x10003 };

	/* TODO: move this all to just mmap */
	for(size_t j = 0; j < sizeof(slots_to_tie) / sizeof(slots_to_tie[0]); j++) {
		size_t i = slots_to_tie[j];
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		if(twz_object_wire_guid(&new_view, id) < 0)
			abort();
	}

	for(size_t j = 0; j < sizeof(slots_to_copy) / sizeof(slots_to_copy[0]); j++) {
		size_t i = slots_to_copy[j];
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		// twix_log("FORK COPY-DERIVE %lx\n", i);
		/* Copy-derive */
		objid_t nid;
		if((r = twz_object_create(
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_TIED_NONE,
		      0,
		      id,
		      &nid))) {
			/* TODO: cleanup */
			return r;
		}
		//	twix_log("FORK COPY-DERIVE %lx: " IDFMT " --> " IDFMT "\n", i, IDPR(id),
		// IDPR(nid));
		if(flags & VE_FIXED) {
		}
		//		twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
		else
			twz_view_set(&new_view, i, nid, flags);
		if(twz_object_wire_guid(&new_view, nid) < 0)
			abort();
		//	if(twz_object_delete_guid(nid, 0) < 0)
		//		abort();
	}

	for(size_t j = TWZSLOT_MMAP_BASE; j < TWZSLOT_MMAP_BASE + TWZSLOT_MMAP_NUM; j++) {
		size_t i = j;
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID) || !(flags & VE_WRITE)) {
			if(flags & VE_VALID) {
				if(twz_object_wire_guid(&new_view, id) < 0)
					abort();
			}
			continue;
		}
		/* Copy-derive */
		objid_t nid;
		if((r = twz_object_create(
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_TIED_NONE,
		      0,
		      id,
		      &nid))) {
			/* TODO: cleanup */
			return r;
		}
		//	twix_log("FORK COPY-DERIVE %lx: " IDFMT " --> " IDFMT "\n", i, IDPR(id),
		// IDPR(nid));
		if(flags & VE_FIXED) {
		}
		//		twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
		else
			twz_view_set(&new_view, i, nid, flags);
		if(twz_object_wire_guid(&new_view, nid) < 0)
			abort();
		//	if(twz_object_delete_guid(nid, 0) < 0)
		//		abort();
	}

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_CLONE,
	  0,
	  0,
	  3,
	  ID_LO(twz_object_guid(&thread)),
	  ID_HI(twz_object_guid(&thread)),
	  TWIX_FLAGS_CLONE_PROCESS);
	twix_sync_command(&tqe);
	if((r = tqe.ret) < 0) {
		goto cleanup_stack;
	}

	uint64_t fs;
	asm volatile("rdfsbase %%rax" : "=a"(fs));

	struct sys_thrd_spawn_args sa = {
		.target_view = twz_object_guid(&new_view),
		.start_func = (void *)__return_from_fork_v2,
		.arg = NULL,
		.stack_base = (void *)args->frame, // twz_ptr_rebase(TWZSLOT_STACK, soff),
		.stack_size = 8,
		.tls_base = (void *)fs,
		.thrd_ctrl = TWZSLOT_THRD,
	};

	if((r = sys_thrd_spawn(twz_object_guid(&thread), &sa, 0, NULL))) {
		goto cleanup_stack;
	}

	struct twix_queue_entry tqe2 = build_tqe(TWIX_CMD_WAIT_READY, 0, 0, 1, tqe.ret);
	twix_sync_command(&tqe2);

	return tqe.ret;

cleanup_stack:
	twz_object_delete(&new_view, 0);
cleanup_thread:
	twz_object_release(&thread);
cleanup_view:
	twz_object_delete(&new_view, 0);

	return r;
#if 0
	int r;
	twzobj view, cur_view;
	twz_view_object_init(&cur_view);

	// debug_printf("== creating view\n");
	if((r = twz_object_new(
	      &view, &cur_view, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE))) {
		return r;
	}

	objid_t vid = twz_object_guid(&view);

	/*if((r = twz_object_wire(NULL, &view)))
	    return r;
	if((r = twz_object_delete(&view, 0)))
	    return r;*/

	// debug_printf("== creating thread\n");

	if(twz_thread_create(&pds[pid].thrd) < 0)
		abort();

	if(twz_view_clone(NULL, &view, 0, __fork_view_clone) < 0)
		abort();

	objid_t sid;
	twzobj stack;
	twz_view_fixedset(
	  &pds[pid].thrd.obj, TWZSLOT_THRD, pds[pid].thrd.tid, VE_READ | VE_WRITE | VE_FIXED);
	/* TODO: handle these */
	if(twz_object_wire_guid(&view, pds[pid].thrd.tid) < 0)
		abort();

	twz_view_set(&view, TWZSLOT_CVIEW, vid, VE_READ | VE_WRITE);

	//	debug_printf("== creating stack\n");
	if((r = twz_object_new(&stack, twz_stdstack, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		twix_log(":: fork create stack returned %d\n", r);
		abort();
	}
	if(twz_object_tie(&pds[pid].thrd.obj, &stack, 0) < 0)
		abort();
	sid = twz_object_guid(&stack);
	twz_view_set(&view, TWZSLOT_STACK, sid, VE_READ | VE_WRITE);
	// twz_object_wire_guid(&view, sid);

	// twix_log("FORK view = " IDFMT ", stack = " IDFMT "\n",
	//  IDPR(twz_object_guid(&view)),
	//  IDPR(twz_object_guid(&stack)));
	size_t slots_to_copy[] = {
		1, TWZSLOT_UNIX, 0x10004, 0x10006 /* mmap */
	};

	size_t slots_to_tie[] = { 0, 0x10003 };

	/* TODO: move this all to just mmap */
	for(size_t j = 0; j < sizeof(slots_to_tie) / sizeof(slots_to_tie[0]); j++) {
		size_t i = slots_to_tie[j];
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		if(twz_object_wire_guid(&view, id) < 0)
			abort();
	}

	for(size_t j = 0; j < sizeof(slots_to_copy) / sizeof(slots_to_copy[0]); j++) {
		size_t i = slots_to_copy[j];
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		// twix_log("FORK COPY-DERIVE %lx\n", i);
		/* Copy-derive */
		objid_t nid;
		if((r = twz_object_create(
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_TIED_NONE,
		      0,
		      id,
		      &nid))) {
			/* TODO: cleanup */
			return r;
		}
		//	twix_log("FORK COPY-DERIVE %lx: " IDFMT " --> " IDFMT "\n", i, IDPR(id), IDPR(nid));
		if(flags & VE_FIXED) {
		}
		//		twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
		else
			twz_view_set(&view, i, nid, flags);
		if(twz_object_wire_guid(&view, nid) < 0)
			abort();
		if(twz_object_delete_guid(nid, 0) < 0)
			abort();
	}

	for(size_t j = TWZSLOT_MMAP_BASE; j < TWZSLOT_MMAP_BASE + TWZSLOT_MMAP_NUM; j++) {
		size_t i = j;
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID) || !(flags & VE_WRITE)) {
			if(flags & VE_VALID) {
				if(twz_object_wire_guid(&view, id) < 0)
					abort();
			}
			continue;
		}
		/* Copy-derive */
		objid_t nid;
		if((r = twz_object_create(
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_TIED_NONE,
		      0,
		      id,
		      &nid))) {
			/* TODO: cleanup */
			return r;
		}
		//	twix_log("FORK COPY-DERIVE %lx: " IDFMT " --> " IDFMT "\n", i, IDPR(id), IDPR(nid));
		if(flags & VE_FIXED) {
		}
		//		twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
		else
			twz_view_set(&view, i, nid, flags);
		if(twz_object_wire_guid(&view, nid) < 0)
			abort();
		if(twz_object_delete_guid(nid, 0) < 0)
			abort();
	}

	// twz_object_wire(NULL, &stack);
	// twz_object_delete(&stack, 0);

	// size_t soff = (uint64_t)twz_ptr_local(frame) - 1024;
	// void *childstack = twz_object_lea(&stack, (void *)soff);

	// memcpy(childstack, frame, sizeof(struct twix_register_frame));

	uint64_t fs;
	asm volatile("rdfsbase %%rax" : "=a"(fs));

	struct sys_thrd_spawn_args sa = {
		.target_view = vid,
		.start_func = (void *)__return_from_fork,
		.arg = NULL,
		.stack_base = (void *)frame, // twz_ptr_rebase(TWZSLOT_STACK, soff),
		.stack_size = 8,
		.tls_base = (void *)fs,
		.thrd_ctrl = TWZSLOT_THRD,
	};

	//	debug_printf("== spawning\n");
	if((r = sys_thrd_spawn(pds[pid].thrd.tid, &sa, 0, NULL))) {
		return r;
	}

	if(twz_object_tie(NULL, &view, TIE_UNTIE) < 0)
		abort();
	if(twz_object_tie(NULL, &stack, TIE_UNTIE) < 0)
		abort();
	twz_object_release(&view);
	twz_object_release(&stack);

	return pid;
#endif
}
