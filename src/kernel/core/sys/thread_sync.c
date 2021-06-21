#include <kalloc.h>
#include <lib/inthash.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <slab.h>
#include <syscall.h>

#define MAX_SLEEPS 1024

struct thread_list {
	struct thread *thread;
	struct list entry;
};

static DECLARE_SLABCACHE(sc_tlist, sizeof(struct thread_list), NULL, NULL, NULL, NULL, NULL);

struct syncpoint {
	struct object *obj;
	size_t off;
	int flags;
	struct krc refs;

	struct spinlock lock;
	struct list waiters;
	struct rbnode node;
};

static DECLARE_SLABCACHE(sc_syncpoint, sizeof(struct syncpoint), NULL, NULL, NULL, NULL, NULL);

static int __sp_compar_key(struct syncpoint *a, size_t n)
{
	if(a->off > n)
		return 1;
	else if(a->off < n)
		return -1;
	return 0;
}

static int __sp_compar(struct syncpoint *a, struct syncpoint *b)
{
	return __sp_compar_key(a, b->off);
}

/* TODO: do we need to make sure that this is enough syncronization. Do we need
 * to lock the relevant pagecache pages? */

/* TODO: determine if timeout occurred */

static struct syncpoint *sp_lookup(struct object *obj, size_t off, bool create)
{
	spinlock_acquire_save(&obj->tslock);
	struct rbnode *node =
	  rb_search(&obj->tstable_root, off, struct syncpoint, node, __sp_compar_key);
	struct syncpoint *sp = node ? rb_entry(node, struct syncpoint, node) : NULL;
	if(!sp || !krc_get_unless_zero(&sp->refs)) {
		if(!create) {
			spinlock_release_restore(&obj->tslock);
			return NULL;
		}
		sp = slabcache_alloc(&sc_syncpoint, NULL);
		krc_get(&obj->refs);
		sp->obj = obj;
		sp->off = off;
		sp->flags = 0;
		krc_init(&sp->refs);
		list_init(&sp->waiters);
		rb_insert(&obj->tstable_root, sp, struct syncpoint, node, __sp_compar);
	}
	spinlock_release_restore(&obj->tslock);
	return sp;
}

static void _sp_release(void *_sp)
{
	struct syncpoint *sp = _sp;
	spinlock_acquire_save(&sp->obj->tslock);
	rb_delete(&sp->node, &sp->obj->tstable_root);
	spinlock_release_restore(&sp->obj->tslock);
	struct object *obj = sp->obj;
	sp->obj = NULL;
	obj_put(obj);
	assert(list_empty(&sp->waiters));
	slabcache_free(&sc_syncpoint, sp, NULL);
}

#define SLEEP_32BIT 1
#define SLEEP_DONTCHECK 2

static struct thread_list *sp_sleep_prep(struct syncpoint *sp,
  long *addr,
  long val,
  int flags,
  int *result)
{
	struct thread_list *tl = slabcache_alloc(&sc_tlist, NULL);
	spinlock_acquire_save(&sp->lock);
	thread_sleep(current_thread, 0);

	tl->thread = current_thread;
	list_insert(&sp->waiters, &tl->entry);

	/* TODO: verify that addr is a valid address that we can access */
	int r = (flags & SLEEP_DONTCHECK)
	        || ((flags & SLEEP_32BIT) ? (atomic_load((_Atomic int *)addr) == (int)val)
	                                  : (atomic_load((_Atomic long *)addr) == val));
	spinlock_release_restore(&sp->lock);
	*result = r;
	return tl;
}

static int sp_sleep(struct syncpoint *sp, long *addr, long val, int idx, int flags)
{
	int r;
	struct thread_list *tl = sp_sleep_prep(sp, addr, val, flags, &r);
	if(!r) {
		spinlock_acquire_save(&sp->lock);
		list_remove(&tl->entry);
		thread_wake(current_thread);
		spinlock_release_restore(&sp->lock);
		krc_put_call(sp, refs, _sp_release);
	} else {
		current_thread->sleep_entries[idx].tl = tl;
		current_thread->sleep_entries[idx].sp = sp;
	}
	return 0;
}

