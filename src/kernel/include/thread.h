#pragma once
#include <arch/thread.h>
#include <krc.h>
#include <lib/inthash.h>
#include <lib/list.h>
#include <memory.h>
#include <spinlock.h>
#include <thread-bits.h>
#include <time.h>

#include <twz/sys/fault.h>
#include <twz/sys/thread.h>

struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
	THREADSTATE_EXITED,
	THREADSTATE_INITING,
	THREADSTATE_PAUSING,
};

#define MAX_SC TWZ_THRD_MAX_SCS

struct thread_become_frame {
	struct arch_become_frame frame;
	struct list entry;
	struct object *view;
};

struct thread_list;
struct sleep_entry {
	struct thread_list *tl;
	struct syncpoint *sp;
};

struct thread_sctx_entry {
	struct sctx *context;
	struct sctx *backup;
	uint32_t attr, backup_attr;
};

struct thread_view {
	objid_t id;
	struct vm_context *ctx;
};

/* TODO: maybe a better system for this? */
#define MAX_BACK_VIEWS 8

struct thread {
	struct arch_thread arch;
	struct spinlock lock;
	unsigned long id;
	objid_t thrid;
	_Atomic enum thread_state state;

	uint64_t timeslice_expire;
	int priority;
	struct processor *processor;

	struct vm_context *ctx;
	struct thread_view backup_views[MAX_BACK_VIEWS];

	struct spinlock sc_lock;
	struct sctx *active_sc;
	struct thread_sctx_entry *sctx_entries;

	struct object *thrctrl;
	struct object *reprobj;
	int kso_attachment_num;

	struct list rq_entry, all_entry;
	struct sleep_entry *sleep_entries;
	size_t sleep_count;
	_Atomic bool sleep_restart;

	/* pager info */
	objid_t pager_obj_req;
	ssize_t pager_page_req;

	void *pending_fault_info;
	int pending_fault;
	size_t pending_fault_infolen;

	struct timer sleep_timer;

	struct list become_stack;
};

struct arch_syscall_become_args;
void arch_thread_become(struct arch_syscall_become_args *ba, struct thread_become_frame *, bool);
void arch_thread_become_restore(struct thread_become_frame *frame, long *args);
void thread_sleep(struct thread *t, int flags);
void thread_wake(struct thread *t);
void thread_exit(void);
void thread_raise_fault(struct thread *t, int fault, void *info, size_t);
struct timespec;
long thread_sync_single(int operation, long *addr, long arg, bool);
long thread_wake_object(struct object *obj, size_t offset, long arg);
void thread_sync_uninit_thread(struct thread *thr);
long thread_sleep_on_object(struct object *obj, size_t offset, long arg, bool dont_check);
void thread_print_all_threads(void);
void thread_queue_fault(struct thread *thr, int fault, void *info, size_t infolen);
void arch_thread_raise_call(struct thread *t, void *addr, long a0, void *, size_t);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void);
void arch_thread_prep_start(struct thread *thread,
  void *entry,
  void *arg,
  void *stack,
  size_t stacksz,
  void *tls,
  size_t);
void arch_thread_fini(struct thread *thread);
void arch_thread_init(struct thread *thread);
void arch_thread_ctor(struct thread *thread);

void thread_initialize_processor(struct processor *proc);

long arch_thread_syscall_num(void);
void thread_schedule_resume(void);
void thread_schedule_resume_proc(struct processor *proc);
void arch_thread_resume(struct thread *thread, uint64_t timeout);
uintptr_t arch_thread_instruction_pointer(void);
void thread_free_become_frame(struct thread_become_frame *frame);
void arch_thread_print_info(struct thread *t);
uintptr_t arch_thread_base_pointer(void);
void thread_onresume_clear_other_sleeps(struct thread *);
void thread_do_all_threads(void (*fn)(struct thread *, void *data), void *data);
