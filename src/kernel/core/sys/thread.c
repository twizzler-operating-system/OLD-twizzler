#include <kalloc.h>
#include <limits.h>
#include <object.h>
#include <processor.h>
#include <secctx.h>
#include <syscall.h>
#include <thread.h>
#include <vmm.h>

#include <twz/meta.h>
#include <twz/sys/sctx.h>
#include <twz/sys/slots.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

long syscall_thread_spawn(uint64_t tidlo,
  uint64_t tidhi,
  struct sys_thrd_spawn_args *tsa,
  int flags,
  objid_t *tctrl)
{
	if(current_thread && !verify_user_pointer(tsa, sizeof(*tsa))) {
		return -EINVAL;
	}
	if(current_thread && (!verify_user_pointer(tctrl, sizeof(objid_t)) && tctrl != NULL)) {
		return -EINVAL;
	}
	void *start = tsa->start_func;
	void *stack_base = tsa->stack_base;
	void *tls_base = tsa->tls_base;
	if(!verify_user_pointer(start, sizeof(void *))
	   || !verify_user_pointer(stack_base, sizeof(void *))
	   || !verify_user_pointer(tls_base, sizeof(void *))) {
		return -EINVAL;
	}

	if(flags) {
		return -EINVAL;
	}
	objid_t tid = MKID(tidhi, tidlo);
	struct object *repr = obj_lookup(tid, 0);
	if(!repr) {
		return -ENOENT;
	}

	int r;
	if((r = obj_check_permission(repr, SCP_WRITE))) {
		obj_put(repr);
		return r;
	}

	spinlock_acquire_save(&repr->lock);
	if(repr->kso_type != KSO_NONE && repr->kso_type != KSO_THREAD) {
		obj_put(repr);
		return -EINVAL;
	}

	if(repr->kso_type == KSO_NONE) {
		obj_kso_init(repr, KSO_THREAD);
	}
	spinlock_release_restore(&repr->lock);

	struct object *view;
	if(tsa->target_view) {
		view = obj_lookup(tsa->target_view, 0);
		if(view == NULL) {
			obj_put(repr);
			return -ENOENT;
		}
		/* TODO (sec): shoud this be SCP_USE? */
		if((r = obj_check_permission(view, SCP_WRITE))) {
			obj_put(view);
			obj_put(repr);
			return r;
		}
	} else {
		view = current_thread->ctx->viewobj;
		krc_get(&view->refs);
	}

	obj_write_data(repr, offsetof(struct twzthread_repr, reprid), sizeof(objid_t), &tid);

	struct thread *t = thread_create();
	t->thrid = tid;
	t->reprobj = repr; /* krc: move */
	struct kso_throbj *thr = object_get_kso_data_checked(repr, KSO_THREAD);
	thr->thread = t;
	vm_setview(t, view);

	obj_put(view);

	objid_t ctrlid;
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &ctrlid);
	assert(r == 0); // TODO

	t->thrctrl = obj_lookup(ctrlid, 0);

	obj_write_data(t->thrctrl, offsetof(struct twzthread_ctrl_repr, reprid), sizeof(objid_t), &tid);
	obj_write_data(
	  t->thrctrl, offsetof(struct twzthread_ctrl_repr, ctrl_reprid), sizeof(objid_t), &ctrlid);

	struct viewentry ve = {
		.id = ctrlid,
		.flags = VE_READ | VE_WRITE | VE_VALID | VE_FIXED,
	};
	obj_write_data(t->thrctrl,
	  offsetof(struct twzthread_ctrl_repr, fixed_points) + sizeof(struct viewentry) * TWZSLOT_TCTRL,
	  sizeof(struct viewentry),
	  &ve);
	struct viewentry ve2 = {
		.id = tid,
		.flags = VE_READ | VE_WRITE | VE_VALID | VE_FIXED,
	};
	obj_write_data(t->thrctrl,
	  offsetof(struct twzthread_ctrl_repr, fixed_points) + sizeof(struct viewentry) * TWZSLOT_THRD,
	  sizeof(struct viewentry),
	  &ve2);