static int sp_wake(struct syncpoint *sp, long arg)
{
	if(!sp) {
		return 0;
	}
	spinlock_acquire_save(&sp->lock);
	struct list *next;
	int count = 0;
	for(struct list *e = list_iter_start(&sp->waiters); e != list_iter_end(&sp->waiters);
	    e = next) {
		next = list_iter_next(e);
		struct thread_list *tl = list_entry(e, struct thread_list, entry);
		if(arg == 0)
			break;
		else if(arg > 0)
			arg--;

		tl->thread->sleep_restart = true;
		thread_wake(tl->thread);
		count++;
	}
	spinlock_release_restore(&sp->lock);

	return count;
}

void thread_onresume_clear_other_sleeps(struct thread *thr)
{
	/* TODO: optimization to try to elide this? */
	for(size_t i = 0; i < thr->sleep_count; i++) {
		struct syncpoint *op = thr->sleep_entries[i].sp;
		struct thread_list *tl = thr->sleep_entries[i].tl;
		if(op) {
			spinlock_acquire_save(&op->lock);
			list_remove(&tl->entry);
			spinlock_release_restore(&op->lock);
			krc_put_call(op, refs, _sp_release);
			thr->sleep_entries[i].sp = NULL;
		}
	}
}

void thread_sync_uninit_thread(struct thread *thr)
{
	for(size_t i = 0; i < thr->sleep_count; i++) {
		struct syncpoint *op = thr->sleep_entries[i].sp;
		struct thread_list *tl = thr->sleep_entries[i].tl;
		if(op) {
			spinlock_acquire_save(&op->lock);
			list_remove(&tl->entry);
			spinlock_release_restore(&op->lock);
			krc_put_call(op, refs, _sp_release);
			thr->sleep_entries[i].sp = NULL;
		}
	}

	if(thr->sleep_entries) {
		kfree(thr->sleep_entries);
		thr->sleep_entries = NULL;
	}
	thr->sleep_count = 0;
}

static long thread_sync_single_norestore(int operation, long *addr, long arg, int idx, int flags)
{
	objid_t id;
	uint64_t off;
	// long long a, b, c, d, e, f;
	// a = krdtsc();
	struct object *obj = vm_vaddr_lookup_obj(addr, &off);
	if(!obj) {
		return -EFAULT;
	}
	// c = krdtsc();
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	// d = krdtsc();
	long ret = -EINVAL;
	switch(operation) {
		int result;
		struct thread_list *tl;
		case THREAD_SYNC_SLEEP:
			tl = sp_sleep_prep(sp, addr, arg, flags, &result);
			ret = result;
			current_thread->sleep_entries[idx].sp = sp;
			current_thread->sleep_entries[idx].tl = tl;
			break;
		case THREAD_SYNC_WAKE:
			ret = sp_wake(sp, arg);
			if(sp)
				krc_put_call(sp, refs, _sp_release);
			break;
		default:
			break;
	}
	// e = krdtsc();
	obj_put(obj);
	// f = krdtsc();
	// if(operation == THREAD_SYNC_WAKE)
	//	printk("tssnr: %ld %ld %ld %ld %ld -> %ld\n", b - a, c - b, d - c, e - d, f - e, ret);
	return ret;
}

long thread_wake_object(struct object *obj, size_t offset, long arg)
{
	struct syncpoint *sp = sp_lookup(obj, offset, false);
	if(!sp)
		return 0;
	long c = sp_wake(sp, arg);
	krc_put_call(sp, refs, _sp_release);
	return c;
}

