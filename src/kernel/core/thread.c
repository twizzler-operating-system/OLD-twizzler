#include <kalloc.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <limits.h>
#include <object.h>
#include <processor.h>
#include <slab.h>
#include <thread.h>
static _Atomic unsigned long _internal_tid_counter = 0;

static void _thread_init(void *_u __unused, void *ptr)
{
	struct thread *thr = ptr;
	thr->sc_lock = SPINLOCK_INIT;
	thr->lock = SPINLOCK_INIT;
	list_init(&thr->become_stack);
	thr->sctx_entries = kcalloc(MAX_SC, sizeof(struct thread_sctx_entry), 0);
	arch_thread_init(thr);
}

static void _thread_ctor(void *_data __unused, void *ptr)
{
	struct thread *thr = ptr;
	thr->id = ++_internal_tid_counter;
	thr->state = THREADSTATE_INITING;
	thr->priority = 10;
	assert(list_empty(&thr->become_stack));
	arch_thread_ctor(thr);
}

static void _thread_dtor(void *_data __unused, void *ptr)
{
	struct thread *thr = ptr;
	if(thr->pending_fault_info) {
		kfree(thr->pending_fault_info);
		thr->pending_fault_info = NULL;
	}

	struct list *entry;
	while((entry = list_pop(&thr->become_stack))) {
		struct thread_become_frame *frame = list_entry(entry, struct thread_become_frame, entry);
		if(frame->view) {
			obj_put(frame->view);
		}
		thread_free_become_frame(frame);
	}

	for(int i = 0; i < MAX_SC; i++) {
		/* TODO A: clean up sctx entries */
	}
	/* TODO: remove */
	memset(thr->sctx_entries, 0, sizeof(struct thread_sctx_entry) * MAX_SC);

	for(int i = 0; i < MAX_BACK_VIEWS; i++) {
		if(thr->backup_views[i].ctx && thr->backup_views[i].ctx != thr->ctx) {
			vm_context_free(thr->backup_views[i].ctx);
			thr->backup_views[i].ctx = NULL;
		}
		thr->backup_views[i].id = 0;
	}
	vm_context_free(thr->ctx);
	thr->ctx = NULL;
	thread_sync_uninit_thread(thr);
}

static void _thread_fini(void *_data __unused, void *ptr)
{
	struct thread *thr = ptr;
	kfree(thr->sctx_entries);
	arch_thread_fini(thr);
}

static DECLARE_SLABCACHE(_sc_thread,
  sizeof(struct thread),
  _thread_init,
  _thread_ctor,
  _thread_dtor,
  _thread_fini,
  NULL);

static DECLARE_LIST(allthreads);
static struct spinlock allthreads_lock = SPINLOCK_INIT;

void thread_sleep(struct thread *t, int flags)
{
	(void)flags;
	t->priority *= 20;
	if(t->priority > 1000) {
		t->priority = 1000;
	}
	spinlock_acquire_save(&t->processor->sched_lock);
	if(t->state != THREADSTATE_BLOCKED) {
		t->state = THREADSTATE_BLOCKED;
		list_remove(&t->rq_entry);
		t->processor->stats.running--;
	}
	spinlock_release_restore(&t->processor->sched_lock);

	obj_write_data_atomic64(
	  t->reprobj, offsetof(struct twzthread_repr, syncs[THRD_SYNC_STATE]), THRD_SYNC_STATE_RUNNING);
	thread_wake_object(t->reprobj,
	  offsetof(struct twzthread_repr, syncs[THRD_SYNC_STATE]) + OBJ_NULLPAGE_SIZE,
	  INT_MAX);
}

void thread_wake(struct thread *t)
{
	spinlock_acquire_save(&t->processor->sched_lock);
	int old = atomic_exchange(&t->state, THREADSTATE_RUNNING);
	if(old == THREADSTATE_BLOCKED) {
		obj_write_data_atomic64(t->reprobj,
		  offsetof(struct twzthread_repr, syncs[THRD_SYNC_STATE]),
		  THRD_SYNC_STATE_RUNNING);
		thread_wake_object(t->reprobj,
		  offsetof(struct twzthread_repr, syncs[THRD_SYNC_STATE]) + OBJ_NULLPAGE_SIZE,
		  INT_MAX);

		list_insert(&t->processor->runqueue, &t->rq_entry);
		t->processor->stats.running++;
		t->processor->flags |= PROCESSOR_HASWORK;
		if(t->processor != current_processor) {
			spinlock_release_restore(&t->processor->sched_lock);
			arch_processor_scheduler_wakeup(t->processor);
			return;
		}
	}
	spinlock_release_restore(&t->processor->sched_lock);
}

struct thread *thread_create(void)
{
	struct thread *t = slabcache_alloc(&_sc_thread, NULL);
	spinlock_acquire_save(&allthreads_lock);
	list_insert(&allthreads, &t->all_entry);
	spinlock_release_restore(&allthreads_lock);
	return t;
}

void thread_exit(void)
{
	timer_remove(&current_thread->sleep_timer);
	list_remove(&current_thread->rq_entry);
	current_thread->processor->stats.running--;
	current_thread->state = THREADSTATE_EXITED;
	assert(current_processor->load > 0);
	current_processor->load--;

	spinlock_acquire_save(&allthreads_lock);
	list_remove(&current_thread->all_entry);
	spinlock_release_restore(&allthreads_lock);
	kso_root_detach(current_thread->kso_attachment_num);

	obj_write_data_atomic64(
	  current_thread->reprobj, offsetof(struct twzthread_repr, syncs[THRD_SYNC_EXIT]), 1);
	thread_wake_object(current_thread->reprobj,
	  offsetof(struct twzthread_repr, syncs[THRD_SYNC_EXIT]) + OBJ_NULLPAGE_SIZE,
	  INT_MAX);
	obj_put(current_thread->reprobj);
	current_thread->reprobj = NULL;

	obj_put(current_thread->thrctrl);

	struct thread *thr = current_thread;
	arch_processor_reset_current_thread(current_processor);
	slabcache_free(&_sc_thread, thr, NULL);
}

void thread_print_all_threads(void)
{
	spinlock_acquire_save(&allthreads_lock);
	foreach(e, list, &allthreads) {
		struct thread *t = list_entry(e, struct thread, all_entry);

		spinlock_acquire_save(&t->lock);
		printk("thread %ld\n", t->id);
		printk("  CPU: %d\n", t->processor ? (int)t->processor->id : -1);
		printk("  state: %d\n", t->state);
		printk("  ctx: %p\n", t->ctx);
		arch_thread_print_info(t);
		spinlock_release_restore(&t->lock);
	}
	spinlock_release_restore(&allthreads_lock);
}