#if 0
	struct twzthread_ctrl_repr *ctrl_repr = obj_get_kbase(t->thrctrl);
	ctrl_repr->reprid = tid;
	ctrl_repr->ctrl_reprid = ctrlid;
	ctrl_repr->fixed_points[TWZSLOT_TCTRL] = (struct viewentry){
		.id = ctrlid,
		.flags = VE_READ | VE_WRITE | VE_VALID | VE_FIXED,
	};
	ctrl_repr->fixed_points[TWZSLOT_THRD] = (struct viewentry){
		.id = tid,
		.flags = VE_READ | VE_WRITE | VE_VALID | VE_FIXED,
	};
#endif

	t->thrctrl->flags |= OF_DELETE;

	t->kso_attachment_num = kso_root_attach(repr, 0, KSO_THREAD);

	if(current_thread) {
		spinlock_acquire_save(&current_thread->sc_lock);
		for(int i = 0; i < MAX_SC; i++) {
			if(current_thread->sctx_entries[i].context) {
				krc_get(&current_thread->sctx_entries[i].context->refs);
				t->sctx_entries[i].context = current_thread->sctx_entries[i].context;
				t->sctx_entries[i].attr = 0;
				t->sctx_entries[i].backup_attr = 0;
			}
		}
		krc_get(&current_thread->active_sc->refs);
		t->active_sc = current_thread->active_sc;
		spinlock_release_restore(&current_thread->sc_lock);
	} else {
		t->active_sc = secctx_alloc(0);
		t->active_sc->superuser = true; /* we're the init thread */
		krc_get(&t->active_sc->refs);
		t->sctx_entries[0].context = t->active_sc;
	}

	arch_thread_prep_start(
	  t, start, tsa->arg, stack_base, tsa->stack_size, tls_base, tsa->thrd_ctrl);

	t->state = THREADSTATE_RUNNING;
	processor_attach_thread(NULL, t);

#if 0
	printk("spawned thread %ld from %ld on processor %d\n",
	  t->id,
	  current_thread ? (long)current_thread->id : -1,
	  t->processor->id);
#endif
	return 0;
}

