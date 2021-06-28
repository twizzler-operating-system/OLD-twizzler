#include <objspace.h>
#include <page.h>
#include <pager.h>
#include <processor.h>
#include <rwlock.h>
#include <secctx.h>
#include <slots.h>
#include <thread.h>
#include <tmpmap.h>
#include <twz/meta.h>
#include <vmm.h>
int obj_check_permission_ip(struct object *obj, uint64_t flags, uint64_t ip)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	/*
	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
	    return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
	    //	return 0;
	}
	*/
	if(!current_thread) {
		return 0;
	}
	int r = secctx_check_permissions((void *)ip, obj, flags);
	return r;
}

int obj_check_permission(struct object *obj, uint64_t flags)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	/*
	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
	    return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
	    //	return 0;
	}
	*/
	if(!current_thread) {
		return 0;
	}
	int r = secctx_check_permissions((void *)arch_thread_instruction_pointer(), obj, flags);
	return r;
}

#include <twz/sys/sctx.h>
static uint32_t __conv_objperm_to_scp(uint64_t p)
{
	uint32_t perms = 0;
	if(p & OBJSPACE_READ) {
		perms |= SCP_READ;
	}
	if(p & OBJSPACE_WRITE) {
		perms |= SCP_WRITE;
	}
	if(p & OBJSPACE_EXEC_U) {
		perms |= SCP_EXEC;
	}
	return perms;
}

static uint64_t __conv_scp_to_objperm(uint32_t p)
{
	uint64_t perms = 0;
	if(p & SCP_READ) {
		perms |= OBJSPACE_READ;
	}
	if(p & SCP_WRITE) {
		perms |= OBJSPACE_WRITE;
	}
	if(p & SCP_EXEC) {
		perms |= OBJSPACE_EXEC_U;
	}
	return perms;
}

static bool __objspace_fault_calculate_perms(struct object *o,
  uint32_t flags,
  uintptr_t loaddr,
  uintptr_t vaddr,
  uintptr_t ip,
  uint64_t *perms)
{
	/* optimization: just check if default permissions are enough */
#if 0
	uint32_t p_flags;
	if(!obj_get_pflags(o, &p_flags)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));

		return false;
	}
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
	bool ok = true;
	if(flags & OBJSPACE_FAULT_READ) {
		ok = ok && (dfl & MIP_DFL_READ);
	}
	if(flags & OBJSPACE_FAULT_WRITE) {
		ok = ok && (dfl & MIP_DFL_WRITE);
	}
	if(flags & OBJSPACE_FAULT_EXEC) {
		ok = ok && (dfl & MIP_DFL_EXEC);
	}
	if(dfl & MIP_DFL_READ)
		*perms |= OBJSPACE_READ;
	if(dfl & MIP_DFL_WRITE)
		*perms |= OBJSPACE_WRITE;
	if(dfl & MIP_DFL_EXEC)
		*perms |= OBJSPACE_EXEC_U;
#else
	bool ok = false;