static void __thread_init_sync(size_t count)
{
	if(!current_thread->sleep_entries) {
		current_thread->sleep_entries = kcalloc(count, sizeof(struct sleep_entry), 0);
		current_thread->sleep_count = count;
	}
	if(count > current_thread->sleep_count) {
		current_thread->sleep_entries =
		  krecalloc(current_thread->sleep_entries, count, sizeof(struct sleep_entry), 0);
		current_thread->sleep_count = count;
	}
}

long thread_sleep_on_object(struct object *obj, size_t offset, long arg, bool dont_check)
{
	struct syncpoint *sp = sp_lookup(obj, offset, true);
	__thread_init_sync(1);
	spinlock_acquire_save(&current_thread->lock);
	if(current_thread->sleep_entries[0].sp) {
		spinlock_release_restore(&current_thread->lock);
		return 0;
	}
	spinlock_release_restore(&current_thread->lock);
	if(!dont_check) {
		panic("NI - in-kernel sleep on object with addr check");
	}
	return sp_sleep(sp, NULL, arg, 0, SLEEP_DONTCHECK);
}

long thread_sync_single(int operation, long *addr, long arg, bool bits32)
{
	uint64_t off;
	struct object *obj = vm_vaddr_lookup_obj(addr, &off);
	if(!obj) {
		return -EFAULT;
	}
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	obj_put(obj);
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			__thread_init_sync(1);
			return sp_sleep(sp, addr, arg, 0, bits32 ? SLEEP_32BIT : 0);
		case THREAD_SYNC_WAKE:
			return sp_wake(sp, arg);
		default:
			break;
	}
	return -EINVAL;
}

static void __thread_sync_timer(void *a)
{
	struct thread *thr = a;
	thread_wake(thr);
}

long syscall_thread_sync(size_t count, struct sys_thread_sync_args *args, struct timespec *timeout)
{
	bool ok = false;
	bool wake = false;
	bool was_wake_op = false;
	if(count > MAX_SLEEPS)
		return -EINVAL;
	if(!verify_user_pointer(args, count * sizeof(*args)))
		return -EINVAL;
	if(timeout && !verify_user_pointer(timeout, sizeof(*timeout)))
		return -EINVAL;
	__thread_init_sync(count);
	/* sleep restart is used to handle the race condition of a thread sleeping on several words, and
	 * being woken up by one after it sleeps on that one, but before sleeping on the next one. When
	 * waking a thread, we set this boolean to true, so that when the thread is done sleeping it can
	 * check to see if something woke it up.
	 *
	 * This has the potential, I think, to cause spurious wakeup, but the interface for this syscall
	 * allows for that. It also does mean some wasted work in the case where this happens, but it's
	 * pretty rare, I think. */
	current_thread->sleep_restart = false;
	bool armed_sleep = false;
	int ret = 0;
	for(size_t i = 0; i < count; i++) {
		if(args[i].op == THREAD_SYNC_SLEEP && timeout && !armed_sleep) {
			armed_sleep = true;
			uint64_t timeout_nsec = timeout->tv_nsec + timeout->tv_sec * 1000000000ul;
			timer_add(
			  &current_thread->sleep_timer, timeout_nsec, __thread_sync_timer, current_thread);
		}
		void *addr = args[i].addr;
		int r;
		if(!verify_user_pointer(addr, sizeof(void *))) {
			r = ret = -EINVAL;
		} else {
			if(args[i].op == THREAD_SYNC_WAKE) {
				was_wake_op = true;
			}
			r = thread_sync_single_norestore(args[i].op,
			  addr,
			  args[i].arg,
			  i,
			  (args[i].flags & THREAD_SYNC_32BIT) ? SLEEP_32BIT : 0);
			if(r)
				ret = r;
		}
		ok = ok || r >= 0;
		args[i].res = r >= 0 ? 1 : r;
		if(args[i].op == THREAD_SYNC_SLEEP && r == 0) {
			wake = true;
		}
	}
	if(wake || current_thread->sleep_restart) {
		thread_wake(current_thread);
		timer_remove(&current_thread->sleep_timer);
	}
	return ok ? 0 : ret;
}