long syscall_thrd_ctl(int op, long arg)
{
	if(op <= THRD_CTL_ARCH_MAX) {
		return arch_syscall_thrd_ctl(op, arg);
	}
	int ret = 0;
	switch(op) {
		long *eptr;
		case THRD_CTL_EXIT:
			eptr = (long *)arg;
			if(eptr && verify_user_pointer(eptr, sizeof(void *))) {
				thread_sync_single(THREAD_SYNC_WAKE, eptr, INT_MAX, false);
			}
			thread_exit();
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

#include <slab.h>
static DECLARE_SLABCACHE(_sc_frame,
  sizeof(struct thread_become_frame),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL);

void thread_free_become_frame(struct thread_become_frame *frame)
{
	slabcache_free(&_sc_frame, frame, NULL);
}

static long __syscall_become_return(long a0, long *arg_stack)
{
	struct list *entry = list_pop(&current_thread->become_stack);
	if(!entry) {
		return -EINVAL;
	}

	struct thread_become_frame *frame = list_entry(entry, struct thread_become_frame, entry);

	arch_thread_become_restore(frame, arg_stack);
	if(frame->view) {
		vm_setview(current_thread, frame->view);
		arch_mm_switch_context(current_thread->ctx);
		obj_put(frame->view);
	}

	slabcache_free(&_sc_frame, frame, NULL);
	// kfree(frame);
	return a0;
}

long syscall_become(struct arch_syscall_become_args *_ba,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4)
{
	long extra_args[] = { 4, a1, a2, a3, a4 };
	if(_ba == NULL) {
		return __syscall_become_return(a0, extra_args);
	}
	if(!verify_user_pointer(_ba, sizeof(*_ba))) {
		return -EINVAL;
	}
	struct arch_syscall_become_args ba;
	memcpy(&ba, _ba, sizeof(ba));
	// long _a = rdtsc();
	struct thread_become_frame *oldframe = slabcache_alloc(&_sc_frame, NULL);
	// struct thread_become_frame *oldframe = kalloc(sizeof(struct thread_become_frame));
	oldframe->view = NULL;
	struct object *target_view = NULL;
	// long a = rdtsc();
	//	long a_1, a2, a3;
	if(ba.target_view) {
		oldframe->view = current_thread->ctx->viewobj;
		krc_get(&oldframe->view->refs);
		bool early_find = false;
#if 1
		for(int i = 0; i < MAX_BACK_VIEWS; i++) {
			if(current_thread->backup_views[i].id == ba.target_view) {
				early_find = true;
				target_view = current_thread->backup_views[i].ctx->viewobj;
				panic("A");
				current_thread->ctx = current_thread->backup_views[i].ctx; // TODO: cleanup
				break;
			}
		}
#endif
		if(!early_find) {
			target_view = obj_lookup(ba.target_view, 0);
			if(!target_view) {
				/* TODO: cleanup oldframe */
				//		kfree(oldframe);
				return -ENOENT;
			}

			if(!obj_verify_id(target_view, true, false)) {
				/* TODO: undo things? */
				return -ENOENT;
			}
			//	a_1 = rdtsc();
			//	struct object *obj = kso_get_obj(current_thread->ctx->view, view);
			//	oldframe->view = obj;

			//	a2 = rdtsc();
			vm_setview(current_thread, target_view);

			if(target_view) {
				obj_put(target_view);
			}
		}
		arch_mm_switch_context(current_thread->ctx);
		// a3 = rdtsc();
		// vm_context_fault(ba.target_rip, ba.target_rip, FAULT_ERROR_PRES | FAULT_EXEC);
	}
	// long b = rdtsc();
	arch_thread_become(&ba, oldframe, a1 & SYS_BECOME_INPLACE);
	list_insert(&current_thread->become_stack, &oldframe->entry);
	// long c = rdtsc();
	/* TODO: call check_fault directly, use IP target */
	int r;

#if 0
	/* TODO (sec): use current IP if INPLACE. */
	if((r = obj_check_permission_ip(target_view, SCP_USE, ba.target_rip))) {
		__syscall_become_return(0);
		return r;
	}
#endif

	if((r = secctx_check_permissions_hint(
	      (void *)ba.target_rip, target_view, SCP_USE, ba.sctx_hint))) {
		__syscall_become_return(0, NULL);
		printk("RETURN HERE %d\n", r);
		return r;
	}

	// long d = rdtsc();
	// printk("become: %ld %ld %ld %ld\n", _a - a, b - a, c - b, d - c);
	// printk("    %ld %ld %ld %ld\n", a_1 - a, a2 - a_1, a3 - a2, b - a3);

	return 0;
}

long syscall_signal(uint64_t tidlo, uint64_t tidhi, long arg0, long arg1, long arg2, long arg3)
{
	objid_t tid = MKID(tidhi, tidlo);
	struct object *repr = obj_lookup(tid, 0);

	if(!repr) {
		return -ENOENT;
	}

	if(repr->kso_type != KSO_THREAD) {
		obj_put(repr);
		return -EINVAL;
	}

	struct kso_throbj *thr = object_get_kso_data_checked(repr, KSO_THREAD);
	struct thread *thread = thr->thread;
	struct fault_signal_info info = { { arg0, arg1, arg2, arg3 } };
	/* TODO: locking */
	thread_queue_fault(thread, FAULT_SIGNAL, &info, sizeof(info));
	obj_put(repr);

	return 0;
}