#endif
	if(!ok) {
		*perms = 0;
		uint32_t res;
		if(secctx_fault_resolve(
		     (void *)ip, loaddr, (void *)vaddr, o, __conv_objperm_to_scp(flags), &res, true, 0)
		   == -1) {
			return false;
		}
		*perms = __conv_scp_to_objperm(res);
	}

	bool w = (*perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		return false;
	}

	if(((*perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	return true;
}

void object_map_page(struct object *obj, size_t pagenr, struct page *page, uint64_t flags)
{
	struct omap *omap = mm_objspace_get_object_map(obj, pagenr);
	assert(omap);
	arch_objspace_region_map(
	  current_thread->active_sc->space, omap->region, flags & (MAP_READ | MAP_WRITE | MAP_EXEC));
	if(arch_objspace_region_map_page(
	     omap->region, pagenr % (mm_objspace_region_size() / mm_page_size(0)), page, flags)) {
		arch_mm_objspace_invalidate(NULL,
		  omap->region->addr + (pagenr % (mm_objspace_region_size() / mm_page_size(0))),
		  mm_page_size(0),
		  0);
	}
	/* TODO: would like a better system for this */
	assert(omap->refs > 1);
	omap->refs--;
}

static void __op_fault_callback(struct object *obj,
  size_t pagenr,
  struct page *page,
  void *data __unused,
  uint64_t cbfl)
{
	uint64_t mapflags = MAP_READ | MAP_WRITE | MAP_EXEC;
	// printk("cb: %ld %lx :: %lx\n", pagenr, cbfl, page->addr);
	if(cbfl & PAGE_MAP_COW)
		mapflags |= PAGE_MAP_COW;
	object_map_page(obj, pagenr, page, mapflags);
}

static struct object *fault_get_object(uintptr_t vaddr)
{
	return vm_context_lookup_object(current_thread->ctx, vaddr);
}

#include <thread.h>
void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
#if 0
	printk("objspace fault entry th %ld: %lx %lx %lx %x\n",
	  current_thread ? current_thread->id : -1,
	  ip,
	  loaddr,
	  vaddr,
	  flags);

	if(current_thread) {
		// printk("::: %p\n",
		// current_thread->active_sc->space->arch.root.children[PML4_IDX(loaddr)]);
	}
#endif

	if(vaddr >= KERNEL_REGION_START) {
		panic("object space fault to kernel memory (oaddr=%lx, vaddr=%lx, flags=%x, ip=%lx)",
		  loaddr,
		  vaddr,
		  flags,
		  ip);
	}

	struct object *obj = fault_get_object(vaddr);
	if(!obj) {
		panic("A :userspace fault to object not mapped");
	}
#if 0
	printk("%ld :: %x fault object: " IDFMT " (%d)\n",
	  current_thread->id,
	  flags,
	  IDPR(obj->id),
	  obj->kso_type);
#endif

	size_t pagenr = (vaddr % OBJ_MAXSIZE) / mm_page_size(0);
	if(pagenr == 0) {
		struct fault_null_info info = twz_fault_build_null_info((void *)ip, (void *)vaddr);
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		obj_put(obj);
		return;
	}

	int opflags = 0;
	if(flags & OBJSPACE_FAULT_WRITE) {
		opflags |= OP_LP_DO_COPY;
	}
	object_operate_on_locked_page(obj, pagenr, opflags, __op_fault_callback, NULL);
	obj_put(obj);
}

void object_insert_page(struct object *obj, size_t pagenr, struct page *page)
{
	struct rwlock_result rwres = rwlock_wlock(&obj->rwlock, 0);
	struct range *range = object_find_range(obj, pagenr);
	bool new_range = false;
	if(!range) {
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
	}

	size_t pvidx = range_pv_idx(range, pagenr);

	if(pvidx > PAGEVEC_MAX_IDX) {
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
	}

	pagevec_set_page(range->pv, pvidx, page);

	rwlock_wunlock(&rwres);
}

int object_operate_on_locked_page(struct object *obj,
  size_t pagenr,
  int flags,
  void (*fn)(struct object *obj, size_t pagenr, struct page *page, void *data, uint64_t cb_fl),
  void *data)
{
	uint64_t cb_fl = 0;
	struct rwlock_result rwres = rwlock_rlock(&obj->rwlock, 0);
	struct range *range = object_find_range(obj, pagenr);
	if(!range) {
		if(flags & OP_LP_ZERO_OK) {
			fn(obj, pagenr, NULL, data, cb_fl);
			rwlock_runlock(&rwres);
			return 0;
		}
		rwres = rwlock_upgrade(&rwres, 0);
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
		rwres = rwlock_downgrade(&rwres);
	}

	size_t pvidx = range_pv_idx(range, pagenr);

	struct page *page;
	struct pagevec *locked_pv = range->pv;
	pagevec_lock(locked_pv);
	int ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);

	if(ret == GET_PAGE_BLOCK) {
		printk("TODO: A implement this\n");
		/* TODO: return a "def resched" thing */
	} else {
		if(range->pv->refs > 1) {
			if(flags & OP_LP_DO_COPY) {
				rwres = rwlock_upgrade(&rwres, 0);
				range = range_split(range, pagenr - range->start);
				range_clone(range);
				pagevec_unlock(locked_pv);
				locked_pv = range->pv;
				pagevec_lock(locked_pv);
				rwres = rwlock_downgrade(&rwres);

				pvidx = range_pv_idx(range, pagenr);
				ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);
				assert(ret == 0);
			} else {
				cb_fl |= PAGE_MAP_COW;
			}
		}
		fn(obj, pagenr, page, data, cb_fl);
	}
	pagevec_unlock(range->pv);

	rwlock_runlock(&rwres);

	return 0;
}
