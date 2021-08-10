/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
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
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE))) {
		return r;
	}
	struct twzthread_repr *newrepr = twz_object_base(&thread);
	newrepr->reprid = twz_object_guid(&thread);

	uint32_t newid = get_new_twix_conn_id();
	assert(newid != 0);
	size_t thrd_slot_nr = TWZSLOT_THREAD_OBJ(newid, TWZSLOT_THREAD_OFFSET_CTRL);
	// twz_view_allocate_slot(NULL, twz_object_guid(&thread), VE_READ | VE_WRITE);
	twz_view_set(NULL, thrd_slot_nr, newrepr->reprid, VE_READ | VE_WRITE);

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
		.thrd_ctrl_reg = thrd_slot_nr * OBJ_MAXSIZE | newid,
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

	twz_object_delete(&thread, 0);
	twz_object_release(&thread);

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
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE))) {
		goto cleanup_view;
	}

	twz_object_tie(&new_view, &thread, 0);
	twz_object_delete(&thread, 0);

	struct twzthread_repr *newrepr = twz_object_base(&thread);
	newrepr->reprid = twz_object_guid(&thread);

	twz_view_set(&new_view,
	  TWZSLOT_THREAD_OBJ(0, TWZSLOT_THREAD_OFFSET_CTRL),
	  newrepr->reprid,
	  VE_READ | VE_WRITE);
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
	twz_view_set(&new_view,
	  TWZSLOT_THREAD_OBJ(0, TWZSLOT_THREAD_OFFSET_STACK),
	  twz_object_guid(&new_stack),
	  VE_READ | VE_WRITE);
	twz_object_tie(&thread, &new_stack, 0);
	twz_object_delete(&new_stack, 0);

	size_t slots_to_copy[] = { TWZSLOT_EXEC_DATA, TWZSLOT_UNIX, TWZSLOT_INTERP_DATA };

	size_t slots_to_tie[] = { 0, TWZSLOT_INTERP };

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
		twz_view_set(&new_view, i, nid, flags);
		if(twz_object_wire_guid(&new_view, nid) < 0)
			abort();
		twz_object_delete_guid(nid, 0);
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
		twz_view_set(&new_view, i, nid, flags);
		if(twz_object_wire_guid(&new_view, nid) < 0)
			abort();
		if(twz_object_delete_guid(nid, 0) < 0)
			abort();
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
		.thrd_ctrl_reg = TWZSLOT_THREAD_OBJ(0, TWZSLOT_THREAD_OFFSET_CTRL) * OBJ_MAXSIZE,
	};

	if((r = sys_thrd_spawn(twz_object_guid(&thread), &sa, 0, NULL))) {
		goto cleanup_stack;
	}

	struct twix_queue_entry tqe2 = build_tqe(TWIX_CMD_WAIT_READY, 0, 0, 1, tqe.ret);
	twix_sync_command(&tqe2);

	twz_object_delete(&new_view, 0);
	twz_object_release(&thread);
	twz_object_release(&new_view);
	twz_object_release(&new_stack);

	return tqe.ret;

cleanup_stack:
	twz_object_delete(&new_view, 0);
cleanup_thread:
	twz_object_release(&thread);
cleanup_view:
	twz_object_delete(&new_view, 0);

	return r;
}
